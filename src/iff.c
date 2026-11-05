#include "dpaint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void w16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void w32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t r16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t r32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void set_err(char *err, int errlen, const char *s)
{
    if (err != NULL && errlen > 0) {
        snprintf(err, (size_t)errlen, "%s", s);
    }
}

static int rowbytes_of(int w)
{
    return ((w + 15) / 16) * 2;
}

static int pack_row(const uint8_t *s, int n, uint8_t *d)
{
    int i = 0, o = 0;
    while (i < n) {
        if (i + 1 < n && s[i] == s[i + 1]) {
            int j = i + 1;
            while (j < n && s[j] == s[i] && (j - i) < 128) {
                j++;
            }
            d[o++] = (uint8_t)(1 - (j - i));
            d[o++] = s[i];
            i = j;
        } else {
            int start = i;
            i++;
            while (i < n && (i - start) < 128) {
                if (i + 1 < n && s[i] == s[i + 1]) {
                    break;
                }
                i++;
            }
            d[o++] = (uint8_t)((i - start) - 1);
            memcpy(d + o, s + start, (size_t)(i - start));
            o += i - start;
        }
    }
    return o;
}

static int unpack_row(const uint8_t *s, int slen, uint8_t *d, int n, int *used)
{
    int i = 0, o = 0;
    while (o < n) {
        int c;
        if (i >= slen) {
            return -1;
        }
        c = (int8_t)s[i++];
        if (c == -128) {
            continue;
        }
        if (c >= 0) {
            int len = c + 1;
            if (i + len > slen || o + len > n) {
                return -1;
            }
            memcpy(d + o, s + i, (size_t)len);
            i += len;
            o += len;
        } else {
            int len = 1 - c;
            if (i >= slen || o + len > n) {
                return -1;
            }
            memset(d + o, s[i++], (size_t)len);
            o += len;
        }
    }
    if (used) {
        *used = i;
    }
    return 0;
}

static void planes_from_pix(const uint8_t *pix, int w, int h, int planes,
                            uint8_t *out)
{
    int rb = rowbytes_of(w);
    int p, y, x;
    memset(out, 0, (size_t)planes * (size_t)rb * (size_t)h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint8_t c = pix[y * w + x];
            int byte = x >> 3;
            int bit = 7 - (x & 7);
            for (p = 0; p < planes; p++) {
                if (c & (1u << p)) {
                    out[(p * h + y) * rb + byte] |= (uint8_t)(1u << bit);
                }
            }
        }
    }
}

static void pix_from_planes(uint8_t *pix, int w, int h, int planes,
                            const uint8_t *in)
{
    int rb = rowbytes_of(w);
    int y, x, p;
    memset(pix, 0, (size_t)w * (size_t)h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int byte = x >> 3;
            int bit = 7 - (x & 7);
            uint8_t c = 0;
            for (p = 0; p < planes; p++) {
                uint8_t v = in[(p * h + y) * rb + byte];
                if (v & (1u << bit)) {
                    c |= (uint8_t)(1u << p);
                }
            }
            pix[y * w + x] = c;
        }
    }
}

typedef struct {
    uint8_t *p;
    int n, cap;
} Buf;

static int buf_need(Buf *b, int extra)
{
    if (b->n + extra <= b->cap) {
        return 0;
    }
    while (b->n + extra > b->cap) {
        b->cap = b->cap < 1024 ? 1024 : b->cap * 2;
    }
    b->p = (uint8_t *)realloc(b->p, (size_t)b->cap);
    return b->p == NULL ? -1 : 0;
}

static int buf_bytes(Buf *b, const void *s, int n)
{
    if (buf_need(b, n) != 0) {
        return -1;
    }
    memcpy(b->p + b->n, s, (size_t)n);
    b->n += n;
    return 0;
}

