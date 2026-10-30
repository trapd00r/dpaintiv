#include "dpaint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static int load_hunk(const char *path, uint8_t **out, int *outlen)
{
    FILE *fp;
    uint32_t nlibs, table, first, last, i, typ, nbytes;
    long sz;
    uint8_t *file = NULL;
    uint8_t *mem = NULL;
    int off;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 32 || sz > 8 * 1024 * 1024) {
        fclose(fp);
        return -1;
    }
    rewind(fp);
    file = (uint8_t *)malloc((size_t)sz);
    if (file == NULL || fread(file, 1, (size_t)sz, fp) != (size_t)sz) {
        free(file);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (rd32(file) != 0x000003F3u) {
        free(file);
        return -1;
    }
    off = 4;
    nlibs = rd32(file + off);
    off += 4;
    for (i = 0; i < nlibs; i++) {
        uint32_t n = rd32(file + off);
        off += 4 + (int)n * 4;
        if (off > sz) {
            free(file);
            return -1;
        }
    }
    if (off + 12 > sz) {
        free(file);
        return -1;
    }
    table = rd32(file + off);
    first = rd32(file + off + 4);
    last = rd32(file + off + 8);
    off += 12;
    (void)table;
    if (last < first || off + (int)(last - first + 1) * 4 > sz) {
        free(file);
        return -1;
    }
    off += (int)(last - first + 1) * 4;
    if (off + 8 > sz) {
        free(file);
        return -1;
    }
    typ = rd32(file + off) & 0x3FFFFFFFu;
    off += 4;
    if (typ != 0x3E9u && typ != 0x3EAu) {
        free(file);
        return -1;
    }
    nbytes = (rd32(file + off) & 0x3FFFFFFFu) * 4u;
    off += 4;
    if (off + (int)nbytes > sz) {
        free(file);
        return -1;
    }
    mem = (uint8_t *)malloc(nbytes);
    if (mem == NULL) {
        free(file);
        return -1;
    }
    memcpy(mem, file + off, nbytes);
    free(file);
    *out = mem;
    *outlen = (int)nbytes;
    return 0;
}

static int parse_font(DpFont *f, uint8_t *mem, int len)
{
    int i;

    memset(f, 0, sizeof(*f));
    for (i = 0; i + 32 < len; i += 2) {
        int ys = rd16(mem + i);
        int flags = mem[i + 3];
        int xs, bl, bs, acc, lo, hi, n;
        uint32_t data, loc, sp, kern;
        int modulo;
        int idx, bw;

        if (ys < 5 || ys > 88) {
            continue;
        }
        if ((flags & 0x62) == 0) {
            continue;
        }
        xs = rd16(mem + i + 4);
        bl = rd16(mem + i + 6);
        bs = rd16(mem + i + 8);
        acc = rd16(mem + i + 10);
        lo = mem[i + 12];
        hi = mem[i + 13];
        if (xs < 1 || xs > 88 || bl < 0 || bl > ys || bs > 40 || acc > 8) {
            continue;
        }
        if (lo > 48 || hi <= lo || hi - lo > 250 || hi < 65) {
            continue;
        }
        data = rd32(mem + i + 14);
        modulo = rd16(mem + i + 18);
        loc = rd32(mem + i + 20);
        sp = rd32(mem + i + 24);
        kern = rd32(mem + i + 28);
        n = hi - lo + 1;
        if (modulo < 1 || modulo > 4000) {
            continue;
        }
        if (data + (uint32_t)ys * (uint32_t)modulo > (uint32_t)len) {
            continue;
        }
        if (loc + (uint32_t)n * 4u > (uint32_t)len) {
            continue;
        }
        if (sp + (uint32_t)n * 2u > (uint32_t)len) {
            continue;
        }
        if (lo > (int)'A' || hi < (int)'A') {
            continue;
        }
        idx = (int)'A' - lo;
        bw = rd16(mem + loc + (uint32_t)idx * 4u + 2u);
        if (bw < 1 || bw > 48) {
            continue;
        }
        f->data = mem;
        f->len = len;
        f->ysize = ys;
        f->baseline = bl;
        f->xsize = xs;
        f->lo = lo;
        f->hi = hi;
        f->modulo = modulo;
        f->data_off = (int)data;
        f->loc_off = (int)loc;
        f->space_off = (int)sp;
        f->kern_off = (int)kern;
        f->kern_ok = (kern + (uint32_t)n * 2u <= (uint32_t)len);
        return 0;
    }
    free(mem);
    return -1;
}

