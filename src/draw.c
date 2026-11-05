#include "dpaint.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *dp_tool_name(int tool)
{
    static const char *names[] = {
        "Dot", "Draw", "Line", "Curve", "Fill", "Rect", "FillRect",
        "Circle", "Ellipse", "Airbrush", "Text", "Brush"
    };
    if (tool < 0 || tool >= TOOL_COUNT) {
        return "?";
    }
    return names[tool];
}

const char *dp_mode_name(int mode)
{
    static const char *names[] = {
        "Matte", "Color", "Replc", "Smear", "Shade", "Blend",
        "Cycle", "Smooth", "HBrite", "Mix"
    };
    if (mode < 0 || mode >= MODE_COUNT) {
        return "?";
    }
    return names[mode];
}

const char *const *dp_about_lines(int *n)
{
    static const char *lines[] = {
        "Created by Daniel Silva,",
        "Lee Taran, and Steve Shaw",
        "(c) 1985-1991 Electronic Arts",
        "About Deluxe Paint IV"
    };
    *n = 4;
    return lines;
}

static int fw(const Doc *d)
{
    return d->frames[d->cur].w;
}

static int fh(const Doc *d)
{
    return d->frames[d->cur].h;
}

static uint8_t *fpix(Doc *d)
{
    return d->frames[d->cur].pix;
}

static void snap_free(struct DpUndo *s)
{
    free(s->pix);
    s->pix = NULL;
    s->w = 0;
    s->h = 0;
    s->frame = 0;
}

static void stack_clear(struct DpUndo *s, int *n)
{
    int i;
    for (i = 0; i < *n; i++) {
        snap_free(&s[i]);
    }
    *n = 0;
}

static void undo_clear(Doc *d)
{
    stack_clear(d->ustack, &d->un);
    stack_clear(d->rstack, &d->rn);
}

static void drop_oldest(struct DpUndo *s, int *n)
{
    if (*n < 1) {
        return;
    }
    snap_free(&s[0]);
    if (*n > 1) {
        memmove(&s[0], &s[1], (size_t)(*n - 1) * sizeof(struct DpUndo));
    }
    *n -= 1;
    memset(&s[*n], 0, sizeof(struct DpUndo));
}

static int snap_store(struct DpUndo *s, const Doc *d)
{
    size_t n = (size_t)fw(d) * (size_t)fh(d);
    uint8_t *p = (uint8_t *)malloc(n);
    if (p == NULL) {
        return -1;
    }
    memcpy(p, fpix((Doc *)d), n);
    free(s->pix);
    s->pix = p;
    s->w = fw(d);
    s->h = fh(d);
    s->frame = d->cur;
    return 0;
}

static int snap_match(const struct DpUndo *s, const Doc *d)
{
    return s->pix != NULL && s->frame == d->cur && s->w == fw(d) && s->h == fh(d);
}

static void snap_swap(struct DpUndo *s, Doc *d)
{
    size_t n = (size_t)fw(d) * (size_t)fh(d);
    uint8_t *tmp = (uint8_t *)malloc(n);
    if (tmp == NULL) {
        return;
    }
    memcpy(tmp, fpix(d), n);
    memcpy(fpix(d), s->pix, n);
    memcpy(s->pix, tmp, n);
    free(tmp);
}

void doc_status(Doc *d, const char *s)
{
    snprintf(d->status, sizeof(d->status), "%s", s);
}

static void frame_free(DpFrame *f)
{
    free(f->pix);
    f->pix = NULL;
    f->w = 0;
    f->h = 0;
}

static int frame_alloc(DpFrame *f, int w, int h, uint8_t fill)
{
    size_t n = (size_t)w * (size_t)h;
    uint8_t *p;
    if (w < 1 || h < 1 || n > 1600u * 1200u) {
        return -1;
    }
    p = (uint8_t *)malloc(n);
    if (p == NULL) {
        return -1;
    }
    memset(p, fill, n);
    free(f->pix);
    f->pix = p;
    f->w = w;
    f->h = h;
    return 0;
}

void brush_free(DpBrush *b)
{
    free(b->pix);
    b->pix = NULL;
    b->w = 0;
    b->h = 0;
}

int brush_clone(DpBrush *dst, const DpBrush *src)
{
    size_t n;
    uint8_t *p;
    if (src->pix == NULL || src->w < 1 || src->h < 1) {
        return -1;
    }
    n = (size_t)src->w * (size_t)src->h;
    p = (uint8_t *)malloc(n);
    if (p == NULL) {
        return -1;
    }
    memcpy(p, src->pix, n);
    free(dst->pix);
    *dst = *src;
    dst->pix = p;
    return 0;
}

int doc_nearest(const Doc *d, int r, int g, int b)
{
    int i, best = 0, bestd = 1 << 30;
    int n = d->ncolors > 0 ? d->ncolors : 1;
    for (i = 0; i < n; i++) {
        int dr = (int)d->pal[i].r - r;
        int dg = (int)d->pal[i].g - g;
        int db = (int)d->pal[i].b - b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestd) {
            bestd = dist;
            best = i;
        }
    }
    return best;
}

static void set_default_range(Doc *d)
{
    d->nranges = 1;
    d->ranges[0].low = 20;
    d->ranges[0].high = 31;
    d->ranges[0].rate = 16;
    d->ranges[0].active = 0;
}

int doc_init(Doc *d, int w, int h, int ncolors, const DpColor *pal)
{
    int i;
    memset(d, 0, sizeof(*d));
    if (ncolors < 2) {
        ncolors = 2;
    }
    if (ncolors > DP_MAX_COLORS) {
        ncolors = DP_MAX_COLORS;
    }
    d->ncolors = ncolors;
    d->nframes = 1;
    d->cur = 0;
    d->fps = 12;
    d->range_a = 0;
    d->range_b = 0;
    d->play_dir = 1;
    d->tool = TOOL_DOT;
    d->brush_d = 1;
    d->grid_on = 0;
    d->sym_on = 0;
    d->sym_pass = 0;
    d->mode = MODE_COLOR;
    d->handle_center = 1;
    d->method_compressed = 1;
    d->bg = 0;
    d->fg = ncolors > 2 ? 1 : ncolors - 1;
    if (pal != NULL) {
        memcpy(d->pal, pal, sizeof(d->pal));
    }
    for (i = 32; i < d->ncolors; i++) {
        int t = i - 32;
        d->pal[i].r = (uint8_t)(15 + t * 7);
        d->pal[i].g = (uint8_t)(255 - t * 5);
        d->pal[i].b = (uint8_t)((t * 40) & 255);
    }
    memcpy(d->pal_saved, d->pal, sizeof(d->pal));
    set_default_range(d);
    d->translucency_pct = 50;
    d->dpps[0] = 0;
    d->dpps[1] = 1;
    if (frame_alloc(&d->frames[0], w, h, 0) != 0) {
        return -1;
    }
    if (frame_alloc(&d->spare, w, h, 0) != 0) {
        doc_free(d);
        return -1;
    }
    d->undo_hist = 1;
    doc_status(d, "Deluxe Paint IV");
    (void)i;
    return 0;
}

void doc_free(Doc *d)
{
    int i;
    for (i = 0; i < d->nframes; i++) {
        frame_free(&d->frames[i]);
    }
    frame_free(&d->spare);
    brush_free(&d->brush);
    brush_free(&d->brush_orig);
    brush_free(&d->brush_spare);
    for (i = 0; i < d->nabrush; i++) {
        brush_free(&d->abrush[i]);
    }
    free(d->stencil);
    d->stencil = NULL;
    free(d->bg_fixed);
    d->bg_fixed = NULL;
    d->bg_fixed_on = 0;
    undo_clear(d);
    d->nframes = 0;
}