static int buf_chunk(Buf *b, const char id[4], const void *data, int n)
{
    uint8_t hdr[8];
    memcpy(hdr, id, 4);
    w32(hdr + 4, (uint32_t)n);
    if (buf_bytes(b, hdr, 8) != 0) {
        return -1;
    }
    if (n > 0 && buf_bytes(b, data, n) != 0) {
        return -1;
    }
    if (n & 1) {
        uint8_t z = 0;
        if (buf_bytes(b, &z, 1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int planes_for(int ncolors)
{
    int p = 1;
    int n = 2;
    while (n < ncolors && p < 6) {
        p++;
        n *= 2;
    }
    return p;
}

static int pix_max(const uint8_t *p, int n)
{
    int i, m = 0;
    if (p == NULL || n < 1) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (p[i] > m) {
            m = p[i];
        }
    }
    return m;
}

/* 5 planes and 32 colors unless a pixel uses a slot above 31. */
static int save_ncolors(const DpFrame *frames, int nframes, int ncolors, int slot)
{
    int i, m = 0;
    for (i = 0; i < nframes; i++) {
        int k;
        if (frames[i].pix == NULL) {
            continue;
        }
        k = pix_max(frames[i].pix, frames[i].w * frames[i].h);
        if (k > m) {
            m = k;
        }
    }
    if (slot > m) {
        m = slot;
    }
    if (m > 31) {
        if (ncolors < 64) {
            ncolors = 64;
        }
        if (ncolors > DP_MAX_COLORS) {
            ncolors = DP_MAX_COLORS;
        }
        return ncolors;
    }
    if (ncolors > 32) {
        return 32;
    }
    return ncolors;
}

static int build_body(const DpFrame *f, int planes, int compression, uint8_t **out, int *outlen)
{
    int rb = rowbytes_of(f->w);
    int plane_bytes = rb * f->h;
    uint8_t *plbuf = (uint8_t *)malloc((size_t)planes * (size_t)plane_bytes);
    uint8_t *body;
    int y, p, o = 0;
    int cap;
    if (plbuf == NULL) {
        return -1;
    }
    planes_from_pix(f->pix, f->w, f->h, planes, plbuf);
    if (compression == 0) {
        *out = plbuf;
        *outlen = planes * plane_bytes;
        return 0;
    }
    cap = planes * plane_bytes * 2 + 64;
    body = (uint8_t *)malloc((size_t)cap);
    if (body == NULL) {
        free(plbuf);
        return -1;
    }
    for (y = 0; y < f->h; y++) {
        for (p = 0; p < planes; p++) {
            const uint8_t *row = plbuf + (p * f->h + y) * rb;
            int n = pack_row(row, rb, body + o);
            o += n;
        }
    }
    free(plbuf);
    *out = body;
    *outlen = o;
    return 0;
}

static void bmhd_pack(uint8_t bm[20], int w, int h, int planes, int compression,
                      int trans, int pagew, int pageh)
{
    memset(bm, 0, 20);
    w16(bm + 0, (uint16_t)w);
    w16(bm + 2, (uint16_t)h);
    bm[8] = (uint8_t)planes;
    bm[9] = 2;
    bm[10] = (uint8_t)compression;
    w16(bm + 12, (uint16_t)trans);
    bm[14] = 10;
    bm[15] = 11;
    w16(bm + 16, (uint16_t)pagew);
    w16(bm + 18, (uint16_t)pageh);
}

static int cmap_pack(uint8_t *dst, const DpColor *pal, int ncolors)
{
    int i;
    for (i = 0; i < ncolors; i++) {
        dst[i * 3] = pal[i].r;
        dst[i * 3 + 1] = pal[i].g;
        dst[i * 3 + 2] = pal[i].b;
    }
    return ncolors * 3;
}

static int crng_pack(uint8_t *dst, const DpRange *ranges, int nranges)
{
    int i, o = 0;
    for (i = 0; i < nranges && i < DP_MAX_RANGES; i++) {
        w16(dst + o, 0);
        w16(dst + o + 2, (uint16_t)ranges[i].rate);
        w16(dst + o + 4, (uint16_t)(ranges[i].active ? 1 : 0));
        dst[o + 6] = (uint8_t)ranges[i].low;
        dst[o + 7] = (uint8_t)ranges[i].high;
        o += 8;
    }
    return o;
}

static int write_file(const char *path, const uint8_t *p, int n)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fwrite(p, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int form_wrap(const char type[4], Buf *inner, uint8_t **out, int *outlen)
{
    uint8_t *p = (uint8_t *)malloc((size_t)inner->n + 12);
    if (p == NULL) {
        return -1;
    }
    memcpy(p, "FORM", 4);
    w32(p + 4, (uint32_t)inner->n + 4);
    memcpy(p + 8, type, 4);
    memcpy(p + 12, inner->p, (size_t)inner->n);
    *out = p;
    *outlen = inner->n + 12;
    return 0;
}

int iff_save_ilbm(const char *path, const DpFrame *f,
                  const DpColor *pal, int ncolors, int camg,
                  const DpRange *ranges, int nranges,
                  int grab, int gx, int gy, int trans, int compression,
                  int slot)
{
    int planes;
    uint8_t bm[20], cm[DP_MAX_COLORS * 3], cr[DP_MAX_RANGES * 8];
    uint8_t cam[4], gr[4];
    uint8_t *body = NULL;
    int bodylen = 0, crlen;
    Buf b;
    uint8_t *file = NULL;
    int flen = 0;
    int rc;
    memset(&b, 0, sizeof(b));
    ncolors = save_ncolors(f, 1, ncolors, slot);
    planes = planes_for(ncolors);
    if (build_body(f, planes, compression, &body, &bodylen) != 0) {
        return -1;
    }
    bmhd_pack(bm, f->w, f->h, planes, compression, trans, f->w, f->h);
    cmap_pack(cm, pal, ncolors);
    w32(cam, (uint32_t)camg);
    if (buf_chunk(&b, "BMHD", bm, 20) != 0 ||
        buf_chunk(&b, "CMAP", cm, ncolors * 3) != 0 ||
        buf_chunk(&b, "CAMG", cam, 4) != 0) {
        free(body);
        free(b.p);
        return -1;
    }
    crlen = crng_pack(cr, ranges, nranges);
    if (crlen > 0 && buf_chunk(&b, "CRNG", cr, crlen) != 0) {
        free(body);
        free(b.p);
        return -1;
    }
    if (grab) {
        w16(gr, (uint16_t)gx);
        w16(gr + 2, (uint16_t)gy);
        if (buf_chunk(&b, "GRAB", gr, 4) != 0) {
            free(body);
            free(b.p);
            return -1;
        }
    }
    if (buf_chunk(&b, "BODY", body, bodylen) != 0) {
        free(body);
        free(b.p);
        return -1;
    }
    free(body);
    rc = form_wrap("ILBM", &b, &file, &flen);
    free(b.p);
    if (rc != 0) {
        return -1;
    }
    rc = write_file(path, file, flen);
    free(file);
    return rc;
}

static int anim5_encode(const uint8_t *cur, const uint8_t *prev,
                        int w, int h, int planes, uint8_t **out, int *outlen)
{
    int rb = rowbytes_of(w);
    int pb = rb * h;
    uint8_t *a = (uint8_t *)malloc((size_t)planes * (size_t)pb);
    uint8_t *pr = (uint8_t *)malloc((size_t)planes * (size_t)pb);
    uint8_t *cols;
    int p, x, cap, o;
    uint32_t offsets[8];
    (void)prev;
    if (a == NULL || pr == NULL) {
        free(a);
        free(pr);
        return -1;
    }
    planes_from_pix(cur, w, h, planes, a);
    if (prev != NULL) {
        planes_from_pix(prev, w, h, planes, pr);
    } else {
        memset(pr, 0, (size_t)planes * (size_t)pb);
    }
    cap = 8 * 4 + planes * rb * (h * 2 + 8);
    cols = (uint8_t *)malloc((size_t)cap);
    if (cols == NULL) {
        free(a);
        free(pr);
        return -1;
    }
    o = 8 * 4;
    memset(offsets, 0, sizeof(offsets));
    for (p = 0; p < planes && p < 8; p++) {
        int same = memcmp(a + p * pb, pr + p * pb, (size_t)pb) == 0;
        if (same) {
            offsets[p] = 0;
            continue;
        }
        offsets[p] = (uint32_t)o;
        for (x = 0; x < rb; x++) {
            int y = 0;
            while (y < h) {
                int y2 = y + 1;
                uint8_t v = a[(p * h + y) * rb + x];
                while (y2 < h && y2 - y < 127 &&
                       a[(p * h + y2) * rb + x] == v) {
                    y2++;
                }
                if (y2 - y >= 2) {
                    cols[o++] = (uint8_t)(-(y2 - y));
                    cols[o++] = v;
                    y = y2;
                } else {
                    int start = y;
                    y++;
                    while (y < h && y - start < 127) {
                        int run = 1;
                        uint8_t vv = a[(p * h + y) * rb + x];
                        while (y + run < h && run < 127 &&
                               a[(p * h + (y + run)) * rb + x] == vv) {
                            run++;
                        }
                        if (run >= 2) {
                            break;
                        }
                        y++;
                    }
                    cols[o++] = (uint8_t)(y - start);
                    while (start < y) {
                        cols[o++] = a[(p * h + start) * rb + x];
                        start++;
                    }
                }
            }
            cols[o++] = 0;
        }
    }
    for (p = 0; p < 8; p++) {
        w32(cols + p * 4, offsets[p]);
    }
    free(a);
    free(pr);
    *out = cols;
    *outlen = o;
    return 0;
}

static int kent_delta(const uint8_t *dlta, int dlen)
{
    int p, min = 0;
    if (dlen < 64) {
        return 0;
    }
    for (p = 0; p < 8; p++) {
        int o = (int)r32(dlta + p * 4);
        if (o != 0 && (min == 0 || o < min)) {
            min = o;
        }
    }
    return min >= 64;
}

static int anim5_decode_kent(uint8_t *pl, int rb, int h, int planes,
                             const uint8_t *dlta, int dlen)
{
    uint32_t off[8];
    int p, x;
    for (p = 0; p < 8; p++) {
        off[p] = r32(dlta + p * 4);
    }
    for (p = 0; p < planes && p < 8; p++) {
        int i;
        if (off[p] == 0) {
            continue;
        }
        if ((int)off[p] >= dlen) {
            return -1;
        }
        i = (int)off[p];
        for (x = 0; x < rb; x++) {
            int ops, k, y;
            if (i >= dlen) {
                return -1;
            }
            ops = dlta[i++];
            y = 0;
            for (k = 0; k < ops; k++) {
                int code;
                if (i >= dlen) {
                    return -1;
                }
                code = dlta[i++];
                if (code == 0) {
                    int n, c;
                    if (i + 1 >= dlen) {
                        return -1;
                    }
                    n = dlta[i++];
                    c = dlta[i++];
                    if (y + n > h) {
                        return -1;
                    }
                    while (n-- > 0) {
                        pl[(p * h + y) * rb + x] = (uint8_t)c;
                        y++;
                    }
                } else if (code & 0x80) {
                    int n = code & 0x7f;
                    if (n < 1 || i + n > dlen || y + n > h) {
                        return -1;
                    }
                    while (n-- > 0) {
                        pl[(p * h + y) * rb + x] = dlta[i++];
                        y++;
                    }
                } else {
                    y += code;
                    if (y > h) {
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

static int anim5_decode(uint8_t *pix, int w, int h, int planes,
                        const uint8_t *dlta, int dlen)
{
    int rb = rowbytes_of(w);
    int pb = rb * h;
    uint8_t *pl;
    uint32_t off[8];
    int p, x, i;
    if (dlen < 32) {
        return -1;
    }
    pl = (uint8_t *)malloc((size_t)planes * (size_t)pb);
    if (pl == NULL) {
        return -1;
    }
    planes_from_pix(pix, w, h, planes, pl);
    if (kent_delta(dlta, dlen)) {
        if (anim5_decode_kent(pl, rb, h, planes, dlta, dlen) != 0) {
            free(pl);
            return -1;
        }
        pix_from_planes(pix, w, h, planes, pl);
        free(pl);
        return 0;
    }
    for (p = 0; p < 8; p++) {
        off[p] = r32(dlta + p * 4);
    }
    for (p = 0; p < planes && p < 8; p++) {
        if (off[p] == 0) {
            continue;
        }
        if ((int)off[p] >= dlen) {
            free(pl);
            return -1;
        }
        i = (int)off[p];
        for (x = 0; x < rb; x++) {
            int y = 0;
            for (;;) {
            int n;
            if (i >= dlen) {
                free(pl);
                return -1;
            }
            n = (int8_t)dlta[i++];
            if (n == 0) {
                break;
            }
            if (n > 0) {
                if (i + n > dlen || y + n > h) {
                    free(pl);
                    return -1;
                }
                while (n-- > 0) {
                    pl[(p * h + y) * rb + x] = dlta[i++];
                    y++;
                }
            } else {
                int c;
                n = -n;
                if (i >= dlen || y + n > h) {
                    free(pl);
                    return -1;
                }
                c = dlta[i++];
                while (n-- > 0) {
                    pl[(p * h + y) * rb + x] = (uint8_t)c;
                    y++;
                }
            }
            }
        }
    }
    pix_from_planes(pix, w, h, planes, pl);
    free(pl);
    return 0;
}

static void anhd_pack(uint8_t a[40], int op, int w, int h, int reltime)
{
    memset(a, 0, 40);
    a[0] = (uint8_t)op;
    w16(a + 2, (uint16_t)w);
    w16(a + 4, (uint16_t)h);
    w32(a + 12, (uint32_t)reltime);
}

static void dpan_pack(uint8_t d[8], int nframes, int fps)
{
    memset(d, 0, 8);
    w16(d, 3);
    w16(d + 2, (uint16_t)nframes);
    w16(d + 6, (uint16_t)fps);
}

static int ilbm_inner(Buf *b, const DpFrame *f, const DpColor *pal, int ncolors,
                      int camg, const DpRange *ranges, int nranges,
                      int compression, int with_dpan, int nframes, int fps,
                      int grab, int gx, int gy)
{
    int planes;
    uint8_t bm[20], cm[DP_MAX_COLORS * 3], cr[DP_MAX_RANGES * 8];
    uint8_t cam[4], dp[8], gr[4];
    uint8_t *body = NULL;
    int bodylen = 0, crlen;
    planes = planes_for(ncolors);
    if (build_body(f, planes, compression, &body, &bodylen) != 0) {
        return -1;
    }
    bmhd_pack(bm, f->w, f->h, planes, compression, 0, f->w, f->h);
    cmap_pack(cm, pal, ncolors);
    w32(cam, (uint32_t)camg);
    if (buf_chunk(b, "BMHD", bm, 20) != 0 ||
        buf_chunk(b, "CMAP", cm, ncolors * 3) != 0 ||
        buf_chunk(b, "CAMG", cam, 4) != 0) {
        free(body);
        return -1;
    }
    if (with_dpan) {
        dpan_pack(dp, nframes, fps);
        if (buf_chunk(b, "DPAN", dp, 8) != 0) {
            free(body);
            return -1;
        }
    }
    crlen = crng_pack(cr, ranges, nranges);
    if (crlen > 0 && buf_chunk(b, "CRNG", cr, crlen) != 0) {
        free(body);
        return -1;
    }
    if (grab) {
        w16(gr, (uint16_t)gx);
        w16(gr + 2, (uint16_t)gy);
        if (buf_chunk(b, "GRAB", gr, 4) != 0) {
            free(body);
            return -1;
        }
    }
    if (buf_chunk(b, "BODY", body, bodylen) != 0) {
        free(body);
        return -1;
    }
    free(body);
    return 0;
}

int iff_save_anim(const char *path, const DpFrame *frames, int nframes,
                  const DpColor *pal, int ncolors, int camg,
                  const DpRange *ranges, int nranges,
                  int fps, int compressed, int grab, int gx, int gy, int slot)
{
    Buf all;
    int i;
    int rel = fps > 0 ? (60 + fps / 2) / fps : 5;
    uint8_t *file = NULL;
    int flen = 0, rc;
    int planes;
    memset(&all, 0, sizeof(all));
    if (nframes < 1) {
        return -1;
    }
    ncolors = save_ncolors(frames, nframes, ncolors, slot);
    planes = planes_for(ncolors);
    for (i = 0; i < nframes; i++) {
        Buf inner;
        uint8_t *form = NULL;
        int formlen = 0;
        memset(&inner, 0, sizeof(inner));
        if (i == 0 || !compressed) {
            if (ilbm_inner(&inner, &frames[i], pal, ncolors, camg, ranges, nranges,
                           1, i == 0, nframes, fps, grab && i == 0, gx, gy) != 0) {
                free(all.p);
                return -1;
            }
        } else {
            uint8_t bm[20], ah[40];
            uint8_t *dlta = NULL;
            int dltalen = 0;
            bmhd_pack(bm, frames[i].w, frames[i].h, planes, 0, 0,
                      frames[i].w, frames[i].h);
            anhd_pack(ah, 5, frames[i].w, frames[i].h, rel);
            if (anim5_encode(frames[i].pix, frames[i - 1].pix, frames[i].w,
                             frames[i].h, planes, &dlta, &dltalen) != 0 ||
                buf_chunk(&inner, "BMHD", bm, 20) != 0 ||
                buf_chunk(&inner, "ANHD", ah, 40) != 0 ||
                buf_chunk(&inner, "DLTA", dlta, dltalen) != 0) {
                free(dlta);
                free(inner.p);
                free(all.p);
                return -1;
            }
            free(dlta);
        }
        if (form_wrap("ILBM", &inner, &form, &formlen) != 0) {
            free(inner.p);
            free(all.p);
            return -1;
        }
        free(inner.p);
        if (buf_bytes(&all, form, formlen) != 0) {
            free(form);
            free(all.p);
            return -1;
        }
        free(form);
    }
    rc = form_wrap("ANIM", &all, &file, &flen);
    free(all.p);
    if (rc != 0) {
        return -1;
    }
    rc = write_file(path, file, flen);
    free(file);
    return rc;
}

int iff_save_prsp(const char *path, const uint8_t dpps[DP_DPPS_LEN])
{
    Buf b;
    uint8_t *file = NULL;
    int flen = 0, rc;
    memset(&b, 0, sizeof(b));
    if (buf_chunk(&b, "DPPS", dpps, DP_DPPS_LEN) != 0) {
        return -1;
    }
    rc = form_wrap("PRSP", &b, &file, &flen);
    free(b.p);
    if (rc != 0) {
        return -1;
    }
    rc = write_file(path, file, flen);
    free(file);
    return rc;
}

int iff_save_stencil(const char *path, const uint8_t *mask, int w, int h)
{
    DpFrame f;
    DpColor pal[2];
    int rc;
    int i;
    memset(&f, 0, sizeof(f));
    f.w = w;
    f.h = h;
    f.pix = (uint8_t *)malloc((size_t)w * (size_t)h);
    if (f.pix == NULL) {
        return -1;
    }
    for (i = 0; i < w * h; i++) {
        f.pix[i] = mask[i] ? 1 : 0;
    }
    pal[0].r = pal[0].g = pal[0].b = 0;
    pal[1].r = pal[1].g = pal[1].b = 255;
    rc = iff_save_ilbm(path, &f, pal, 2, 0, NULL, 0, 0, 0, 0, 0, 1, 0);
    free(f.pix);
    return rc;
}

typedef struct {
    const uint8_t *p;
    int n;
    int i;
} Rd;

static int rd_skip(Rd *r, int n)
{
    if (n < 0 || r->i + n > r->n) {
        return -1;
    }
    r->i += n;
    return 0;
}

static int rd_chunk(Rd *r, char id[4], const uint8_t **data, int *len, int end)
{
    int n;
    if (r->i + 8 > end || r->i + 8 > r->n) {
        return 0;
    }
    memcpy(id, r->p + r->i, 4);
    n = (int)r32(r->p + r->i + 4);
    r->i += 8;
    if (n < 0 || r->i + n > r->n || r->i + n > end) {
        return -1;
    }
    *data = r->p + r->i;
    *len = n;
    r->i += n;
    if (n & 1) {
        if (rd_skip(r, 1) != 0) {
            return -1;
        }
    }
    return 1;
}

static int decode_body(const uint8_t *body, int bodylen, int compression,
                       int w, int h, int planes, uint8_t *pix)
{
    int rb = rowbytes_of(w);
    int pb = rb * h;
    uint8_t *pl = (uint8_t *)malloc((size_t)planes * (size_t)pb);
    int y, p, i = 0;
    if (pl == NULL) {
        return -1;
    }
    if (compression == 0) {
        if (bodylen < planes * pb) {
            free(pl);
            return -1;
        }
        memcpy(pl, body, (size_t)planes * (size_t)pb);
    } else if (compression == 1) {
        memset(pl, 0, (size_t)planes * (size_t)pb);
        for (y = 0; y < h; y++) {
            for (p = 0; p < planes; p++) {
                int used = 0;
                if (unpack_row(body + i, bodylen - i, pl + (p * h + y) * rb,
                               rb, &used) != 0) {
                    free(pl);
                    return -1;
                }
                i += used;
            }
        }
    } else {
        free(pl);
        return -1;
    }
    pix_from_planes(pix, w, h, planes, pl);
    free(pl);
    return 0;
}

typedef struct {
    int w, h, planes, compression, trans, camg;
    int xaspect, yaspect;
    int pagew, pageh;
    int gx, gy, grab;
    int op, reltime;
    int dpan_frames, dpan_fps, has_dpan;
    DpColor pal[DP_MAX_COLORS];
    int ncolors;
    DpRange ranges[DP_MAX_RANGES];
    int nranges;
    uint8_t *body;
    int bodylen;
    uint8_t *dlta;
    int dltalen;
    uint8_t *pix;
} Ilbm;

static void ilbm_clear(Ilbm *im)
{
    free(im->body);
    free(im->dlta);
    free(im->pix);
    memset(im, 0, sizeof(*im));
}

static int parse_ilbm(const uint8_t *p, int n, Ilbm *im, char *err, int errlen)
{
    Rd r;
    int end;
    memset(im, 0, sizeof(*im));
    im->op = -1;
    if (n < 12 || memcmp(p, "FORM", 4) != 0) {
        set_err(err, errlen, "Not an IFF file.");
        return -1;
    }
    end = 8 + (int)r32(p + 4);
    if (end > n) {
        end = n;
    }
    if (memcmp(p + 8, "ILBM", 4) != 0) {
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    r.p = p;
    r.n = n;
    r.i = 12;
    while (r.i + 8 <= end) {
        char id[5];
        const uint8_t *data;
        int len, rc;
        id[4] = 0;
        rc = rd_chunk(&r, id, &data, &len, end);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            set_err(err, errlen, "Short IFF chunk");
            return -1;
        }
        if (memcmp(id, "BMHD", 4) == 0 && len >= 20) {
            im->w = r16(data);
            im->h = r16(data + 2);
            im->planes = data[8];
            im->compression = data[10];
            im->trans = r16(data + 12);
            im->xaspect = data[14];
            im->yaspect = data[15];
            im->pagew = (int16_t)r16(data + 16);
            im->pageh = (int16_t)r16(data + 18);
        } else if (memcmp(id, "CMAP", 4) == 0) {
            int i, nc = len / 3, maxc = 0;
            if (nc > DP_MAX_COLORS) {
                nc = DP_MAX_COLORS;
            }
            for (i = 0; i < nc * 3 && i < len; i++) {
                if (data[i] > maxc) {
                    maxc = data[i];
                }
            }
            for (i = 0; i < nc; i++) {
                int r = data[i * 3];
                int g = data[i * 3 + 1];
                int b = data[i * 3 + 2];
                if (maxc <= 15) {
                    r *= 17;
                    g *= 17;
                    b *= 17;
                }
                im->pal[i].r = (uint8_t)r;
                im->pal[i].g = (uint8_t)g;
                im->pal[i].b = (uint8_t)b;
            }
            im->ncolors = nc;
        } else if (memcmp(id, "CAMG", 4) == 0 && len >= 4) {
            im->camg = (int)r32(data);
        } else if (memcmp(id, "CRNG", 4) == 0 && len >= 8 && im->nranges < DP_MAX_RANGES) {
            int o = 0;
            while (o + 8 <= len && im->nranges < DP_MAX_RANGES) {
                DpRange *rg = &im->ranges[im->nranges++];
                rg->rate = r16(data + o + 2);
                rg->active = r16(data + o + 4) ? 1 : 0;
                rg->low = data[o + 6];
                rg->high = data[o + 7];
                o += 8;
            }
        } else if (memcmp(id, "DRNG", 4) == 0 && len >= 6 && im->nranges < DP_MAX_RANGES) {
            DpRange *rg = &im->ranges[im->nranges++];
            rg->low = data[0];
            rg->high = data[1];
            rg->rate = r16(data + 2);
            rg->active = (r16(data + 4) & 1) ? 1 : 0;
        } else if (memcmp(id, "GRAB", 4) == 0 && len >= 4) {
            im->grab = 1;
            im->gx = (int16_t)r16(data);
            im->gy = (int16_t)r16(data + 2);
        } else if (memcmp(id, "DPAN", 4) == 0 && len >= 8) {
            im->has_dpan = 1;
            im->dpan_frames = r16(data + 2);
            im->dpan_fps = r16(data + 6);
        } else if (memcmp(id, "ANHD", 4) == 0 && len >= 16) {
            im->op = data[0];
            im->reltime = (int)r32(data + 12);
            if (im->w < 1 && len >= 6) {
                im->w = r16(data + 2);
                im->h = r16(data + 4);
            }
        } else if (memcmp(id, "BODY", 4) == 0) {
            free(im->body);
            im->body = (uint8_t *)malloc((size_t)len);
            if (im->body == NULL) {
                return -1;
            }
            memcpy(im->body, data, (size_t)len);
            im->bodylen = len;
        } else if (memcmp(id, "DLTA", 4) == 0) {
            free(im->dlta);
            im->dlta = (uint8_t *)malloc((size_t)len);
            if (im->dlta == NULL) {
                return -1;
            }
            memcpy(im->dlta, data, (size_t)len);
            im->dltalen = len;
        }
    }
    if (im->w < 1 || im->h < 1 || im->w > 4096 || im->h > 4096) {
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    if ((im->camg & 0x800) && im->planes != 6) {
        set_err(err, errlen, "HAM not in this build");
        return -1;
    }
    if (im->planes < 1 && im->op == 5) {
        im->planes = 0;
    } else if (im->planes < 1 || im->planes > 6) {
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    if (im->ncolors < 1) {
        im->ncolors = 1 << im->planes;
        if (im->ncolors > DP_MAX_COLORS) {
            im->ncolors = DP_MAX_COLORS;
        }
    }
    return 0;
}

static int color_dist(DpColor c, int r, int g, int b)
{
    int dr = (int)c.r - r;
    int dg = (int)c.g - g;
    int db = (int)c.b - b;
    return dr * dr + dg * dg + db * db;
}

static void ham_step(int value, const DpColor *base, int nbase, int *r, int *g, int *b)
{
    int ctl = (value >> 4) & 3;
    int nib = value & 15;
    int i;
    if (ctl == 0) {
        i = nib < nbase ? nib : 0;
        *r = base[i].r;
        *g = base[i].g;
        *b = base[i].b;
        return;
    }
    if (ctl == 2) {
        *r = nib * 17;
    } else if (ctl == 3) {
        *g = nib * 17;
    } else {
        *b = nib * 17;
    }
}

static int nearest_pal(const DpColor *pal, int n, int r, int g, int b)
{
    int i, best = 0, bd = 1 << 30;
    for (i = 0; i < n; i++) {
        int d = color_dist(pal[i], r, g, b);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

/* Six planes are HAM unless the file is an indexed 64-color picture. */
static int is_ham6(int planes, int camg, int ncolors)
{
    if (planes != 6) {
        return 0;
    }
    if (camg & 0x800) {
        return 1;
    }
    if (ncolors > 32) {
        return 0;
    }
    return 1;
}

static void ham_quantize(uint8_t *pix, int w, int h, DpColor *pal, int *ncolors, int grow)
{
    DpColor base[16];
    int nbase, i, n, r, g, b, npix;
    nbase = *ncolors;
    if (nbase < 1) {
        nbase = 1;
    }
    if (nbase > 16) {
        nbase = 16;
    }
    memcpy(base, pal, (size_t)nbase * sizeof(DpColor));
    n = grow ? nbase : *ncolors;
    if (n < 1) {
        n = 1;
    }
    if (n > DP_MAX_COLORS) {
        n = DP_MAX_COLORS;
    }
    r = base[0].r;
    g = base[0].g;
    b = base[0].b;
    npix = w * h;
    for (i = 0; i < npix; i++) {
        int idx, dist;
        ham_step(pix[i], base, nbase, &r, &g, &b);
        idx = nearest_pal(pal, n, r, g, b);
        dist = color_dist(pal[idx], r, g, b);
        if (grow && dist > 48 * 48 && n < DP_MAX_COLORS) {
            pal[n].r = (uint8_t)r;
            pal[n].g = (uint8_t)g;
            pal[n].b = (uint8_t)b;
            idx = n;
            n++;
        }
        pix[i] = (uint8_t)idx;
    }
    if (grow) {
        *ncolors = n;
    }
}

static int materialize(Ilbm *im, const uint8_t *prev, char *err, int errlen)
{
    size_t n = (size_t)im->w * (size_t)im->h;
    im->pix = (uint8_t *)malloc(n);
    if (im->pix == NULL) {
        return -1;
    }
    if (im->body != NULL) {
        if (decode_body(im->body, im->bodylen, im->compression, im->w, im->h,
                        im->planes, im->pix) != 0) {
            set_err(err, errlen, "Mangled IFF file");
            return -1;
        }
        return 0;
    }
    if (im->dlta != NULL && im->op == 5 && prev != NULL) {
        memcpy(im->pix, prev, n);
        if (anim5_decode(im->pix, im->w, im->h, im->planes, im->dlta, im->dltalen) != 0) {
            set_err(err, errlen, "Not a recognizable ANIM type !!");
            return -1;
        }
        return 0;
    }
    set_err(err, errlen, "Mangled IFF file");
    return -1;
}

int iff_load_ilbm(const char *path, DpFrame *f, DpColor *pal, int *ncolors,
                  int *camg, DpRange *ranges, int *nranges,
                  int *grab, int *gx, int *gy, int *trans, char *err, int errlen)
{
    uint8_t *file = NULL;
    int flen = 0;
    Ilbm im;
    FILE *fp = fopen(path, "rb");
    long sz;
    if (fp == NULL) {
        set_err(err, errlen, "No such file found.");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    rewind(fp);
    if (sz < 12 || sz > 64 * 1024 * 1024) {
        fclose(fp);
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    file = (uint8_t *)malloc((size_t)sz);
    if (file == NULL || fread(file, 1, (size_t)sz, fp) != (size_t)sz) {
        free(file);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    flen = (int)sz;
    if (parse_ilbm(file, flen, &im, err, errlen) != 0 ||
        materialize(&im, NULL, err, errlen) != 0) {
        ilbm_clear(&im);
        free(file);
        return -1;
    }
    if (is_ham6(im.planes, im.camg, im.ncolors)) {
        ham_quantize(im.pix, im.w, im.h, im.pal, &im.ncolors, 1);
    }
    free(f->pix);
    f->pix = im.pix;
    im.pix = NULL;
    f->w = im.w;
    f->h = im.h;
    if (pal != NULL && ncolors != NULL) {
        memcpy(pal, im.pal, sizeof(im.pal));
        *ncolors = im.ncolors;
    }
    if (camg) {
        *camg = im.camg;
    }
    if (ranges && nranges) {
        memcpy(ranges, im.ranges, sizeof(im.ranges));
        *nranges = im.nranges;
    }
    if (grab) {
        *grab = im.grab;
    }
    if (gx) {
        *gx = im.gx;
    }
    if (gy) {
        *gy = im.gy;
    }
    if (trans) {
        *trans = im.trans;
    }
    ilbm_clear(&im);
    free(file);
    return 0;
}

static void drop_loaded(DpFrame *frames, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        free(frames[i].pix);
        frames[i].pix = NULL;
    }
}

int iff_load_anim(const char *path, DpFrame *frames, int maxframes, int *nframes,
                  DpColor *pal, int *ncolors, int *camg,
                  DpRange *ranges, int *nranges, int *fps,
                  char *err, int errlen)
{
    FILE *fp = fopen(path, "rb");
    long sz;
    uint8_t *file;
    int end, i, count = 0;
    Rd r;
    uint8_t *prev = NULL;
    int prev_n = 0;
    int prev_planes = 0;
    if (fp == NULL) {
        set_err(err, errlen, "No such file found.");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    rewind(fp);
    if (sz < 12 || sz > 128 * 1024 * 1024) {
        fclose(fp);
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    file = (uint8_t *)malloc((size_t)sz);
    if (file == NULL || fread(file, 1, (size_t)sz, fp) != (size_t)sz) {
        free(file);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (memcmp(file, "FORM", 4) != 0) {
        free(file);
        set_err(err, errlen, "Not an IFF file.");
        return -1;
    }
    end = 8 + (int)r32(file + 4);
    if (end > (int)sz) {
        end = (int)sz;
    }
    if (memcmp(file + 8, "ANIM", 4) != 0) {
        free(file);
        set_err(err, errlen, "Not a recognizable ANIM type !!");
        return -1;
    }
    r.p = file;
    r.n = (int)sz;
    r.i = 12;
    while (r.i + 8 <= end && count < maxframes) {
        char id[4];
        const uint8_t *data;
        int len, rc;
        Ilbm im;
        rc = rd_chunk(&r, id, &data, &len, end);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            drop_loaded(frames, count);
            free(prev);
            free(file);
            set_err(err, errlen, "Short IFF chunk");
            return -1;
        }
        if (memcmp(id, "FORM", 4) != 0) {
            continue;
        }
        {
            const uint8_t *form = data - 8;
            int formlen = len + 8;
            if (form < file || form + formlen > file + sz) {
                drop_loaded(frames, count);
                free(prev);
                free(file);
                set_err(err, errlen, "Mangled IFF file");
                return -1;
            }
            if (parse_ilbm(form, formlen, &im, err, errlen) != 0) {
                ilbm_clear(&im);
                drop_loaded(frames, count);
                free(prev);
                free(file);
                return -1;
            }
            if (im.planes < 1) {
                if (prev_planes < 1) {
                    ilbm_clear(&im);
                    drop_loaded(frames, count);
                    free(prev);
                    free(file);
                    set_err(err, errlen, "Bad IFF Form");
                    return -1;
                }
                im.planes = prev_planes;
            }
            if (count > 0 && (im.w * im.h) != prev_n) {
                ilbm_clear(&im);
                drop_loaded(frames, count);
                free(prev);
                free(file);
                set_err(err, errlen, "Can't load different sized picture as anim frame");
                return -1;
            }
            if (materialize(&im, prev, err, errlen) != 0) {
                ilbm_clear(&im);
                drop_loaded(frames, count);
                free(prev);
                free(file);
                return -1;
            }
            frames[count].w = im.w;
            frames[count].h = im.h;
            frames[count].pix = im.pix;
            im.pix = NULL;
            if (count == 0) {
                if (pal && ncolors) {
                    memcpy(pal, im.pal, sizeof(im.pal));
                    *ncolors = im.ncolors;
                }
                if (camg) {
                    *camg = im.camg;
                }
                if (ranges && nranges) {
                    memcpy(ranges, im.ranges, sizeof(im.ranges));
                    *nranges = im.nranges;
                }
                if (fps) {
                    if (im.reltime > 0 && im.reltime <= 60) {
                        *fps = 60 / im.reltime;
                    } else if (im.has_dpan && im.dpan_fps > 0 && im.dpan_fps <= 60) {
                        *fps = im.dpan_fps;
                    } else {
                        *fps = 12;
                    }
                    if (*fps < 1) {
                        *fps = 1;
                    }
                }
            }
            free(prev);
            prev_n = im.w * im.h;
            prev = (uint8_t *)malloc((size_t)prev_n);
            if (prev == NULL) {
                ilbm_clear(&im);
                drop_loaded(frames, count + 1);
                free(file);
                return -1;
            }
            memcpy(prev, frames[count].pix, (size_t)prev_n);
            if (is_ham6(im.planes, im.camg, im.ncolors)) {
                if (count == 0) {
                    ham_quantize(frames[count].pix, im.w, im.h, im.pal, &im.ncolors, 1);
                    if (pal != NULL && ncolors != NULL) {
                        memcpy(pal, im.pal, sizeof(im.pal));
                        *ncolors = im.ncolors;
                    }
                } else if (pal != NULL && ncolors != NULL && *ncolors > 0) {
                    ham_quantize(frames[count].pix, im.w, im.h, pal, ncolors, 0);
                }
            }
            prev_planes = im.planes;
            count++;
            ilbm_clear(&im);
        }
        (void)i;
    }
    free(prev);
    free(file);
    if (count < 1) {
        set_err(err, errlen, "Not a recognizable ANIM type !!");
        return -1;
    }
    *nframes = count;
    return 0;
}

int iff_load_prsp(const char *path, uint8_t dpps[DP_DPPS_LEN], char *err, int errlen)
{
    FILE *fp = fopen(path, "rb");
    long sz;
    uint8_t *file;
    Rd r;
    int end, found = 0;
    if (fp == NULL) {
        set_err(err, errlen, "No such file found.");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    rewind(fp);
    if (sz < 12 || sz > 1024 * 1024) {
        fclose(fp);
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    file = (uint8_t *)malloc((size_t)sz);
    if (file == NULL || fread(file, 1, (size_t)sz, fp) != (size_t)sz) {
        free(file);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (memcmp(file, "FORM", 4) != 0 || memcmp(file + 8, "PRSP", 4) != 0) {
        free(file);
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    end = 8 + (int)r32(file + 4);
    if (end > (int)sz) {
        end = (int)sz;
    }
    r.p = file;
    r.n = (int)sz;
    r.i = 12;
    memset(dpps, 0, DP_DPPS_LEN);
    while (r.i + 8 <= end) {
        char id[4];
        const uint8_t *data;
        int len, rc;
        rc = rd_chunk(&r, id, &data, &len, end);
        if (rc <= 0) {
            break;
        }
        if (memcmp(id, "DPPS", 4) == 0) {
            int n = len < DP_DPPS_LEN ? len : DP_DPPS_LEN;
            memcpy(dpps, data, (size_t)n);
            found = 1;
        }
    }
    free(file);
    if (!found) {
        set_err(err, errlen, "Bad IFF Form");
        return -1;
    }
    return 0;
}

int iff_load_stencil(const char *path, uint8_t **mask, int *w, int *h,
                     char *err, int errlen)
{
    DpFrame f;
    int ncolors = 0, planes_guess;
    memset(&f, 0, sizeof(f));
    if (iff_load_ilbm(path, &f, NULL, &ncolors, NULL, NULL, NULL,
                      NULL, NULL, NULL, NULL, err, errlen) != 0) {
        return -1;
    }
    planes_guess = ncolors <= 2 ? 1 : 0;
    if (!planes_guess && ncolors > 2) {
        free(f.pix);
        set_err(err, errlen, "Not a stencil file");
        return -1;
    }
    *mask = f.pix;
    *w = f.w;
    *h = f.h;
    return 0;
}

static uint32_t png_crc_table[256];
static int png_crc_ready;

static void png_crc_init(void)
{
    int n, k;
    if (png_crc_ready) {
        return;
    }
    for (n = 0; n < 256; n++) {
        uint32_t c = (uint32_t)n;
        for (k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        png_crc_table[n] = c;
    }
    png_crc_ready = 1;
}

static uint32_t png_crc(const uint8_t *buf, int n)
{
    uint32_t c = 0xFFFFFFFFu;
    int i;
    png_crc_init();
    for (i = 0; i < n; i++) {
        c = png_crc_table[(c ^ buf[i]) & 255u] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static uint32_t png_adler(const uint8_t *buf, int n)
{
    uint32_t a = 1, b = 0;
    int i;
    for (i = 0; i < n; i++) {
        a = (a + buf[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static int png_chunk(FILE *fp, const char *type, const uint8_t *data, int n)
{
    uint8_t head[8];
    uint8_t crcbuf[4];
    uint32_t c;
    uint8_t *tmp;
    w32(head, (uint32_t)n);
    memcpy(head + 4, type, 4);
    if (fwrite(head, 1, 8, fp) != 8) {
        return -1;
    }
    if (n > 0 && fwrite(data, 1, (size_t)n, fp) != (size_t)n) {
        return -1;
    }
    tmp = (uint8_t *)malloc((size_t)n + 4u);
    if (tmp == NULL) {
        return -1;
    }
    memcpy(tmp, type, 4);
    if (n > 0) {
        memcpy(tmp + 4, data, (size_t)n);
    }
    c = png_crc(tmp, n + 4);
    free(tmp);
    w32(crcbuf, c);
    if (fwrite(crcbuf, 1, 4, fp) != 4) {
        return -1;
    }
    return 0;
}

int iff_write_png(const char *path, const uint8_t *pix, int w, int h,
                  const DpColor *pal, int ncolors)
{
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    FILE *fp;
    uint8_t ihdr[13];
    uint8_t *raw, *zbuf;
    int stride, y, x, zlen, off, cap;
    uint32_t ad;
    if (path == NULL || pix == NULL || pal == NULL || w < 1 || h < 1) {
        return -1;
    }
    if (ncolors < 1) {
        ncolors = 1;
    }
    if (ncolors > DP_MAX_COLORS) {
        ncolors = DP_MAX_COLORS;
    }
    stride = 1 + w * 3;
    raw = (uint8_t *)malloc((size_t)stride * (size_t)h);
    if (raw == NULL) {
        return -1;
    }
    for (y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)y * (size_t)stride;
        row[0] = 0;
        for (x = 0; x < w; x++) {
            int c = pix[y * w + x];
            if (c < 0 || c >= ncolors) {
                c = 0;
            }
            row[1 + x * 3] = pal[c].r;
            row[2 + x * 3] = pal[c].g;
            row[3 + x * 3] = pal[c].b;
        }
    }
    cap = (int)((size_t)stride * (size_t)h) + ((stride * h) / 65535 + 1) * 5 + 6;
    zbuf = (uint8_t *)malloc((size_t)cap);
    if (zbuf == NULL) {
        free(raw);
        return -1;
    }
    zbuf[0] = 0x78;
    zbuf[1] = 0x01;
    off = 2;
    {
        int left = stride * h;
        int src = 0;
        while (left > 0) {
            int n = left > 65535 ? 65535 : left;
            int final = (n == left);
            zbuf[off++] = (uint8_t)(final ? 1 : 0);
            zbuf[off++] = (uint8_t)(n & 255);
            zbuf[off++] = (uint8_t)((n >> 8) & 255);
            zbuf[off++] = (uint8_t)((~n) & 255);
            zbuf[off++] = (uint8_t)(((~n) >> 8) & 255);
            memcpy(zbuf + off, raw + src, (size_t)n);
            off += n;
            src += n;
            left -= n;
        }
    }
    ad = png_adler(raw, stride * h);
    w32(zbuf + off, ad);
    off += 4;
    zlen = off;
    free(raw);
    memset(ihdr, 0, sizeof(ihdr));
    w32(ihdr, (uint32_t)w);
    w32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        free(zbuf);
        return -1;
    }
    if (fwrite(sig, 1, 8, fp) != 8 ||
        png_chunk(fp, "IHDR", ihdr, 13) != 0 ||
        png_chunk(fp, "IDAT", zbuf, zlen) != 0 ||
        png_chunk(fp, "IEND", NULL, 0) != 0) {
        fclose(fp);
        free(zbuf);
        return -1;
    }
    free(zbuf);
    if (fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

int dp_write_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    if (fwrite(rgb, 1, (size_t)w * (size_t)h * 3u, fp) !=
        (size_t)w * (size_t)h * 3u) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}
