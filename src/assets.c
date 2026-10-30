#include "dpaint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int read_file(const char *path, uint8_t **out, int *outlen)
{
    FILE *fp = fopen(path, "rb");
    long sz;
    uint8_t *buf;
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 16 || sz > 16 * 1024 * 1024) {
        fclose(fp);
        return -1;
    }
    rewind(fp);
    buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL || fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *out = buf;
    *outlen = (int)sz;
    return 0;
}

/* Pull the three loadable hunks out of the DPaint executable. */
static int split_hunks(const uint8_t *file, int flen,
                       uint8_t **h1, int *h1len, uint8_t **h2, int *h2len)
{
    int off = 4;
    uint32_t nlibs, first, last, n, i;
    if (flen < 32 || rd32(file) != 0x000003F3u) {
        return -1;
    }
    nlibs = rd32(file + off);
    off += 4;
    for (i = 0; i < nlibs; i++) {
        n = rd32(file + off);
        off += 4 + (int)n * 4;
    }
    off += 4;
    first = rd32(file + off);
    off += 4;
    last = rd32(file + off);
    off += 4;
    if (last < first) {
        return -1;
    }
    off += (int)(last - first + 1) * 4;
    for (i = first; i <= last; i++) {
        uint32_t typ, nbytes;
        if (off + 8 > flen) {
            return -1;
        }
        typ = rd32(file + off) & 0x3FFFFFFFu;
        off += 4;
        nbytes = (rd32(file + off) & 0x3FFFFFFFu) * 4u;
        off += 4;
        if (typ != 0x3E9u && typ != 0x3EAu) {
            return -1;
        }
        if (off + (int)nbytes > flen) {
            return -1;
        }
        if (i == first + 1) {
            *h1 = (uint8_t *)malloc(nbytes);
            if (*h1 == NULL) {
                return -1;
            }
            memcpy(*h1, file + off, nbytes);
            *h1len = (int)nbytes;
        } else if (i == first + 2) {
            *h2 = (uint8_t *)malloc(nbytes);
            if (*h2 == NULL) {
                return -1;
            }
            memcpy(*h2, file + off, nbytes);
            *h2len = (int)nbytes;
        }
        off += (int)nbytes;
        while (off + 4 <= flen) {
            uint32_t t = rd32(file + off) & 0x3FFFFFFFu;
            off += 4;
            if (t == 0x3ECu) {
                for (;;) {
                    uint32_t c, hn;
                    if (off + 4 > flen) {
                        return -1;
                    }
                    c = rd32(file + off);
                    off += 4;
                    if (c == 0) {
                        break;
                    }
                    if (off + 4 > flen) {
                        return -1;
                    }
                    hn = rd32(file + off);
                    off += 4;
                    (void)hn;
                    off += (int)c * 4;
                }
            } else if (t == 0x3F2u) {
                break;
            } else {
                return -1;
            }
        }
    }
    return (*h1 && *h2) ? 0 : -1;
}

static void load_icons(DpAssets *a, const uint8_t *h1, int h1len,
                       const uint8_t *h2, int h2len)
{
    static const uint8_t sig[] = {
        0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0x00, 0xea, 0x00, 0x00, 0x01, 0x00
    };
    int i, n;
    const uint8_t *hit = NULL;
    for (i = 0; i + 12 < h2len; i++) {
        if (memcmp(h2 + i, sig, 12) == 0) {
            hit = h2 + i;
            break;
        }
    }
    if (hit == NULL) {
        return;
    }
    for (n = 0; n < DP_NICONS; n++) {
        uint32_t off;
        int y, x;
        if ((hit - h2) + (n + 1) * 4 > h2len) {
            break;
        }
        off = rd32(hit + n * 4);
        if ((int)off + 22 > h1len) {
            break;
        }
        for (y = 0; y < 11; y++) {
            uint16_t row = rd16(h1 + off + (uint32_t)y * 2u);
            for (x = 0; x < 16; x++) {
                a->icon[n][y][x] = (row & (0x8000u >> x)) ? 1 : 0;
            }
        }
    }
    a->nicons = n;
}