DpFrame *doc_frame(Doc *d)
{
    return &d->frames[d->cur];
}

static int realloc_mask(uint8_t **mask, int oldw, int oldh, int w, int h, const uint8_t *src)
{
    uint8_t *p = (uint8_t *)calloc((size_t)w * (size_t)h, 1);
    int y, x, cw, ch;
    if (p == NULL) {
        return -1;
    }
    if (src != NULL && oldw > 0 && oldh > 0) {
        cw = oldw < w ? oldw : w;
        ch = oldh < h ? oldh : h;
        for (y = 0; y < ch; y++) {
            memcpy(p + y * w, src + y * oldw, (size_t)cw);
        }
    }
    free(*mask);
    *mask = p;
    (void)x;
    return 0;
}

int doc_resize(Doc *d, int w, int h, int ncolors)
{
    int i;
    int oldw = d->nframes > 0 ? d->frames[0].w : 0;
    int oldh = d->nframes > 0 ? d->frames[0].h : 0;
    if (ncolors < 2) {
        ncolors = 2;
    }
    if (ncolors > DP_MAX_COLORS) {
        ncolors = DP_MAX_COLORS;
    }
    for (i = 0; i < d->nframes; i++) {
        DpFrame nfr;
        int y, x, cw, ch;
        memset(&nfr, 0, sizeof(nfr));
        if (frame_alloc(&nfr, w, h, (uint8_t)d->bg) != 0) {
            return -1;
        }
        cw = oldw < w ? oldw : w;
        ch = oldh < h ? oldh : h;
        for (y = 0; y < ch; y++) {
            memcpy(nfr.pix + y * w, d->frames[i].pix + y * oldw, (size_t)cw);
        }
        if (ncolors < d->ncolors) {
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    if (nfr.pix[y * w + x] >= ncolors) {
                        nfr.pix[y * w + x] = (uint8_t)(ncolors - 1);
                    }
                }
            }
        }
        frame_free(&d->frames[i]);
        d->frames[i] = nfr;
    }
    {
        DpFrame nfr;
        int y, cw, ch;
        memset(&nfr, 0, sizeof(nfr));
        if (frame_alloc(&nfr, w, h, (uint8_t)d->bg) != 0) {
            return -1;
        }
        cw = d->spare.w < w ? d->spare.w : w;
        ch = d->spare.h < h ? d->spare.h : h;
        for (y = 0; y < ch; y++) {
            memcpy(nfr.pix + y * w, d->spare.pix + y * d->spare.w, (size_t)cw);
        }
        frame_free(&d->spare);
        d->spare = nfr;
    }
    if (d->stencil != NULL) {
        if (realloc_mask(&d->stencil, oldw, oldh, w, h, d->stencil) != 0) {
            return -1;
        }
    }
    if (d->bg_fixed != NULL) {
        if (realloc_mask(&d->bg_fixed, oldw, oldh, w, h, d->bg_fixed) != 0) {
            return -1;
        }
    }
    undo_clear(d);
    d->ncolors = ncolors;
    if (d->fg >= ncolors) {
        d->fg = ncolors - 1;
    }
    if (d->bg >= ncolors) {
        d->bg = 0;
    }
    return 0;
}

void doc_set_format(Doc *d, int fmt)
{
    int w = 320, h = 200, n = 32, camg = 0;
    if (fmt == FMT_MED) {
        w = 640;
        h = 200;
        n = 16;
        camg = 0x8000;
    } else if (fmt == FMT_LACE) {
        w = 320;
        h = 400;
        n = 32;
        camg = 0x0004;
    } else if (fmt == FMT_HI) {
        w = 640;
        h = 400;
        n = 16;
        camg = 0x8004;
    }
    if (doc_resize(d, w, h, n) == 0) {
        d->camg = camg;
        doc_status(d, fmt == FMT_LO ? "Lo-Res     320x200" :
                       fmt == FMT_MED ? "Med-Res    640x200" :
                       fmt == FMT_LACE ? "Interlace  320x400" :
                       "Hi-Res     640x400");
    }
}

void doc_checkpoint(Doc *d)
{
    stack_clear(d->rstack, &d->rn);
    if (!d->undo_hist) {
        while (d->un > 1) {
            drop_oldest(d->ustack, &d->un);
        }
        if (d->un == 1) {
            snap_store(&d->ustack[0], d);
            return;
        }
    } else if (d->un >= DP_UNDO_MAX) {
        drop_oldest(d->ustack, &d->un);
    }
    if (d->un >= DP_UNDO_MAX) {
        return;
    }
    memset(&d->ustack[d->un], 0, sizeof(d->ustack[0]));
    if (snap_store(&d->ustack[d->un], d) == 0) {
        d->un++;
    }
}

void doc_preview_restore(Doc *d)
{
    struct DpUndo *s;
    size_t n;
    if (d->un < 1) {
        return;
    }
    s = &d->ustack[d->un - 1];
    if (!snap_match(s, d)) {
        return;
    }
    n = (size_t)s->w * (size_t)s->h;
    memcpy(fpix(d), s->pix, n);
}

void doc_undo(Doc *d)
{
    struct DpUndo *s;
    if (d->un < 1) {
        doc_status(d, "UNDO");
        return;
    }
    s = &d->ustack[d->un - 1];
    if (!snap_match(s, d)) {
        doc_status(d, "UNDO");
        return;
    }
    snap_swap(s, d);
    if (d->undo_hist) {
        if (d->rn >= DP_UNDO_MAX) {
            drop_oldest(d->rstack, &d->rn);
        }
        d->rstack[d->rn] = *s;
        memset(s, 0, sizeof(*s));
        d->un--;
        d->rn++;
    }
    doc_status(d, "UNDO");
}

void doc_redo(Doc *d)
{
    struct DpUndo *s;
    if (!d->undo_hist || d->rn < 1) {
        doc_status(d, "REDO");
        return;
    }
    s = &d->rstack[d->rn - 1];
    if (!snap_match(s, d)) {
        doc_status(d, "REDO");
        return;
    }
    snap_swap(s, d);
    if (d->un >= DP_UNDO_MAX) {
        drop_oldest(d->ustack, &d->un);
    }
    d->ustack[d->un] = *s;
    memset(s, 0, sizeof(*s));
    d->rn--;
    d->un++;
    doc_status(d, "REDO");
}

void doc_set_undo_hist(Doc *d, int on)
{
    if (!on) {
        if (d->rn > 0) {
            struct DpUndo keep = d->rstack[d->rn - 1];
            d->rstack[d->rn - 1].pix = NULL;
            stack_clear(d->rstack, &d->rn);
            stack_clear(d->ustack, &d->un);
            d->ustack[0] = keep;
            d->un = keep.pix != NULL ? 1 : 0;
        } else {
            while (d->un > 1) {
                drop_oldest(d->ustack, &d->un);
            }
        }
    }
    d->undo_hist = on ? 1 : 0;
}

static void rgb_to_hsv(int r, int g, int b, int *h, int *s, int *v)
{
    int max = r;
    int min = r;
    int dlt;
    if (g > max) {
        max = g;
    }
    if (b > max) {
        max = b;
    }
    if (g < min) {
        min = g;
    }
    if (b < min) {
        min = b;
    }
    *v = max;
    dlt = max - min;
    if (max <= 0 || dlt == 0) {
        *s = 0;
        *h = 0;
        return;
    }
    *s = dlt * 255 / max;
    if (max == r) {
        *h = 60 * (g - b) / dlt;
    } else if (max == g) {
        *h = 120 + 60 * (b - r) / dlt;
    } else {
        *h = 240 + 60 * (r - g) / dlt;
    }
    if (*h < 0) {
        *h += 360;
    }
}