int dp_font_load(DpFont *f, const char *path)
{
    uint8_t *mem = NULL;
    int len = 0;

    memset(f, 0, sizeof(*f));
    if (load_hunk(path, &mem, &len) != 0) {
        return -1;
    }
    return parse_font(f, mem, len);
}

int dp_font_adopt(DpFont *f, const uint8_t *bytes, int len, const char *face)
{
    uint8_t *mem;

    memset(f, 0, sizeof(*f));
    if (bytes == NULL || len < 32) {
        return -1;
    }
    mem = (uint8_t *)malloc((size_t)len);
    if (mem == NULL) {
        return -1;
    }
    memcpy(mem, bytes, (size_t)len);
    if (parse_font(f, mem, len) != 0) {
        return -1;
    }
    if (face != NULL) {
        snprintf(f->face, sizeof(f->face), "%s", face);
    }
    return 0;
}

void dp_font_free(DpFont *f)
{
    free(f->data);
    memset(f, 0, sizeof(*f));
}

static int glyph(const DpFont *f, int ch, int *bitpos, int *bitw, int *adv, int *kern)
{
    int idx;
    const uint8_t *p;

    if (f->data == NULL || ch < f->lo || ch > f->hi) {
        return -1;
    }
    idx = ch - f->lo;
    p = f->data + f->loc_off + idx * 4;
    *bitpos = rd16(p);
    *bitw = rd16(p + 2);
    *adv = (int16_t)rd16(f->data + f->space_off + idx * 2);
    *kern = 0;
    if (f->kern_ok) {
        *kern = (int16_t)rd16(f->data + f->kern_off + idx * 2);
    }
    if (*adv <= 0) {
        *adv = *bitw > 0 ? *bitw : (f->xsize > 0 ? f->xsize : 6);
    }
    if (*bitw < 0) {
        *bitw = 0;
    }
    return 0;
}

/* Advance is CharSpace. The bitmap is drawn at pen + CharKern. */
static int advance_of(const DpFont *f, unsigned char c, int *kern, int *bitw)
{
    int bp, bw, adv, k;
    if (glyph(f, (int)c, &bp, &bw, &adv, &k) != 0) {
        *kern = 0;
        *bitw = f->xsize > 0 ? f->xsize : 6;
        return *bitw;
    }
    *kern = k;
    *bitw = bw;
    {
        int step = adv + k;
        /* Keep at least one pixel between glyph boxes so letters do not touch. */
        if (step < k + bw + 1) {
            step = k + bw + 1;
        }
        if (step < 1) {
            step = 1;
        }
        return step;
    }
}

int dp_font_char_space(const DpFont *f, int ch)
{
    int bp, bw, adv, k;
    if (f == NULL || glyph(f, ch, &bp, &bw, &adv, &k) != 0) {
        return 0;
    }
    return adv;
}

int dp_font_width(const DpFont *f, const char *s)
{
    int x = 0;
    if (f == NULL || f->data == NULL || s == NULL) {
        return 0;
    }
    for (; *s; s++) {
        int k, bw;
        x += advance_of(f, (unsigned char)*s, &k, &bw);
    }
    return x;
}