static void load_palette(DpAssets *a, const uint8_t *h1, int h1len)
{
    static const uint16_t ramp[] = {
        0x0333, 0x0444, 0x0555, 0x0666, 0x0777, 0x0888,
        0x0999, 0x0aaa, 0x0ccc, 0x0ddd, 0x0eee, 0x0fff
    };
    int i;
    for (i = 40; i + 64 < h1len; i += 2) {
        int k;
        int ok = 1;
        for (k = 0; k < 12; k++) {
            if (rd16(h1 + i + k * 2) != ramp[k]) {
                ok = 0;
                break;
            }
        }
        if (!ok) {
            continue;
        }
        /* The gray ramp is colors 20..31 of the default palette. */
        for (k = 0; k < DP_MAX_COLORS; k++) {
            uint16_t c = rd16(h1 + i - 40 + k * 2);
            int r = (c >> 8) & 15;
            int g = (c >> 4) & 15;
            int b = c & 15;
            if (c & 0xF000) {
                ok = 0;
                break;
            }
            a->pal[k].r = (uint8_t)(r * 17);
            a->pal[k].g = (uint8_t)(g * 17);
            a->pal[k].b = (uint8_t)(b * 17);
        }
        if (ok && a->pal[0].r == 0 && a->pal[31].r == 255) {
            a->pal_ok = 1;
            return;
        }
    }
}

static int join(char *dst, int dstlen, const char *root, const char *rel)
{
    int n = snprintf(dst, (size_t)dstlen, "%s/%s", root, rel);
    return (n > 0 && n < dstlen) ? 0 : -1;
}

int dp_assets_load(DpAssets *a, const char *root)
{
    char path[640];
    uint8_t *file = NULL, *h1 = NULL, *h2 = NULL;
    int flen = 0, h1len = 0, h2len = 0;
    static const char *faces[] = {
        "fonts/DPaint/8",
        "fonts/DPaint/5",
        "fonts/Coop/24",
        "fonts/Coop/88",
        "fonts/mChelt/16",
        "fonts/mChelt/48",
        "fonts/TheFont/24u",
        NULL
    };
    static const char *face_name[] = {
        "DPaint 8",
        "DPaint 5",
        "Coop 24",
        "Coop 88",
        "mChelt 16",
        "mChelt 48",
        "TheFont 24"
    };
    int i;

    memset(a, 0, sizeof(*a));
    if (root == NULL || root[0] == '\0') {
        root = "amiga_original/unpacked";
    }
    snprintf(a->root, sizeof(a->root), "%s", root);
    for (i = 0; faces[i] != NULL && a->nfonts < DP_MAX_FONTS; i++) {
        if (join(path, (int)sizeof(path), root, faces[i]) != 0) {
            continue;
        }
        if (dp_font_load(&a->fonts[a->nfonts], path) == 0) {
            snprintf(a->fonts[a->nfonts].face, sizeof(a->fonts[a->nfonts].face),
                     "%s", face_name[i]);
            a->nfonts++;
        }
    }
    if (a->nfonts < 1) {
        return -1;
    }
    if (join(path, (int)sizeof(path), root, "DPaint") == 0 &&
        read_file(path, &file, &flen) == 0) {
        if (split_hunks(file, flen, &h1, &h1len, &h2, &h2len) == 0) {
            load_icons(a, h1, h1len, h2, h2len);
            load_palette(a, h2, h2len);
            if (!a->pal_ok) {
                load_palette(a, h1, h1len);
            }
        }
    }
    free(file);
    free(h1);
    free(h2);
    if (!a->pal_ok) {
        for (i = 0; i < DP_MAX_COLORS; i++) {
            uint8_t v = (uint8_t)(i * 255 / 31);
            a->pal[i].r = v;
            a->pal[i].g = v;
            a->pal[i].b = v;
        }
    }
    return 0;
}

void dp_assets_free(DpAssets *a)
{
    int i;
    for (i = 0; i < a->nfonts; i++) {
        dp_font_free(&a->fonts[i]);
    }
    memset(a, 0, sizeof(*a));
}

const DpFont *dp_font(const DpAssets *a)
{
    if (a->nfonts < 1) {
        return NULL;
    }
    if (a->font_i < 0 || a->font_i >= a->nfonts) {
        return &a->fonts[0];
    }
    return &a->fonts[a->font_i];
}