static void hsv_to_rgb(int h, int s, int v, DpColor *c)
{
    int i, f, p, q, t;
    if (h < 0) {
        h += 360;
    }
    h %= 360;
    if (s < 0) {
        s = 0;
    }
    if (s > 255) {
        s = 255;
    }
    if (v < 0) {
        v = 0;
    }
    if (v > 255) {
        v = 255;
    }
    if (s == 0) {
        c->r = c->g = c->b = (uint8_t)v;
        return;
    }
    i = h / 60;
    f = h % 60;
    p = v * (255 - s) / 255;
    q = v * (255 - s * f / 60) / 255;
    t = v * (255 - s * (60 - f) / 60) / 255;
    if (i == 0) {
        c->r = (uint8_t)v;
        c->g = (uint8_t)t;
        c->b = (uint8_t)p;
    } else if (i == 1) {
        c->r = (uint8_t)q;
        c->g = (uint8_t)v;
        c->b = (uint8_t)p;
    } else if (i == 2) {
        c->r = (uint8_t)p;
        c->g = (uint8_t)v;
        c->b = (uint8_t)t;
    } else if (i == 3) {
        c->r = (uint8_t)p;
        c->g = (uint8_t)q;
        c->b = (uint8_t)v;
    } else if (i == 4) {
        c->r = (uint8_t)t;
        c->g = (uint8_t)p;
        c->b = (uint8_t)v;
    } else {
        c->r = (uint8_t)v;
        c->g = (uint8_t)p;
        c->b = (uint8_t)q;
    }
}

void doc_pal_spread(Doc *d, int a, int b)
{
    int i, n, h0, s0, v0, h1, s1, v1, dh;
    DpColor c0, c1;
    if (d == NULL || a == b || a < 0 || b < 0 || a >= d->ncolors || b >= d->ncolors) {
        return;
    }
    c0 = d->pal[a];
    c1 = d->pal[b];
    rgb_to_hsv(c0.r, c0.g, c0.b, &h0, &s0, &v0);
    rgb_to_hsv(c1.r, c1.g, c1.b, &h1, &s1, &v1);
    dh = h1 - h0;
    if (dh > 180) {
        dh -= 360;
    }
    if (dh < -180) {
        dh += 360;
    }
    n = a < b ? b - a : a - b;
    for (i = 0; i <= n; i++) {
        int idx = a < b ? a + i : a - i;
        int h = h0 + dh * i / n;
        int s = s0 + (s1 - s0) * i / n;
        int v = v0 + (v1 - v0) * i / n;
        if (h < 0) {
            h += 360;
        }
        if (h >= 360) {
            h -= 360;
        }
        hsv_to_rgb(h, s, v, &d->pal[idx]);
    }
    d->pal[a] = c0;
    d->pal[b] = c1;
}

static int inside(const Doc *d, int x, int y)
{
    return x >= 0 && y >= 0 && x < fw(d) && y < fh(d);
}

static void write_px(Doc *d, int x, int y, int color)
{
    int i;
    uint8_t *p;
    if (!inside(d, x, y)) {
        return;
    }
    i = y * fw(d) + x;
    if (d->stencil_on && d->stencil != NULL && d->stencil[i]) {
        return;
    }
    if (color < 0) {
        color = 0;
    }
    if (color >= d->ncolors) {
        color = d->ncolors - 1;
    }
    p = fpix(d);
    p[i] = (uint8_t)color;
}

static int range_of(const Doc *d, int *lo, int *hi)
{
    if (d->nranges < 1) {
        *lo = 0;
        *hi = d->ncolors - 1;
        return 0;
    }
    *lo = d->ranges[0].low;
    *hi = d->ranges[0].high;
    if (*lo < 0) {
        *lo = 0;
    }
    if (*hi >= d->ncolors) {
        *hi = d->ncolors - 1;
    }
    if (*hi < *lo) {
        *hi = *lo;
    }
    return 1;
}

static int blend_idx(const Doc *d, int c_dst, int c_src, int pct)
{
    int r, g, b;
    if (c_dst < 0 || c_dst >= d->ncolors) {
        c_dst = 0;
    }
    if (c_src < 0 || c_src >= d->ncolors) {
        c_src = 0;
    }
    if (pct <= 0) {
        return c_dst;
    }
    if (pct >= 100) {
        return c_src;
    }
    r = ((int)d->pal[c_src].r * pct + (int)d->pal[c_dst].r * (100 - pct)) / 100;
    g = ((int)d->pal[c_src].g * pct + (int)d->pal[c_dst].g * (100 - pct)) / 100;
    b = ((int)d->pal[c_src].b * pct + (int)d->pal[c_dst].b * (100 - pct)) / 100;
    return doc_nearest(d, r, g, b);
}

static int process_idx(const Doc *d, int c_dst, int c_src)
{
    int h0, s0, v0, h1, s1, v1;
    DpColor out;
    if (c_dst < 0 || c_dst >= d->ncolors) {
        c_dst = 0;
    }
    if (c_src < 0 || c_src >= d->ncolors) {
        c_src = 0;
    }
    if (d->process_mode == PROC_TINT) {
        return blend_idx(d, c_dst, c_src, 35);
    }
    rgb_to_hsv(d->pal[c_dst].r, d->pal[c_dst].g, d->pal[c_dst].b, &h0, &s0, &v0);
    rgb_to_hsv(d->pal[c_src].r, d->pal[c_src].g, d->pal[c_src].b, &h1, &s1, &v1);
    if (d->process_mode == PROC_HUE) {
        hsv_to_rgb(h1, s0 > 16 ? s0 : s1, v0 > 16 ? v0 : v1, &out);
    } else {
        hsv_to_rgb(h0, s0, (v0 + v1) / 2, &out);
    }
    return doc_nearest(d, out.r, out.g, out.b);
}

void doc_plot(Doc *d, int x, int y, int color)
{
    int i, lo, hi, c;
    uint8_t *p;
    if (d->sym_on && !d->sym_pass) {
        int w = fw(d) - 1;
        int h = fh(d) - 1;
        d->sym_pass = 1;
        doc_plot(d, x, y, color);
        doc_plot(d, w - x, y, color);
        doc_plot(d, x, h - y, color);
        doc_plot(d, w - x, h - y, color);
        d->sym_pass = 0;
        return;
    }
    if (!inside(d, x, y)) {
        return;
    }
    i = y * fw(d) + x;
    if (d->stencil_on && d->stencil != NULL && d->stencil[i]) {
        return;
    }
    p = fpix(d);
    c = p[i];
    if (d->bg_fixed_on && d->bg_fixed != NULL && color == d->bg) {
        color = d->bg_fixed[i];
    }
    if (d->translucency_on) {
        int pct = d->translucency_pct > 0 ? d->translucency_pct : 50;
        p[i] = (uint8_t)blend_idx(d, c, color, pct);
        return;
    }
    if (d->process_on) {
        p[i] = (uint8_t)process_idx(d, c, color);
        return;
    }
    range_of(d, &lo, &hi);
    switch (d->mode) {
    case MODE_SHADE:
        if (c >= lo && c <= hi) {
            if (color == d->fg && c < hi) {
                c++;
            } else if (color == d->bg && c > lo) {
                c--;
            }
        }
        p[i] = (uint8_t)c;
        break;
    case MODE_BLEND: {
        int r = ((int)d->pal[c].r + (int)d->pal[color].r) / 2;
        int g = ((int)d->pal[c].g + (int)d->pal[color].g) / 2;
        int b = ((int)d->pal[c].b + (int)d->pal[color].b) / 2;
        p[i] = (uint8_t)doc_nearest(d, r, g, b);
        break;
    }
    case MODE_CYCLE:
        if (c >= lo && c <= hi) {
            c = (c == hi) ? lo : c + 1;
            p[i] = (uint8_t)c;
        }
        break;
    case MODE_SMOOTH: {
        int acc_r = 0, acc_g = 0, acc_b = 0, n = 0;
        int yy, xx;
        for (yy = y - 1; yy <= y + 1; yy++) {
            for (xx = x - 1; xx <= x + 1; xx++) {
                int cc;
                if (!inside(d, xx, yy)) {
                    continue;
                }
                cc = p[yy * fw(d) + xx];
                acc_r += d->pal[cc].r;
                acc_g += d->pal[cc].g;
                acc_b += d->pal[cc].b;
                n++;
            }
        }
        if (n > 0) {
            p[i] = (uint8_t)doc_nearest(d, acc_r / n, acc_g / n, acc_b / n);
        }
        break;
    }
    case MODE_HBRITE: {
        int r = d->pal[color].r / 2;
        int g = d->pal[color].g / 2;
        int b = d->pal[color].b / 2;
        p[i] = (uint8_t)doc_nearest(d, r, g, b);
        break;
    }
    case MODE_MIX:
        if (((x * 13 + y * 7) & 1) == 0) {
            p[i] = (uint8_t)color;
        }
        break;
    default:
        p[i] = (uint8_t)color;
        break;
    }
}