int dp_font_glyphs(const DpFont *f, const char *s, int x,
                   int *xs, int *ws, int maxn)
{
    int n = 0;
    if (f == NULL || f->data == NULL || s == NULL || maxn < 1) {
        return 0;
    }
    for (; *s && n < maxn; s++) {
        int k, bw;
        int adv = advance_of(f, (unsigned char)*s, &k, &bw);
        if (xs != NULL) {
            xs[n] = x + k;
        }
        if (ws != NULL) {
            ws[n] = bw > 0 ? bw : 0;
        }
        x += adv;
        n++;
    }
    return n;
}

static void paint_glyph(const DpFont *f, int ch, int x0, int y0,
                        void (*dot)(void *, int, int), void *user)
{
    int bp, bw, adv, kern, y, x;
    if (glyph(f, ch, &bp, &bw, &adv, &kern) != 0 || bw <= 0) {
        return;
    }
    (void)adv;
    x0 += kern;
    for (y = 0; y < f->ysize; y++) {
        int byte = bp >> 3;
        int bit = bp & 7;
        uint64_t acc = 0;
        int t;
        int row = f->data_off + y * f->modulo;
        for (t = 0; t < 8; t++) {
            int ix = row + byte + t;
            uint8_t b = 0;
            if (ix >= 0 && ix < f->len && byte + t < f->modulo) {
                b = f->data[ix];
            }
            acc = (acc << 8) | (uint64_t)b;
        }
        acc <<= bit;
        for (x = 0; x < bw && x < 48; x++) {
            if ((acc >> (63 - x)) & 1ull) {
                dot(user, x0 + x, y0 + y);
            }
        }
    }
}

struct IdxCtx {
    uint8_t *dst;
    int w, h;
    uint8_t color;
};

static void idx_dot(void *user, int x, int y)
{
    struct IdxCtx *c = user;
    if (x >= 0 && y >= 0 && x < c->w && y < c->h) {
        c->dst[y * c->w + x] = c->color;
    }
}

void dp_font_index(const DpFont *f, uint8_t *dst, int w, int h,
                   int x, int y, const char *s, uint8_t color)
{
    struct IdxCtx ctx;
    int pen = x;
    ctx.dst = dst;
    ctx.w = w;
    ctx.h = h;
    ctx.color = color;
    if (f == NULL || f->data == NULL || s == NULL) {
        return;
    }
    for (; *s; s++) {
        int k, bw;
        unsigned char c = (unsigned char)*s;
        paint_glyph(f, (int)c, pen, y, idx_dot, &ctx);
        pen += advance_of(f, c, &k, &bw);
    }
}

struct RgbCtx {
    uint8_t *rgb;
    int w, h, scale;
    uint8_t r, g, b;
};

static void rgb_dot(void *user, int x, int y)
{
    struct RgbCtx *c = user;
    int sy, sx;
    for (sy = 0; sy < c->scale; sy++) {
        for (sx = 0; sx < c->scale; sx++) {
            int px = x * c->scale + sx;
            int py = y * c->scale + sy;
            int di;
            if (px < 0 || py < 0 || px >= c->w || py >= c->h) {
                continue;
            }
            di = (py * c->w + px) * 3;
            c->rgb[di] = c->r;
            c->rgb[di + 1] = c->g;
            c->rgb[di + 2] = c->b;
        }
    }
}

void dp_font_rgb(const DpFont *f, uint8_t *rgb, int w, int h,
                 int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b,
                 int scale)
{
    struct RgbCtx ctx;
    int pen, top;
    if (scale < 1) {
        scale = 1;
    }
    ctx.rgb = rgb;
    ctx.w = w;
    ctx.h = h;
    ctx.scale = scale;
    ctx.r = r;
    ctx.g = g;
    ctx.b = b;
    pen = x / scale;
    top = y / scale;
    if (f == NULL || f->data == NULL || s == NULL) {
        return;
    }
    for (; *s; s++) {
        int k, bw;
        unsigned char c = (unsigned char)*s;
        paint_glyph(f, (int)c, pen, top, rgb_dot, &ctx);
        pen += advance_of(f, c, &k, &bw);
    }
}