int doc_brush_covers(int size, int dx, int dy)
{
    static const int lim[8] = {0, 1, 2, 4, 5, 8, 10, 13};
    int n = size;
    if (n < 1) {
        n = 1;
    }
    if (n >= 5 && n <= 8) {
        static const int ex[4] = {2, 3, 3, 4};
        static const int ey[4] = {2, 2, 3, 3};
        int i = n - 5;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        return dx <= ex[i] && dy <= ey[i];
    }
    if (n == 9) {
        return (dx == 0 && dy == -1) || (dx == -1 && dy == 1) || (dx == 1 && dy == 1);
    }
    if (n == 10) {
        return (dx == -1 && dy == -1) || (dx == 1 && dy == -1) ||
               (dx == 0 && dy == 0) ||
               (dx == -1 && dy == 1) || (dx == 1 && dy == 1);
    }
    if (n > 8) {
        n = 8;
    }
    if (n == 1) {
        return dx == 0 && dy == 0;
    }
    return dx * dx + dy * dy <= lim[n - 1];
}

void doc_dab(Doc *d, int x, int y, int color)
{
    int dy, dx;
    if (d->brush_d <= 1) {
        doc_plot(d, x, y, color);
        return;
    }
    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            if (doc_brush_covers(d->brush_d, dx, dy)) {
                doc_plot(d, x + dx, y + dy, color);
            }
        }
    }
}

static void plot_aa(Doc *d, int x, int y, int color, int pct)
{
    int i, cur;
    if (!inside(d, x, y)) {
        return;
    }
    i = y * fw(d) + x;
    if (d->stencil_on && d->stencil != NULL && d->stencil[i]) {
        return;
    }
    cur = fpix(d)[i];
    if (cur == color) {
        return;
    }
    write_px(d, x, y, blend_idx(d, cur, color, pct));
}

void doc_line(Doc *d, int x0, int y0, int x1, int y1, int color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    int err;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    err = dx - dy;
    for (;;) {
        int e2;
        int ox = x0, oy = y0;
        doc_dab(d, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
        if (d->antialias > 0 && dx > 0 && dy > 0 && x0 != ox && y0 != oy) {
            int pct = d->antialias >= 2 ? 50 : 33;
            plot_aa(d, ox + sx, oy, color, pct);
            plot_aa(d, ox, oy + sy, color, pct);
        }
    }
}

void doc_rect(Doc *d, int x0, int y0, int x1, int y1, int color, int fill)
{
    int x, y, t;
    if (x0 > x1) {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        t = y0;
        y0 = y1;
        y1 = t;
    }
    if (fill) {
        for (y = y0; y <= y1; y++) {
            for (x = x0; x <= x1; x++) {
                doc_plot(d, x, y, color);
            }
        }
        return;
    }
    for (x = x0; x <= x1; x++) {
        doc_dab(d, x, y0, color);
        doc_dab(d, x, y1, color);
    }
    for (y = y0; y <= y1; y++) {
        doc_dab(d, x0, y, color);
        doc_dab(d, x1, y, color);
    }
}

void doc_ellipse(Doc *d, int cx, int cy, int rx, int ry, int color, int fill)
{
    int x, y;
    int64_t rx2, ry2, edge;
    if (rx < 0) {
        rx = -rx;
    }
    if (ry < 0) {
        ry = -ry;
    }
    if (rx == 0 && ry == 0) {
        doc_dab(d, cx, cy, color);
        return;
    }
    if (rx == 0) {
        doc_line(d, cx, cy - ry, cx, cy + ry, color);
        return;
    }
    if (ry == 0) {
        doc_line(d, cx - rx, cy, cx + rx, cy, color);
        return;
    }
    rx2 = (int64_t)rx * (int64_t)rx;
    ry2 = (int64_t)ry * (int64_t)ry;
    edge = rx2 * ry2;
    for (y = -ry; y <= ry; y++) {
        int xmin = rx + 1;
        int xmax = -rx - 1;
        for (x = -rx; x <= rx; x++) {
            int64_t v = (int64_t)x * (int64_t)x * ry2 + (int64_t)y * (int64_t)y * rx2;
            int ax, ay;
            int64_t vx, vy;
            if (v > edge) {
                continue;
            }
            if (fill) {
                if (x < xmin) {
                    xmin = x;
                }
                if (x > xmax) {
                    xmax = x;
                }
                continue;
            }
            ax = x >= 0 ? x + 1 : x - 1;
            ay = y >= 0 ? y + 1 : y - 1;
            vx = (int64_t)ax * (int64_t)ax * ry2 + (int64_t)y * (int64_t)y * rx2;
            vy = (int64_t)x * (int64_t)x * ry2 + (int64_t)ay * (int64_t)ay * rx2;
            if (vx > edge || vy > edge) {
                doc_dab(d, cx + x, cy + y, color);
                if (d->antialias > 0) {
                    int pct = d->antialias >= 2 ? 50 : 33;
                    if (vx > edge && x != 0) {
                        plot_aa(d, cx + ax, cy + y, color, pct);
                    }
                    if (vy > edge && y != 0) {
                        plot_aa(d, cx + x, cy + ay, color, pct);
                    }
                }
            }
        }
        if (fill && xmax >= xmin) {
            int fx;
            for (fx = xmin; fx <= xmax; fx++) {
                doc_plot(d, cx + fx, cy + y, color);
            }
        }
    }
}

void doc_fill(Doc *d, int x, int y, int color)
{
    int w = fw(d);
    int h = fh(d);
    int n = w * h;
    uint8_t *seen;
    int *qx, *qy;
    int qh = 0, qt = 0;
    uint8_t src;
    uint8_t *p;
    if (!inside(d, x, y)) {
        return;
    }
    p = fpix(d);
    src = p[y * w + x];
    if (src == (uint8_t)color) {
        return;
    }
    seen = (uint8_t *)calloc((size_t)n, 1);
    qx = (int *)malloc((size_t)n * sizeof(int));
    qy = (int *)malloc((size_t)n * sizeof(int));
    if (seen == NULL || qx == NULL || qy == NULL) {
        free(seen);
        free(qx);
        free(qy);
        return;
    }
    qx[qt] = x;
    qy[qt] = y;
    qt++;
    seen[y * w + x] = 1;
    while (qh < qt) {
        int cx = qx[qh];
        int cy = qy[qh];
        int k;
        qh++;
        if (p[cy * w + cx] != src) {
            continue;
        }
        doc_plot(d, cx, cy, color);
        for (k = 0; k < 4; k++) {
            int nx = cx + (k == 0 ? -1 : k == 1 ? 1 : 0);
            int ny = cy + (k == 2 ? -1 : k == 3 ? 1 : 0);
            int ni;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                continue;
            }
            ni = ny * w + nx;
            if (seen[ni] || p[ni] != src) {
                continue;
            }
            seen[ni] = 1;
            qx[qt] = nx;
            qy[qt] = ny;
            qt++;
        }
    }
    free(seen);
    free(qx);
    free(qy);
}

void doc_air(Doc *d, int x, int y, int color)
{
    int i;
    for (i = 0; i < 14; i++) {
        int dx = (int)((x * 17 + y * 31 + i * 13) % 17) - 8;
        int dy = (int)((x * 9 + y * 19 + i * 29) % 17) - 8;
        if (dx * dx + dy * dy <= 64) {
            doc_plot(d, x + dx, y + dy, color);
        }
    }
}

void doc_smear(Doc *d, int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int y, x;
    uint8_t *p = fpix(d);
    int w = fw(d);
    if (dx > 8) {
        dx = 8;
    }
    if (dx < -8) {
        dx = -8;
    }
    if (dy > 8) {
        dy = 8;
    }
    if (dy < -8) {
        dy = -8;
    }
    for (y = -2; y <= 2; y++) {
        for (x = -2; x <= 2; x++) {
            int sx = x1 - dx + x;
            int sy = y1 - dy + y;
            int c;
            if (!inside(d, sx, sy)) {
                continue;
            }
            c = p[sy * w + sx];
            write_px(d, x1 + x, y1 + y, c);
        }
    }
}

void doc_stamp_brush(Doc *d, const DpBrush *b, int x, int y, int use_bg)
{
    int by, bx;
    int ink = use_bg ? d->bg : d->fg;
    if (b == NULL || b->pix == NULL) {
        return;
    }
    for (by = 0; by < b->h; by++) {
        for (bx = 0; bx < b->w; bx++) {
            uint8_t c = b->pix[by * b->w + bx];
            int dx = x - b->hx + bx;
            int dy = y - b->hy + by;
            if (d->translucency_on || d->process_on) {
                if (d->mode == MODE_REPLC || c != b->trans) {
                    int sc = (d->mode == MODE_COLOR) ? ink : (int)c;
                    doc_plot(d, dx, dy, sc);
                }
            } else if (d->mode == MODE_REPLC) {
                write_px(d, dx, dy, c);
            } else if (d->mode == MODE_COLOR) {
                if (c != b->trans) {
                    doc_plot(d, dx, dy, ink);
                }
            } else if (d->mode == MODE_MATTE) {
                if (c != b->trans) {
                    write_px(d, dx, dy, c);
                }
            } else if (c != b->trans) {
                doc_plot(d, dx, dy, d->mode == MODE_REPLC ? c : ink);
            }
        }
    }
}

void doc_pickup(Doc *d, int x0, int y0, int x1, int y1)
{
    int t, w, h, y;
    uint8_t *p;
    if (x0 > x1) {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= fw(d)) {
        x1 = fw(d) - 1;
    }
    if (y1 >= fh(d)) {
        y1 = fh(d) - 1;
    }
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    if (w < 1 || h < 1) {
        return;
    }
    p = (uint8_t *)malloc((size_t)w * (size_t)h);
    if (p == NULL) {
        return;
    }
    for (y = 0; y < h; y++) {
        memcpy(p + y * w, fpix(d) + (y0 + y) * fw(d) + x0, (size_t)w);
    }
    brush_free(&d->brush);
    d->brush.pix = p;
    d->brush.w = w;
    d->brush.h = h;
    d->brush.trans = 0;
    d->brush.ncolors = d->ncolors;
    d->brush.has_pal = 1;
    memcpy(d->brush.pal, d->pal, sizeof(d->pal));
    if (d->handle_center) {
        d->brush.hx = w / 2;
        d->brush.hy = h / 2;
    } else {
        d->brush.hx = 0;
        d->brush.hy = 0;
    }
    d->pick_x = x0;
    d->pick_y = y0;
    d->pick_w = w;
    d->pick_h = h;
    d->pick_ok = 1;
    if (brush_clone(&d->brush_orig, &d->brush) == 0) {
        d->brush_orig_ok = 1;
    }
    doc_status(d, "Brush");
}

void doc_clear(Doc *d)
{
    if (d->bg_fixed_on && d->bg_fixed != NULL) {
        memcpy(fpix(d), d->bg_fixed, (size_t)fw(d) * (size_t)fh(d));
        return;
    }
    memset(fpix(d), (uint8_t)d->bg, (size_t)fw(d) * (size_t)fh(d));
}

static int copy_frame(DpFrame *dst, const DpFrame *src)
{
    return frame_alloc(dst, src->w, src->h, 0) == 0 &&
           (memcpy(dst->pix, src->pix, (size_t)src->w * (size_t)src->h), 1);
}

int doc_add_frames(Doc *d, int n, int blank)
{
    int i;
    if (n < 1) {
        return -1;
    }
    if (d->nframes + n > DP_MAX_FRAMES) {
        n = DP_MAX_FRAMES - d->nframes;
    }
    if (n < 1) {
        doc_status(d, "Not enough memory");
        return -1;
    }
    for (i = d->nframes - 1; i > d->cur; i--) {
        d->frames[i + n] = d->frames[i];
    }
    for (i = 0; i < n; i++) {
        DpFrame fr;
        memset(&fr, 0, sizeof(fr));
        if (blank) {
            if (frame_alloc(&fr, fw(d), fh(d), (uint8_t)d->bg) != 0) {
                return -1;
            }
        } else {
            if (!copy_frame(&fr, &d->frames[d->cur])) {
                return -1;
            }
        }
        d->frames[d->cur + 1 + i] = fr;
    }
    d->nframes += n;
    d->cur += 1;
    if (d->range_b < d->nframes - 1) {
        d->range_b = d->nframes - 1;
    }
    undo_clear(d);
    doc_status(d, "Add frames..");
    return 0;
}

int doc_delete_frames(Doc *d, int n)
{
    int i;
    if (d->nframes <= 1) {
        doc_status(d, "Can't delete only page.");
        return -1;
    }
    if (n < 1) {
        n = 1;
    }
    if (d->cur + n > d->nframes) {
        n = d->nframes - d->cur;
    }
    if (d->nframes - n < 1) {
        n = d->nframes - 1;
    }
    for (i = 0; i < n; i++) {
        frame_free(&d->frames[d->cur + i]);
    }
    for (i = d->cur + n; i < d->nframes; i++) {
        d->frames[i - n] = d->frames[i];
        d->frames[i].pix = NULL;
    }
    d->nframes -= n;
    if (d->cur >= d->nframes) {
        d->cur = d->nframes - 1;
    }
    if (d->range_b >= d->nframes) {
        d->range_b = d->nframes - 1;
    }
    if (d->range_a > d->range_b) {
        d->range_a = d->range_b;
    }
    undo_clear(d);
    doc_status(d, "Delete frames..");
    return 0;
}

void doc_goto(Doc *d, int index)
{
    if (index < 0) {
        index = 0;
    }
    if (index >= d->nframes) {
        index = d->nframes - 1;
    }
    d->cur = index;
    undo_clear(d);
}

void doc_swap_spare(Doc *d)
{
    DpFrame t = d->spare;
    d->spare = d->frames[d->cur];
    d->frames[d->cur] = t;
    undo_clear(d);
    doc_status(d, "Swap");
}

void doc_copy_spare(Doc *d)
{
    if (d->spare.w != fw(d) || d->spare.h != fh(d)) {
        frame_free(&d->spare);
        if (frame_alloc(&d->spare, fw(d), fh(d), 0) != 0) {
            return;
        }
    }
    memcpy(d->spare.pix, fpix(d), (size_t)fw(d) * (size_t)fh(d));
    doc_status(d, "Copy To Spare");
}

void doc_merge(Doc *d, int front)
{
    int w = fw(d);
    int h = fh(d);
    int i, n;
    if (d->spare.w != w || d->spare.h != h) {
        doc_status(d, "Can't merge different size pages.");
        return;
    }
    n = w * h;
    for (i = 0; i < n; i++) {
        if (front) {
            if (d->spare.pix[i] != (uint8_t)d->bg) {
                fpix(d)[i] = d->spare.pix[i];
            }
        } else if (fpix(d)[i] == (uint8_t)d->bg) {
            fpix(d)[i] = d->spare.pix[i];
        }
    }
    doc_status(d, front ? "Merge in front" : "Merge in back");
}

void doc_flip_page(Doc *d, int horiz)
{
    int w = fw(d);
    int h = fh(d);
    uint8_t *tmp = (uint8_t *)malloc((size_t)w * (size_t)h);
    int y, x;
    if (tmp == NULL) {
        return;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int sx = horiz ? (w - 1 - x) : x;
            int sy = horiz ? y : (h - 1 - y);
            tmp[y * w + x] = fpix(d)[sy * w + sx];
        }
    }
    memcpy(fpix(d), tmp, (size_t)w * (size_t)h);
    free(tmp);
}

void brush_flip(DpBrush *b, int horiz)
{
    int x, y;
    uint8_t *tmp;
    if (b->pix == NULL) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)b->w * (size_t)b->h);
    if (tmp == NULL) {
        return;
    }
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            int sx = horiz ? b->w - 1 - x : x;
            int sy = horiz ? y : b->h - 1 - y;
            tmp[y * b->w + x] = b->pix[sy * b->w + sx];
        }
    }
    memcpy(b->pix, tmp, (size_t)b->w * (size_t)b->h);
    free(tmp);
    if (horiz) {
        b->hx = b->w - 1 - b->hx;
    } else {
        b->hy = b->h - 1 - b->hy;
    }
}

void brush_rot90(DpBrush *b)
{
    int x, y;
    uint8_t *tmp;
    int nw, nh;
    if (b->pix == NULL) {
        return;
    }
    nw = b->h;
    nh = b->w;
    tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
    if (tmp == NULL) {
        return;
    }
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            int nx = b->h - 1 - y;
            int ny = x;
            tmp[ny * nw + nx] = b->pix[y * b->w + x];
        }
    }
    free(b->pix);
    b->pix = tmp;
    b->w = nw;
    b->h = nh;
    {
        int hx = b->h - 1 - b->hy;
        int hy = b->hx;
        /* h was swapped already; old hy maps with the pre-swap height. */
        (void)hx;
        (void)hy;
    }
    b->hx = nw / 2;
    b->hy = nh / 2;
}

void brush_scale(DpBrush *b, int nw, int nh)
{
    uint8_t *tmp;
    int y, x;
    if (b->pix == NULL || nw < 1 || nh < 1 || nw > 2000 || nh > 2000) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
    if (tmp == NULL) {
        return;
    }
    for (y = 0; y < nh; y++) {
        for (x = 0; x < nw; x++) {
            int sx = x * b->w / nw;
            int sy = y * b->h / nh;
            if (sx >= b->w) {
                sx = b->w - 1;
            }
            if (sy >= b->h) {
                sy = b->h - 1;
            }
            tmp[y * nw + x] = b->pix[sy * b->w + sx];
        }
    }
    free(b->pix);
    b->pix = tmp;
    b->hx = b->hx * nw / b->w;
    b->hy = b->hy * nh / b->h;
    b->w = nw;
    b->h = nh;
}

void brush_outline(DpBrush *b, int color)
{
    uint8_t *tmp;
    int x, y;
    if (b->pix == NULL) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)b->w * (size_t)b->h);
    if (tmp == NULL) {
        return;
    }
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            uint8_t c = b->pix[y * b->w + x];
            int edge = 0;
            if (c != b->trans) {
                int k;
                for (k = 0; k < 4; k++) {
                    int nx = x + (k == 0 ? -1 : k == 1 ? 1 : 0);
                    int ny = y + (k == 2 ? -1 : k == 3 ? 1 : 0);
                    if (nx < 0 || ny < 0 || nx >= b->w || ny >= b->h ||
                        b->pix[ny * b->w + nx] == b->trans) {
                        edge = 1;
                    }
                }
            }
            tmp[y * b->w + x] = edge ? (uint8_t)color : b->trans;
        }
    }
    memcpy(b->pix, tmp, (size_t)b->w * (size_t)b->h);
    free(tmp);
}

void brush_trim(DpBrush *b)
{
    uint8_t *tmp;
    int x, y, has_inner = 0;
    int x0, y0, x1 = -1, y1 = -1;
    if (b == NULL || b->pix == NULL || b->w < 1 || b->h < 1) {
        return;
    }
    x0 = b->w;
    y0 = b->h;
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            if (b->pix[y * b->w + x] != b->trans) {
                if (x < x0) {
                    x0 = x;
                }
                if (y < y0) {
                    y0 = y;
                }
                if (x > x1) {
                    x1 = x;
                }
                if (y > y1) {
                    y1 = y;
                }
            }
        }
    }
    if (x1 >= x0 && y1 >= y0 &&
        (x0 > 0 || y0 > 0 || x1 < b->w - 1 || y1 < b->h - 1)) {
        int nw = x1 - x0 + 1;
        int nh = y1 - y0 + 1;
        tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
        if (tmp != NULL) {
            for (y = 0; y < nh; y++) {
                memcpy(tmp + y * nw, b->pix + (y0 + y) * b->w + x0, (size_t)nw);
            }
            free(b->pix);
            b->pix = tmp;
            b->w = nw;
            b->h = nh;
            b->hx = b->hx - x0;
            b->hy = b->hy - y0;
            if (b->hx < 0 || b->hx >= nw) {
                b->hx = nw / 2;
            }
            if (b->hy < 0 || b->hy >= nh) {
                b->hy = nh / 2;
            }
        }
        return;
    }
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            if (b->pix[y * b->w + x] == b->trans) {
                continue;
            }
            if (x > 0 && x < b->w - 1 && y > 0 && y < b->h - 1 &&
                b->pix[(y - 1) * b->w + x] != b->trans &&
                b->pix[(y + 1) * b->w + x] != b->trans &&
                b->pix[y * b->w + (x - 1)] != b->trans &&
                b->pix[y * b->w + (x + 1)] != b->trans) {
                has_inner = 1;
            }
        }
    }
    if (has_inner) {
        tmp = (uint8_t *)malloc((size_t)b->w * (size_t)b->h);
        if (tmp != NULL) {
            for (y = 0; y < b->h; y++) {
                for (x = 0; x < b->w; x++) {
                    uint8_t c = b->pix[y * b->w + x];
                    int keep = (c != b->trans && x > 0 && x < b->w - 1 &&
                                y > 0 && y < b->h - 1 &&
                                b->pix[(y - 1) * b->w + x] != b->trans &&
                                b->pix[(y + 1) * b->w + x] != b->trans &&
                                b->pix[y * b->w + (x - 1)] != b->trans &&
                                b->pix[y * b->w + (x + 1)] != b->trans);
                    tmp[y * b->w + x] = keep ? c : b->trans;
                }
            }
            memcpy(b->pix, tmp, (size_t)b->w * (size_t)b->h);
            free(tmp);
        }
        brush_trim(b);
    }
}

void brush_shear(DpBrush *b, int sx, int sy)
{
    int asx, asy, nw, nh, x, y;
    uint8_t *tmp;
    if (b == NULL || b->pix == NULL || (sx == 0 && sy == 0)) {
        return;
    }
    asx = sx < 0 ? -sx : sx;
    asy = sy < 0 ? -sy : sy;
    nw = b->w + asx;
    nh = b->h + asy;
    if (nw < 1 || nh < 1 || nw > 2000 || nh > 2000) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
    if (tmp == NULL) {
        return;
    }
    memset(tmp, b->trans, (size_t)nw * (size_t)nh);
    for (y = 0; y < b->h; y++) {
        int ox = b->h > 1 ? ((sx >= 0 ? y : (b->h - 1 - y)) * asx) / (b->h - 1) : 0;
        for (x = 0; x < b->w; x++) {
            int oy = b->w > 1 ? ((sy >= 0 ? x : (b->w - 1 - x)) * asy) / (b->w - 1) : 0;
            int dx = x + ox;
            int dy = y + oy;
            if (dx >= 0 && dx < nw && dy >= 0 && dy < nh) {
                tmp[dy * nw + dx] = b->pix[y * b->w + x];
            }
        }
    }
    free(b->pix);
    b->pix = tmp;
    b->w = nw;
    b->h = nh;
    b->hx = nw / 2;
    b->hy = nh / 2;
}

void brush_rotate_angle(DpBrush *b, int deg)
{
    double rad, ca, sa, scx, scy, dcx, dcy;
    int nw, nh, dx, dy;
    uint8_t *tmp;
    if (b == NULL || b->pix == NULL) {
        return;
    }
    deg %= 360;
    if (deg == 0) {
        return;
    }
    rad = (double)deg * 3.14159265358979323846 / 180.0;
    ca = cos(rad);
    sa = sin(rad);
    nw = (int)ceil(fabs((double)b->w * ca) + fabs((double)b->h * sa));
    nh = (int)ceil(fabs((double)b->w * sa) + fabs((double)b->h * ca));
    if (nw < 1) {
        nw = 1;
    }
    if (nh < 1) {
        nh = 1;
    }
    if (nw > 2000 || nh > 2000) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
    if (tmp == NULL) {
        return;
    }
    memset(tmp, b->trans, (size_t)nw * (size_t)nh);
    scx = (double)(b->w - 1) * 0.5;
    scy = (double)(b->h - 1) * 0.5;
    dcx = (double)(nw - 1) * 0.5;
    dcy = (double)(nh - 1) * 0.5;
    for (dy = 0; dy < nh; dy++) {
        for (dx = 0; dx < nw; dx++) {
            double rx = (double)dx - dcx;
            double ry = (double)dy - dcy;
            int sx = (int)floor(rx * ca + ry * sa + scx + 0.5);
            int sy = (int)floor(-rx * sa + ry * ca + scy + 0.5);
            if (sx >= 0 && sx < b->w && sy >= 0 && sy < b->h) {
                tmp[dy * nw + dx] = b->pix[sy * b->w + sx];
            }
        }
    }
    free(b->pix);
    b->pix = tmp;
    b->w = nw;
    b->h = nh;
    b->hx = nw / 2;
    b->hy = nh / 2;
}

void brush_bend(DpBrush *b, int horiz, int amount)
{
    int a, nw, nh, x, y;
    uint8_t *tmp;
    if (b == NULL || b->pix == NULL || amount == 0) {
        return;
    }
    a = amount < 0 ? -amount : amount;
    nw = horiz ? b->w : (b->w + a);
    nh = horiz ? (b->h + a) : b->h;
    if (nw < 1 || nh < 1 || nw > 2000 || nh > 2000) {
        return;
    }
    tmp = (uint8_t *)malloc((size_t)nw * (size_t)nh);
    if (tmp == NULL) {
        return;
    }
    memset(tmp, b->trans, (size_t)nw * (size_t)nh);
    for (y = 0; y < b->h; y++) {
        for (x = 0; x < b->w; x++) {
            double t = horiz ? (b->w > 1 ? (double)x / (b->w - 1) : 0.5)
                             : (b->h > 1 ? (double)y / (b->h - 1) : 0.5);
            double arch = 4.0 * t * (1.0 - t);
            int shift = (int)floor((amount >= 0 ? arch : (1.0 - arch)) * a + 0.5);
            int dx = horiz ? x : x + shift;
            int dy = horiz ? y + shift : y;
            if (dx >= 0 && dx < nw && dy >= 0 && dy < nh) {
                tmp[dy * nw + dx] = b->pix[y * b->w + x];
            }
        }
    }
    free(b->pix);
    b->pix = tmp;
    b->w = nw;
    b->h = nh;
    b->hx = nw / 2;
    b->hy = nh / 2;
}

int brush_metamorph(Doc *d, int steps)
{
    int i, x, y;
    const DpBrush *src;
    const DpBrush *dst;
    if (d == NULL || d->brush.pix == NULL || !d->brush_spare_ok ||
        d->brush_spare.pix == NULL) {
        return -1;
    }
    if (steps < 2) {
        steps = 2;
    }
    if (steps > 16) {
        steps = 16;
    }
    for (i = 0; i < d->nabrush; i++) {
        brush_free(&d->abrush[i]);
    }
    d->nabrush = 0;
    d->abrush_i = 0;
    src = &d->brush;
    dst = &d->brush_spare;
    for (i = 0; i < steps; i++) {
        int num = i;
        int den = steps - 1;
        int cw = src->w + (dst->w - src->w) * num / den;
        int ch = src->h + (dst->h - src->h) * num / den;
        uint8_t *p;
        DpBrush *cel = &d->abrush[i];
        if (cw < 1) {
            cw = 1;
        }
        if (ch < 1) {
            ch = 1;
        }
        p = (uint8_t *)malloc((size_t)cw * (size_t)ch);
        if (p == NULL) {
            break;
        }
        for (y = 0; y < ch; y++) {
            int sy = y * src->h / ch;
            int dy = y * dst->h / ch;
            for (x = 0; x < cw; x++) {
                int sx = x * src->w / cw;
                int dx = x * dst->w / cw;
                uint8_t c0 = src->pix[sy * src->w + sx];
                uint8_t c1 = dst->pix[dy * dst->w + dx];
                int t0 = (c0 == src->trans);
                int t1 = (c1 == dst->trans);
                if (t0 && t1) {
                    p[y * cw + x] = src->trans;
                } else if (t0) {
                    p[y * cw + x] = (num * 2 >= den) ? c1 : src->trans;
                } else if (t1) {
                    p[y * cw + x] = (num * 2 < den) ? c0 : src->trans;
                } else {
                    int pct = num * 100 / den;
                    p[y * cw + x] = (uint8_t)blend_idx(d, c0, c1, pct);
                }
            }
        }
        memset(cel, 0, sizeof(*cel));
        cel->pix = p;
        cel->w = cw;
        cel->h = ch;
        cel->hx = cw / 2;
        cel->hy = ch / 2;
        cel->trans = src->trans;
        d->nabrush++;
    }
    return d->nabrush > 0 ? 0 : -1;
}

static int refit_side(Doc *d, int w, int h)
{
    frame_free(&d->spare);
    if (frame_alloc(&d->spare, w, h, (uint8_t)d->bg) != 0) {
        return -1;
    }
    undo_clear(d);
    free(d->stencil);
    d->stencil = NULL;
    d->stencil_on = 0;
    d->range_a = 0;
    d->range_b = d->nframes - 1;
    if (d->fg >= d->ncolors) {
        d->fg = d->ncolors - 1;
    }
    return 0;
}

static void take_palette(Doc *d, const DpColor *pal, int ncolors, int camg,
                         const DpRange *rg, int nr)
{
    int i;
    if (ncolors < 2) {
        ncolors = 2;
    }
    if (ncolors > DP_MAX_COLORS) {
        ncolors = DP_MAX_COLORS;
    }
    if (pal != NULL) {
        int copy = ncolors;
        if (copy > DP_MAX_COLORS) {
            copy = DP_MAX_COLORS;
        }
        for (i = 0; i < copy; i++) {
            d->pal[i] = pal[i];
        }
    }
    if (ncolors > d->ncolors) {
        d->ncolors = ncolors;
    }
    d->camg = camg;
    if (rg != NULL && nr > 0) {
        if (nr > DP_MAX_RANGES) {
            nr = DP_MAX_RANGES;
        }
        memcpy(d->ranges, rg, (size_t)nr * sizeof(DpRange));
        d->nranges = nr;
    }
}

int doc_adopt_frame(Doc *d, DpFrame *fr, const DpColor *pal, int ncolors,
                    int camg, const DpRange *rg, int nr)
{
    int i;
    if (fr == NULL || fr->pix == NULL) {
        return -1;
    }
    for (i = 0; i < d->nframes; i++) {
        frame_free(&d->frames[i]);
    }
    d->frames[0] = *fr;
    fr->pix = NULL;
    d->nframes = 1;
    d->cur = 0;
    d->play = 0;
    take_palette(d, pal, ncolors, camg, rg, nr);
    return refit_side(d, d->frames[0].w, d->frames[0].h);
}

int doc_adopt_anim(Doc *d, DpFrame *frames, int nframes, const DpColor *pal,
                   int ncolors, int camg, const DpRange *rg, int nr, int fps)
{
    int i;
    if (nframes < 1 || nframes > DP_MAX_FRAMES) {
        return -1;
    }
    for (i = 0; i < d->nframes; i++) {
        frame_free(&d->frames[i]);
    }
    for (i = 0; i < nframes; i++) {
        d->frames[i] = frames[i];
        frames[i].pix = NULL;
    }
    d->nframes = nframes;
    d->cur = 0;
    d->play = 0;
    if (fps > 0 && fps <= 60) {
        d->fps = fps;
    }
    take_palette(d, pal, ncolors, camg, rg, nr);
    return refit_side(d, d->frames[0].w, d->frames[0].h);
}

void doc_cycle_palette(Doc *d)
{
    int i;
    for (i = 0; i < d->nranges; i++) {
        DpRange *r = &d->ranges[i];
        DpColor tmp;
        int c;
        if (!r->active || r->high <= r->low) {
            continue;
        }
        if (r->low < 0 || r->high >= d->ncolors) {
            continue;
        }
        tmp = d->pal[r->high];
        for (c = r->high; c > r->low; c--) {
            d->pal[c] = d->pal[c - 1];
        }
        d->pal[r->low] = tmp;
    }
}

void doc_antialias_frame(Doc *d, int level)
{
    int w, h, x, y, pct;
    uint8_t *src, *dst;
    if (d == NULL || level <= 0) {
        return;
    }
    w = fw(d);
    h = fh(d);
    dst = fpix(d);
    src = (uint8_t *)malloc((size_t)w * (size_t)h);
    if (src == NULL) {
        return;
    }
    memcpy(src, dst, (size_t)w * (size_t)h);
    pct = level >= 2 ? 50 : 33;
    for (y = 1; y < h - 1; y++) {
        for (x = 1; x < w - 1; x++) {
            int i = y * w + x;
            uint8_t c = src[i];
            uint8_t u = src[(y - 1) * w + x];
            uint8_t dn = src[(y + 1) * w + x];
            uint8_t l = src[y * w + (x - 1)];
            uint8_t r = src[y * w + (x + 1)];
            if (d->stencil_on && d->stencil != NULL && d->stencil[i]) {
                continue;
            }
            if (u == l && u != c && src[(y - 1) * w + (x - 1)] != u) {
                dst[i] = (uint8_t)blend_idx(d, c, u, pct);
            } else if (u == r && u != c && src[(y - 1) * w + (x + 1)] != u) {
                dst[i] = (uint8_t)blend_idx(d, c, u, pct);
            } else if (dn == l && dn != c && src[(y + 1) * w + (x - 1)] != dn) {
                dst[i] = (uint8_t)blend_idx(d, c, dn, pct);
            } else if (dn == r && dn != c && src[(y + 1) * w + (x + 1)] != dn) {
                dst[i] = (uint8_t)blend_idx(d, c, dn, pct);
            }
        }
    }
    free(src);
}

void doc_fix_background(Doc *d)
{
    size_t n;
    uint8_t *p;
    if (d == NULL) {
        return;
    }
    n = (size_t)fw(d) * (size_t)fh(d);
    p = (uint8_t *)malloc(n);
    if (p == NULL) {
        return;
    }
    memcpy(p, fpix(d), n);
    free(d->bg_fixed);
    d->bg_fixed = p;
    d->bg_fixed_on = 1;
}

void doc_free_background(Doc *d)
{
    if (d == NULL) {
        return;
    }
    free(d->bg_fixed);
    d->bg_fixed = NULL;
    d->bg_fixed_on = 0;
}

void doc_lock_fg(Doc *d)
{
    int i, n;
    uint8_t *m;
    if (d == NULL) {
        return;
    }
    n = fw(d) * fh(d);
    m = (uint8_t *)malloc((size_t)n);
    if (m == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        uint8_t c = fpix(d)[i];
        if (d->bg_fixed_on && d->bg_fixed != NULL) {
            m[i] = (c != d->bg_fixed[i]) ? 1 : 0;
        } else {
            m[i] = (c != (uint8_t)d->bg) ? 1 : 0;
        }
    }
    free(d->stencil);
    d->stencil = m;
    d->stencil_on = 1;
}

void doc_remap_page(Doc *d)
{
    int i, n;
    uint8_t map[DP_MAX_COLORS];
    const DpColor *src;
    if (d == NULL) {
        return;
    }
    src = d->brush.has_pal ? d->brush.pal : d->pal_saved;
    for (i = 0; i < DP_MAX_COLORS; i++) {
        map[i] = (uint8_t)doc_nearest(d, src[i].r, src[i].g, src[i].b);
    }
    n = fw(d) * fh(d);
    for (i = 0; i < n; i++) {
        uint8_t c = fpix(d)[i];
        if (c < DP_MAX_COLORS) {
            fpix(d)[i] = map[c];
        }
    }
}

void doc_recompute_palette(Doc *d)
{
    int i;
    if (d == NULL) {
        return;
    }
    for (i = 0; i < d->nranges; i++) {
        if (d->ranges[i].high > d->ranges[i].low) {
            doc_pal_spread(d, d->ranges[i].low, d->ranges[i].high);
        }
    }
    memcpy(d->pal_saved, d->pal, sizeof(d->pal));
}
