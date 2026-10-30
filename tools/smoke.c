#define _GNU_SOURCE
#include "dpaint.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        fails++;
    }
}

static int files_equal(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    int c1, c2;
    if (fa == NULL || fb == NULL) {
        if (fa) {
            fclose(fa);
        }
        if (fb) {
            fclose(fb);
        }
        return 0;
    }
    do {
        c1 = fgetc(fa);
        c2 = fgetc(fb);
        if (c1 != c2) {
            fclose(fa);
            fclose(fb);
            return 0;
        }
    } while (c1 != EOF);
    fclose(fa);
    fclose(fb);
    return 1;
}

static int pix_same(const uint8_t *a, const uint8_t *b, int n)
{
    return memcmp(a, b, (size_t)n) == 0;
}

static int orange_at(const uint8_t *rgb, int w, int h, int x, int y)
{
    const uint8_t *p;
    if (x < 0 || y < 0 || x >= w || y >= h) {
        return 0;
    }
    p = rgb + ((y * w + x) * 3);
    return p[0] > 220 && p[1] > 40 && p[1] < 160 && p[2] < 40;
}

static int cross_centers(const uint8_t *rgb, int w, int h, int *xs, int *ys, int max)
{
    int n = 0;
    int y, x;
    for (y = 4; y < h - 4; y++) {
        for (x = 4; x < w - 4; x++) {
            int k, ok = 1;
            for (k = 1; k <= 4; k++) {
                if (!orange_at(rgb, w, h, x - k, y) || !orange_at(rgb, w, h, x + k, y) ||
                    !orange_at(rgb, w, h, x, y - k) || !orange_at(rgb, w, h, x, y + k)) {
                    ok = 0;
                    break;
                }
            }
            if (!ok || orange_at(rgb, w, h, x, y)) {
                continue;
            }
            if (n < max) {
                xs[n] = x;
                ys[n] = y;
            }
            n++;
        }
    }
    return n;
}

static int read_be32(FILE *fp, unsigned long *n)
{
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) {
        return -1;
    }
    *n = ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) |
         ((unsigned long)b[2] << 8) | (unsigned long)b[3];
    return 0;
}

static int walk_form(FILE *fp, unsigned long remain, int *w, int *h, int *got, int *ilbms)
{
    while (remain >= 8) {
        unsigned char id[4];
        unsigned long sz;
        if (fread(id, 1, 4, fp) != 4 || read_be32(fp, &sz) != 0) {
            return -1;
        }
        remain -= 8;
        if (sz > remain) {
            return -1;
        }
        if (memcmp(id, "FORM", 4) == 0) {
            unsigned char typ[4];
            if (sz < 4 || fread(typ, 1, 4, fp) != 4) {
                return -1;
            }
            if (memcmp(typ, "ILBM", 4) == 0) {
                (*ilbms)++;
            }
            if (walk_form(fp, sz - 4, w, h, got, ilbms) != 0) {
                return -1;
            }
        } else if (memcmp(id, "BMHD", 4) == 0 && sz >= 4 && !*got) {
            unsigned char b[4];
            if (fread(b, 1, 4, fp) != 4) {
                return -1;
            }
            *w = ((int)b[0] << 8) | (int)b[1];
            *h = ((int)b[2] << 8) | (int)b[3];
            *got = 1;
            if (sz > 4 && fseek(fp, (long)(sz - 4), SEEK_CUR) != 0) {
                return -1;
            }
        } else if (fseek(fp, (long)sz, SEEK_CUR) != 0) {
            return -1;
        }
        if (sz & 1UL) {
            if (fseek(fp, 1, SEEK_CUR) != 0) {
                return -1;
            }
            remain--;
        }
        remain -= sz;
    }
    return 0;
}

static int iff_header(const char *path, int *w, int *h, int *ilbms)
{
    FILE *fp = fopen(path, "rb");
    unsigned char id[8];
    unsigned long sz = 0;
    int got = 0;
    *w = 0;
    *h = 0;
    *ilbms = 0;
    if (fp == NULL) {
        return -1;
    }
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "FORM", 4) != 0 ||
        read_be32(fp, &sz) != 0 || fread(id, 1, 4, fp) != 4) {
        fclose(fp);
        return -1;
    }
    if (memcmp(id, "ILBM", 4) == 0) {
        *ilbms = 1;
    }
    if (sz < 4 || walk_form(fp, sz - 4, w, h, &got, ilbms) != 0 || !got) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int main(void)
{
    DpAssets assets;
    Doc doc;
    Ui ui;
    char dir[] = "/tmp/dpaint-smoke-XXXXXX";
    char p1[256], p2[256], p3[256], p4[256], p5[256], p6[256], p7[256], ppm[256];
    uint8_t *rgb;
    int i, graybar = 0;
    int nabout = 0;
    const char *const *about;
    DpFrame loaded;
    DpColor pal[DP_MAX_COLORS];
    DpRange rg[DP_MAX_RANGES];
    int nc = 0, camg = 0, nr = 0, fps = 0, nframes = 0;
    char err[120];
    DpFrame frames[8];
    uint8_t *mask = NULL;
    int mw = 0, mh = 0;
    uint8_t dpps[DP_DPPS_LEN];
    uint8_t *snap;
    if (mkdtemp(dir) == NULL) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    snprintf(p1, sizeof(p1), "%s/pic.ilbm", dir);
    snprintf(p2, sizeof(p2), "%s/pic2.ilbm", dir);
    snprintf(p3, sizeof(p3), "%s/anim.iff", dir);
    snprintf(p4, sizeof(p4), "%s/anim2.iff", dir);
    snprintf(p5, sizeof(p5), "%s/view.prsp", dir);
    snprintf(p6, sizeof(p6), "%s/mask.ilbm", dir);
    snprintf(p7, sizeof(p7), "%s/circle.ilbm", dir);
    snprintf(ppm, sizeof(ppm), "%s/ui.ppm", dir);

    expect(dp_assets_load(&assets, NULL) == 0, "font load");
    expect(assets.nfonts >= 1, "DPaint font");
    expect(assets.fonts[0].ysize == 8, "font size 8");
    {
        uint8_t cell[16 * 16];
        int ink = 0;
        int ci;
        DpFont picked;
        memset(cell, 0, sizeof(cell));
        dp_font_index(&assets.fonts[0], cell, 16, 16, 0, 0, "A", 1);
        for (ci = 0; ci < 16 * 16; ci++) {
            if (cell[ci] == 1) {
                ink++;
            }
        }
        expect(ink > 4, "baked font draws A");
        expect(dp_font_load(&picked, "/no/such/dpaint-font") != 0,
               "a missing font file is not treated as built in");
    }
    expect(assets.nicons >= 29, "tool bitplanes");
    expect(assets.icon[2][0][0] == 1 && assets.icon[2][5][5] == 0, "line icon is the bitplane");
    expect(assets.icon[11][2][4] == 0 && assets.icon[16][1][1] == 0, "text and undo bitplanes");
    expect(dp_font_width(&assets.fonts[0], "Picture") > 20, "Picture width");
    expect(dp_font_width(&assets.fonts[0], "Picture") < 90, "Picture not doubled");
    expect(assets.pal_ok, "default palette");
    expect(assets.pal[0].r == 0 && assets.pal[31].r == 255, "palette ends");
    expect(assets.nicons >= 20, "tool icons");
    expect(assets.icon[2][5][3] == 1 || assets.icon[2][6][4] == 1, "line icon");

    about = dp_about_lines(&nabout);
    expect(nabout == 4, "about count");
    expect(strstr(about[0], "Daniel Silva") != NULL, "Silva");
    expect(strstr(about[1], "Lee Taran") != NULL, "Taran");
    expect(strstr(about[1], "Steve Shaw") != NULL, "Shaw");
    expect(strstr(about[2], "1985-1991") != NULL, "copyright");
    expect(strcmp(about[3], "About Deluxe Paint IV") == 0, "about title");

    expect(doc_init(&doc, 48, 40, 32, assets.pal) == 0, "doc");
    doc.fg = 31;
    doc.bg = 0;
    doc_plot(&doc, 2, 2, 31);
    doc_plot(&doc, 4, 2, 31);
    doc_plot(&doc, 6, 3, 2);
    doc_line(&doc, 1, 10, 20, 18, 4);
    doc_rect(&doc, 24, 4, 40, 16, 5, 1);
    doc_rect(&doc, 8, 22, 20, 34, 1, 0);
    doc_fill(&doc, 12, 26, 6);
    expect(doc.frames[0].pix[2 * 48 + 2] == 31, "dot");
    expect(doc.frames[0].pix[10 * 48 + 1] == 4, "line start");
    expect(doc.frames[0].pix[18 * 48 + 20] == 4, "line end");
    expect(doc.frames[0].pix[8 * 48 + 30] == 5, "filled rect");
    expect(doc.frames[0].pix[26 * 48 + 12] == 6, "fill inside");
    expect(doc.frames[0].pix[22 * 48 + 8] == 1, "fill border");
    expect(doc.frames[0].pix[0] == 0, "outside fill");

    snap = (uint8_t *)malloc(48 * 40);
    memcpy(snap, doc.frames[0].pix, 48 * 40);
    expect(doc_add_frames(&doc, 1, 0) == 0, "add frame");
    expect(doc.nframes == 2 && doc.cur == 1, "on new frame");
    doc_plot(&doc, 3, 3, 7);
    expect(doc.frames[1].pix[3 * 48 + 3] == 7, "frame 2 pixel");
    expect(doc.frames[0].pix[3 * 48 + 3] == snap[3 * 48 + 3], "frame 1 stable");

    expect(iff_save_ilbm(p1, &doc.frames[0], doc.pal, doc.ncolors, 0,
                         doc.ranges, doc.nranges, 0, 0, 0, 0, 1, 0) == 0, "save ilbm");
    expect(iff_save_ilbm(p2, &doc.frames[0], doc.pal, doc.ncolors, 0,
                         doc.ranges, doc.nranges, 0, 0, 0, 0, 1, 0) == 0, "save ilbm 2");
    expect(files_equal(p1, p2), "ilbm bytes");
    memset(&loaded, 0, sizeof(loaded));
    err[0] = 0;
    expect(iff_load_ilbm(p1, &loaded, pal, &nc, &camg, rg, &nr,
                         NULL, NULL, NULL, NULL, err, 120) == 0, "load ilbm");
    expect(loaded.w == 48 && loaded.h == 40, "ilbm size");
    expect(pix_same(loaded.pix, doc.frames[0].pix, 48 * 40), "ilbm pixels");
    expect(nc == 32 && pal[31].r == doc.pal[31].r, "ilbm cmap");
    free(loaded.pix);
    loaded.pix = NULL;

    expect(iff_save_ilbm(p2, &doc.frames[0], doc.pal, doc.ncolors, 0,
                         NULL, 0, 0, 0, 0, 0, 0, 0) == 0, "save raw");
    expect(iff_load_ilbm(p2, &loaded, pal, &nc, &camg, rg, &nr,
                         NULL, NULL, NULL, NULL, err, 120) == 0, "load raw");
    expect(pix_same(loaded.pix, doc.frames[0].pix, 48 * 40), "raw pixels");
    free(loaded.pix);
    loaded.pix = NULL;

    {
        int grab = 0, gx = -1, gy = -1, trans = -1;
        expect(iff_save_ilbm(p2, &doc.frames[0], doc.pal, doc.ncolors, 0,
                             NULL, 0, 1, 3, 7, 0, 1, 0) == 0, "save brush");
        expect(iff_load_ilbm(p2, &loaded, pal, &nc, &camg, rg, &nr,
                             &grab, &gx, &gy, &trans, err, 120) == 0, "load brush");
        expect(grab == 1 && gx == 3 && gy == 7 && trans == 0, "grab");
        expect(pix_same(loaded.pix, doc.frames[0].pix, 48 * 40), "brush pixels");
        free(loaded.pix);
        loaded.pix = NULL;
    }

    expect(iff_save_anim(p3, doc.frames, doc.nframes, doc.pal, doc.ncolors, 0,
                         doc.ranges, doc.nranges, 12, 1, 0, 0, 0, 0) == 0, "save anim");
    expect(iff_save_anim(p4, doc.frames, doc.nframes, doc.pal, doc.ncolors, 0,
                         doc.ranges, doc.nranges, 12, 1, 0, 0, 0, 0) == 0, "save anim 2");
    expect(files_equal(p3, p4), "anim bytes");
    memset(frames, 0, sizeof(frames));
    expect(iff_load_anim(p3, frames, 8, &nframes, pal, &nc, &camg, rg, &nr, &fps,
                         err, 120) == 0, "load anim");
    expect(nframes == 2, "anim frames");
    expect(frames[0].w == 48 && pix_same(frames[0].pix, doc.frames[0].pix, 48 * 40),
           "anim frame 0");
    expect(pix_same(frames[1].pix, doc.frames[1].pix, 48 * 40), "anim frame 1");
    expect(fps == 12, "anim fps");
    free(frames[0].pix);
    free(frames[1].pix);

    doc.method_compressed = 0;
    expect(iff_save_anim(p4, doc.frames, 2, doc.pal, doc.ncolors, 0,
                         NULL, 0, 10, 0, 0, 0, 0, 0) == 0, "save expanded");
    memset(frames, 0, sizeof(frames));
    expect(iff_load_anim(p4, frames, 8, &nframes, pal, &nc, &camg, rg, &nr, &fps,
                         err, 120) == 0, "load expanded");
    expect(nframes == 2 && pix_same(frames[1].pix, doc.frames[1].pix, 48 * 40),
           "expanded pixels");
    free(frames[0].pix);
    free(frames[1].pix);

    memset(doc.dpps, 0, sizeof(doc.dpps));
    doc.dpps[0] = 0;
    doc.dpps[1] = 1;
    doc.dpps[4] = 9;
    doc.dpps[109] = 42;
    expect(iff_save_prsp(p5, doc.dpps) == 0, "save prsp");
    memset(dpps, 0, sizeof(dpps));
    expect(iff_load_prsp(p5, dpps, err, 120) == 0, "load prsp");
    expect(memcmp(dpps, doc.dpps, DP_DPPS_LEN) == 0, "prsp bytes");

    {
        uint8_t m[48 * 40];
        memset(m, 0, sizeof(m));
        m[10] = 1;
        m[11] = 1;
        m[100] = 1;
        expect(iff_save_stencil(p6, m, 48, 40) == 0, "save stencil");
        expect(iff_load_stencil(p6, &mask, &mw, &mh, err, 120) == 0, "load stencil");
        expect(mw == 48 && mh == 40 && mask[10] == 1 && mask[0] == 0, "stencil bits");
        free(mask);
    }

    ui_init(&ui);
    ui.win_w = 800;
    ui.win_h = 360;
    ui.scale = 1;
    rgb = (uint8_t *)malloc(800 * 360 * 3);
    expect(rgb != NULL, "ppm buf");
    if (rgb != NULL) {
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        expect(dp_write_ppm(ppm, rgb, 800, 360) == 0, "ppm");
        for (i = 0; i < 800 * 10; i++) {
            int r = rgb[i * 3];
            int g = rgb[i * 3 + 1];
            int b = rgb[i * 3 + 2];
            int dg = r - g;
            int db = r - b;
            if (dg < 0) {
                dg = -dg;
            }
            if (db < 0) {
                db = -db;
            }
            if (r > 140 && r < 200 && dg < 12 && db < 12) {
                graybar++;
            }
        }
        expect(graybar > 100, "gray title bar");
        {
            const DpFont *face = &assets.fonts[0];
            int ntitles = ui_menu_count();
            int ti;
            int prev_right = -1;
            static const char *expect_titles[] = {
                "Picture", "Brush", "Mode", "Anim", "Color", "Effect", "Font", "Prefs"
            };
            expect(ntitles == 8, "eight menus");
            for (ti = 0; ti < ntitles; ti++) {
                const char *title = ui_menu_title(ti);
                int xs[40], ws[40];
                int ng, g;
                int tx = ui_menu_title_x(face, ti);
                expect(title != NULL && strcmp(title, expect_titles[ti]) == 0, "title name");
                ng = dp_font_glyphs(face, title, tx, xs, ws, 40);
                expect(ng == (int)strlen(title), "glyph count");
                for (g = 1; g < ng; g++) {
                    expect(xs[g] >= xs[g - 1] + 1, "glyph x increases");
                }
                if (ti > 0) {
                    expect(xs[0] >= prev_right + 1, "title separated");
                }
                prev_right = xs[ng - 1] + ws[ng - 1];
            }
            {
                uint8_t *bar = (uint8_t *)calloc((size_t)480 * 16 * 3, 1);
                expect(bar != NULL, "title bar buf");
                if (bar != NULL) {
                    for (ti = 0; ti < ntitles; ti++) {
                        int tx = ui_menu_title_x(face, ti);
                        dp_font_rgb(face, bar, 480, 16, tx, 1, ui_menu_title(ti),
                                    255, 255, 255, 1);
                    }
                    for (ti = 0; ti < ntitles; ti++) {
                        int xs[40], ws[40];
                        int tx = ui_menu_title_x(face, ti);
                        int ng = dp_font_glyphs(face, ui_menu_title(ti), tx, xs, ws, 40);
                        int left = xs[0];
                        int right = xs[ng - 1] + ws[ng - 1];
                        int next = ti + 1 < ntitles ? ui_menu_title_x(face, ti + 1) : right + 4;
                        int ink = 0, gapink = 0, yy, xx;
                        for (yy = 0; yy < 16; yy++) {
                            for (xx = left; xx < right && xx < 480; xx++) {
                                if (xx >= 0 && bar[(yy * 480 + xx) * 3] > 200) {
                                    ink++;
                                }
                            }
                            for (xx = right; xx < next && xx < 480; xx++) {
                                if (xx >= 0 && bar[(yy * 480 + xx) * 3] > 200) {
                                    gapink++;
                                }
                            }
                        }
                        expect(ink > 8, "title ink");
                        expect(gapink == 0, "no title overlap");
                    }
                    free(bar);
                }
            }
            ui.menu_open = 3;
            ui.menu_item = -1;
            ui.menu_sub = -1;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            {
                int mx, my, mw, mh;
                expect(ui_menu_box(&ui, face, &mx, &my, &mw, &mh) == 0, "anim column");
                expect(mh == 60 && mw > 40 && mw < 220, "one column");
                expect(ui_menu_subbox(&ui, face, &mx, &my, &mw, &mh) != 0, "submenu closed");
                expect(rgb[((my + 4) * 800 + 8) * 3] < 40, "page not a white slab");
            }
            ui.menu_item = 4;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            {
                int mx, my, mw, mh, sx, sy, sw, sh;
                expect(ui_menu_box(&ui, face, &mx, &my, &mw, &mh) == 0, "anim box");
                expect(ui_menu_subbox(&ui, face, &sx, &sy, &sw, &sh) == 0, "control column");
                expect(sx >= mx + mw, "submenu does not overlap");
                expect(rgb[(my * 800 + mx) * 3] > 200, "menu white");
                expect(rgb[(sy * 800 + sx) * 3] > 200, "submenu white");
            }
            expect(dp_write_ppm(ppm, rgb, 800, 360) == 0, "ppm menu");
        }
        ui.menu_open = -1;
        ui.menu_item = -1;
        ui.menu_sub = -1;
        ui.prompt = 0;
        ui.pointer_seen = 0;
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        {
            int px, py, pw, ph;
            int mh2, tw2, bh2, vw2, vh2;
            int di;
            DpColor paper = doc.pal[doc.bg];
            ui_layout(&ui, &mh2, &tw2, &bh2, &vw2, &vh2);
            expect(ui_palette_box(&ui, doc.ncolors, &px, &py, &pw, &ph) == 0, "palette box");
            expect(bh2 == 0, "no bottom strip");
            expect(px >= vw2, "palette in the right column");
            expect(px + pw <= 800, "palette inside the window");
            expect(pw <= tw2, "palette fits the tool column");
            expect(pw < vw2 / 2, "palette is not full width");
            expect(py > mh2 + 40, "palette sits under the tools");
            expect(ph >= 16 && pw >= 16, "palette block");
            di = ((py + ph / 2) * 800 + 8) * 3;
            expect(rgb[di] == paper.r && rgb[di + 1] == paper.g && rgb[di + 2] == paper.b,
                   "left side under the tools is page");
            di = (350 * 800 + 4) * 3;
            expect(rgb[di] == paper.r && rgb[di + 1] == paper.g && rgb[di + 2] == paper.b,
                   "page reaches the bottom");
            di = ((py + 2) * 800 + px + 2) * 3;
            expect(px > 700, "palette x");
            (void)di;
        }
        expect(dp_write_ppm("/tmp/dpaint-ui.ppm", rgb, 800, 360) == 0, "ui ppm");
        free(rgb);
    }
    doc_clear(&doc);
    doc.fg = 31;
    doc.bg = 0;
    doc.mode = MODE_MATTE;
    doc.tool = TOOL_CIRCLE;
    ui.prompt = 0;
    ui.menu_open = -1;
    ui.menu_hold = 0;
    ui.gesture = GS_NONE;
    ui.stroking = 0;
    ui.scroll_x = 0;
    ui.scroll_y = 0;
    ui.scale = 1;
    ui_mouse(&ui, &doc, &assets, 20, 26, 1, 1);
    ui_motion(&ui, &doc, &assets, 32, 26, 1);
    ui_mouse(&ui, &doc, &assets, 32, 26, 1, 0);
    {
        int x, y, on = 0, off = 0, rim = 0;
        uint8_t *pix = doc.frames[doc.cur].pix;
        int fw = doc.frames[doc.cur].w;
        for (y = 0; y < doc.frames[doc.cur].h; y++) {
            for (x = 0; x < fw; x++) {
                int dx, dy, rr;
                if (pix[y * fw + x] != 31) {
                    continue;
                }
                if (x == 20 || y == 16) {
                    on++;
                } else {
                    off++;
                }
                dx = x - 20;
                dy = y - 16;
                rr = dx * dx + dy * dy;
                if (rr >= 10 * 10 && rr <= 14 * 14) {
                    rim++;
                }
            }
        }
        expect(off > 8, "circle has off-axis pixels");
        expect(rim > 8, "circle rim");
        expect(on > 0, "circle meets the axes");
        expect(pix[16 * fw + 20] != 31, "circle center is empty");
        snap = (uint8_t *)realloc(snap, (size_t)fw * (size_t)doc.frames[doc.cur].h);
        memcpy(snap, pix, (size_t)fw * (size_t)doc.frames[doc.cur].h);
    }
    ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
    expect(ui.menu_hold == 1 && ui.menu_open == 0, "right button opens Picture");
    ui_motion(&ui, &doc, &assets, 16, 24, 4);
    ui_mouse(&ui, &doc, &assets, 16, 24, 3, 0);
    expect(ui.prompt != 0 && ui.menu_hold == 0, "save requester");
    {
        const char *s = p7;
        while (*s) {
            ui_key(&ui, &doc, &assets, *s, *s);
            s++;
        }
    }
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    expect(ui.prompt == 0, "save returned");
    {
        char pngpath[256];
        FILE *pf;
        unsigned char mag[8];
        snprintf(pngpath, sizeof(pngpath), "%s/circle.png", dir);
        pf = fopen(pngpath, "rb");
        expect(pf != NULL, "png sibling");
        if (pf != NULL) {
            expect(fread(mag, 1, 8, pf) == 8, "png header");
            expect(mag[0] == 137 && mag[1] == 'P' && mag[2] == 'N' && mag[3] == 'G',
                   "png magic");
            fclose(pf);
        }
    }
    doc_clear(&doc);
    expect(doc.frames[doc.cur].pix[16 * doc.frames[doc.cur].w + 20] == 0, "cleared");
    ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
    ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
    expect(ui.prompt != 0, "load requester");
    expect(strcmp(ui.dir, dir) == 0, "dialog remembers save directory");
    {
        const char *s = p7;
        while (*s) {
            ui_key(&ui, &doc, &assets, *s, *s);
            s++;
        }
    }
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    expect(ui.prompt == 0, "load returned");
    expect(doc.frames[doc.cur].w == 48, "loaded width");
    expect(memcmp(doc.frames[doc.cur].pix, snap, 48 * 40) == 0, "loaded circle pixels");
    ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
    ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
    expect(ui.prompt != 0, "load again");
    ui_key(&ui, &doc, &assets, KEY_ESC, 0);
    expect(ui.prompt == 0, "esc closes requester");
    ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
    ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
    expect(ui.prompt != 0, "load for cancel");
    {
        int bx = 0, by = 0;
        expect(ui_prompt_button(&ui, 1, &bx, &by) == 0, "cancel button");
        ui_mouse(&ui, &doc, &assets, bx, by, 1, 1);
    }
    expect(ui.prompt == 0, "cancel closes requester");
    doc.tool = TOOL_DRAW;
    ui_mouse(&ui, &doc, &assets, 4, 14, 1, 1);
    ui_mouse(&ui, &doc, &assets, 4, 14, 1, 0);
    expect(doc.frames[doc.cur].pix[4 * doc.frames[doc.cur].w + 4] == 31, "draw after cancel");
    doc_clear(&doc);
    doc.fg = 31;
    doc.tool = TOOL_LINE;
    doc.brush_d = 1;
    ui.prompt = 0;
    ui.menu_open = -1;
    ui.magnify = 0;
    ui_mouse(&ui, &doc, &assets, 8, 28, 1, 1);
    ui_motion(&ui, &doc, &assets, 24, 28, 1);
    ui_mouse(&ui, &doc, &assets, 24, 28, 1, 0);
    {
        int x, y, thin = 0;
        uint8_t *pix = doc.frames[doc.cur].pix;
        int fw = doc.frames[doc.cur].w;
        int fh = doc.frames[doc.cur].h;
        for (y = 0; y < fh; y++) {
            for (x = 0; x < fw; x++) {
                if (pix[y * fw + x] == 31) {
                    thin++;
                }
            }
        }
        doc_clear(&doc);
        ui_mouse(&ui, &doc, &assets, 778, 26, 1, 1);
        expect(doc.brush_d == 5, "built-in brush");
        ui_mouse(&ui, &doc, &assets, 8, 28, 1, 1);
        ui_motion(&ui, &doc, &assets, 24, 28, 1);
        ui_mouse(&ui, &doc, &assets, 24, 28, 1, 0);
        {
            int thick = 0;
            pix = doc.frames[doc.cur].pix;
            for (y = 0; y < fh; y++) {
                for (x = 0; x < fw; x++) {
                    if (pix[y * fw + x] == 31) {
                        thick++;
                    }
                }
            }
            expect(thin > 0 && thick > thin, "brush size 5 is thicker than size 1");
        }
        {
            int sz, prev = 0;
            int grew = 1;
            for (sz = 1; sz <= 8; sz++) {
                int n = 0;
                int dx, dy;
                for (dy = -6; dy <= 6; dy++) {
                    for (dx = -6; dx <= 6; dx++) {
                        if (doc_brush_covers(sz, dx, dy)) {
                            n++;
                        }
                    }
                }
                if (n <= prev) {
                    grew = 0;
                }
                prev = n;
            }
            expect(grew, "eight brush sizes keep growing");
        }
    }
    doc_clear(&doc);
    doc.tool = TOOL_DRAW;
    ui_mouse(&ui, &doc, &assets, 10, 40, 1, 1);
    ui_mouse(&ui, &doc, &assets, 10, 40, 1, 0);
    {
        int x, y, thick = 0;
        uint8_t *pix = doc.frames[doc.cur].pix;
        int fw = doc.frames[doc.cur].w;
        for (y = 28; y <= 32; y++) {
            for (x = 8; x <= 12; x++) {
                if (y >= 0 && x >= 0 && pix[y * fw + x] == 31) {
                    thick++;
                }
            }
        }
        expect(thick > 1, "brush is thicker than one pixel");
    }
    ui_mouse(&ui, &doc, &assets, 780, 51, 1, 1);
    expect(doc.tool == TOOL_LINE, "line tool selected");
    ui_mouse(&ui, &doc, &assets, 780, 129, 1, 1);
    expect(ui.magnify == 0 && ui.mag_arm == 1, "magnify armed");
    expect(ui.mag_x1 > ui.mag_x0 && ui.mag_y1 > ui.mag_y0, "zoom rectangle waiting");
    {
        int parked = ui.mag_x0;
        ui_motion(&ui, &doc, &assets, 8, 18, 0);
        expect(ui.magnify == 0, "motion does not split");
        expect(ui.mag_x0 != parked, "zoom rectangle follows the pointer");
    }
    rgb = (uint8_t *)malloc(800 * 360 * 3);
    expect(rgb != NULL, "magnify buf");
    if (rgb != NULL) {
        int mh2, tw2, bh2, vw, vh, left_w, gap = 4;
        int rw, rect_w, rect_h, block, bh;
        int ox, oy, mx, my;
        int px = 10, py = 30;
        int lsx, lsy, li;
        int wx, wy, wi;
        int mark, plain;
        DpColor ink = doc.pal[31];
        ui_layout(&ui, &mh2, &tw2, &bh2, &vw, &vh);
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        mark = (51 * 800 + 780) * 3;
        doc.tool = TOOL_DRAW;
        ui.mag_arm = 0;
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        plain = rgb[mark];
        doc.tool = TOOL_LINE;
        ui.mag_arm = 1;
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        expect(rgb[mark] != plain, "active tool cell is marked");
        {
            int ink_n = 0, fill_n = 0, sx, sy;
            for (sy = 46; sy < 57; sy++) {
                for (sx = 775; sx < 786; sx++) {
                    int pi = (sy * 800 + sx) * 3;
                    if (rgb[pi] == 255 && rgb[pi + 1] == 255 && rgb[pi + 2] == 255) {
                        ink_n++;
                    }
                    if (rgb[pi] == 85 && rgb[pi + 1] == 85 && rgb[pi + 2] == 85) {
                        fill_n++;
                    }
                }
            }
            expect(ink_n > 3 && ink_n < fill_n, "line glyph is the thin bitplane");
        }
        wx = ui.mag_x0;
        wy = mh2 + ui.mag_y0;
        wi = (wy * 800 + wx) * 3;
        expect(rgb[wi] == 255 && rgb[wi + 1] == 255 && rgb[wi + 2] == 255,
               "first zoom click draws a white rectangle");
        expect(ui.magnify == 0, "first zoom click does not split");
        ui_mouse(&ui, &doc, &assets, 8, 18, 1, 1);
        expect(ui.magnify == 1 && ui.mag_arm == 0, "second click splits");
        ox = ui.mag_x0;
        oy = ui.mag_y0;
        expect(ox == 4 && oy == 4, "zoom locks on the pointer");
        doc.tool = TOOL_DRAW;
        left_w = vw / 2;
        rw = vw - left_w - gap;
        rect_w = ui.mag_x1 - ui.mag_x0 + 1;
        rect_h = ui.mag_y1 - ui.mag_y0 + 1;
        block = rw / rect_w;
        bh = vh / rect_h;
        if (bh < block) {
            block = bh;
        }
        if (block < 1) {
            block = 1;
        }
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        lsx = px * left_w / doc.frames[doc.cur].w;
        lsy = mh2 + py * vh / doc.frames[doc.cur].h;
        li = (lsy * 800 + lsx) * 3;
        expect(rgb[li] == ink.r && rgb[li + 1] == ink.g && rgb[li + 2] == ink.b,
               "normal pane shows the dab");
        mx = left_w + gap + block / 2;
        my = mh2 + block / 2;
        ui_mouse(&ui, &doc, &assets, mx, my, 1, 1);
        ui_motion(&ui, &doc, &assets, mx + block, my, 1);
        ui_mouse(&ui, &doc, &assets, mx + block, my, 1, 0);
        expect(ui.mag_x0 == ox && ui.mag_y0 == oy, "zoom origin stays fixed");
        expect(ui.mag_x1 == ox + rect_w - 1 && ui.mag_y1 == oy + rect_h - 1,
               "zoom rectangle stays fixed");
        expect(doc.frames[doc.cur].pix[oy * doc.frames[doc.cur].w + ox] == 31,
               "magnified pane paints the locked pixels");
        expect(dp_write_ppm("/tmp/dpaint-mag.ppm", rgb, 800, 360) == 0, "mag ppm");
        free(rgb);
    }
    ui.magnify = 0;
    doc.brush_d = 1;
    ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
    ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
    expect(ui.prompt != 0, "load listing");
    snprintf(ui.dir, sizeof(ui.dir), "%s", dir);
    ui.list_scroll = 0;
    ui.list_sel = -1;
    doc_clear(&doc);
    expect(ui_prompt_pick(&ui, "circle.ilbm") == 0, "pick listed ilbm");
    expect(strcmp(ui.text, "circle.ilbm") == 0, "picked name");
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    expect(ui.prompt == 0, "listed load returned");
    expect(doc.frames[doc.cur].w == 48, "listed width");
    expect(memcmp(doc.frames[doc.cur].pix, snap, 48 * 40) == 0, "listed pixels");
    {
        char sub[512];
        uint8_t pix0;
        int bx = 0, by = 0;
        snprintf(sub, sizeof(sub), "%s/drawer", dir);
        expect(mkdir(sub, 0755) == 0 || errno == EEXIST, "make drawer");
        ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
        ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
        expect(ui.prompt != 0, "load for directory");
        expect(strcmp(ui.dir, dir) == 0, "next dialog starts in saved directory");
        expect(ui_prompt_pick(&ui, "drawer") == 0, "select directory");
        pix0 = doc.frames[doc.cur].pix[0];
        ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
        expect(ui.prompt != 0, "enter directory stays open");
        expect(strcmp(ui.dir, sub) == 0, "enter lists the directory");
        expect(doc.frames[doc.cur].pix[0] == pix0, "directory was not loaded");
        ui_key(&ui, &doc, &assets, KEY_DOWN, 0);
        ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
        expect(ui.prompt != 0, "parent stays open");
        expect(strcmp(ui.dir, dir) == 0, "enter on parent goes up");
        expect(ui_prompt_pick(&ui, "drawer") == 0, "select directory again");
        expect(ui_prompt_button(&ui, 0, &bx, &by) == 0, "ok button");
        ui_mouse(&ui, &doc, &assets, bx, by, 1, 1);
        expect(ui.prompt != 0, "ok on directory stays open");
        expect(strcmp(ui.dir, sub) == 0, "ok lists the directory");
        expect(doc.frames[doc.cur].pix[0] == pix0, "ok did not load the directory");
        ui_key(&ui, &doc, &assets, KEY_ESC, 0);
        expect(ui.prompt == 0, "esc closes after browse");
        ui_mouse(&ui, &doc, &assets, 8, 4, 3, 1);
        ui_mouse(&ui, &doc, &assets, 16, 14, 3, 0);
        expect(ui.prompt != 0, "load after esc");
        expect(strcmp(ui.dir, dir) == 0, "esc keeps the remembered directory");
        ui_key(&ui, &doc, &assets, KEY_ESC, 0);
        doc.tool = TOOL_DRAW;
        doc.fg = 4;
        ui_mouse(&ui, &doc, &assets, 6, 16, 1, 1);
        ui_mouse(&ui, &doc, &assets, 6, 16, 1, 0);
        expect(doc.frames[doc.cur].pix[6 * doc.frames[doc.cur].w + 6] == 4,
               "draw after directory dialog");
    }
    {
        const char *anim = "/home/scp1/dev/dpaintiv/animations/IFF/idoler";
        int hw = 0, hh = 0, hilbm = 0;
        DpFrame big[DP_MAX_FRAMES];
        int k;
        expect(iff_header(anim, &hw, &hh, &hilbm) == 0, "idoler header");
        memset(big, 0, sizeof(big));
        err[0] = 0;
        expect(iff_load_anim(anim, big, DP_MAX_FRAMES, &nframes, pal, &nc, &camg,
                             rg, &nr, &fps, err, 120) == 0, "load idoler");
        expect(nframes == hilbm && nframes > 1, "idoler frame count");
        expect(big[0].w == hw && big[0].h == hh && hw == 320 && hh == 256,
               "idoler size");
        for (k = 0; k < nframes && k < DP_MAX_FRAMES; k++) {
            free(big[k].pix);
        }
    }
    expect(ui.os_cursor_hidden == 1, "os cursor hidden");
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_DRAW;
    doc.brush_d = 1;
    doc.mode = MODE_COLOR;
    ui.prompt = 0;
    ui.menu_open = -1;
    ui.menu_hold = 0;
    ui.gesture = GS_NONE;
    ui.stroking = 0;
    ui.magnify = 0;
    ui.scroll_x = 0;
    ui.scroll_y = 0;
    ui.pointer_seen = 0;
    ui.over_page = 0;
    ui.scale = 2;
    ui.win_w = 1280;
    ui.win_h = 800;
    ui_mouse(&ui, &doc, &assets, 46, 52, 1, 1);
    ui_motion(&ui, &doc, &assets, 54, 52, 1);
    ui_mouse(&ui, &doc, &assets, 54, 52, 1, 0);
    expect(doc.frames[doc.cur].pix[16 * doc.frames[doc.cur].w + 23] == 4,
           "window click paints");
    expect(doc.frames[doc.cur].pix[16 * doc.frames[doc.cur].w + 27] == 4,
           "window drag paints");
    ui.scale = 1;
    ui.win_w = 800;
    ui.win_h = 360;
    rgb = (uint8_t *)malloc(800 * 360 * 3);
    expect(rgb != NULL, "cursor buf");
    if (rgb != NULL) {
        int xs[8], ys[8], ncross;
        ui.pointer_seen = 0;
        ui.over_page = 0;
        ui.magnify = 0;
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        ncross = cross_centers(rgb, 800, 360, xs, ys, 8);
        expect(ncross == 0, "no stale crosshair");
        ui_motion(&ui, &doc, &assets, 20, 30, 0);
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        ncross = cross_centers(rgb, 800, 360, xs, ys, 8);
        expect(ncross == 1 && xs[0] == 20 && ys[0] == 30, "one cross at the pointer");
        ui.magnify = 1;
        ui.mag_x = 10;
        ui.mag_y = 10;
        ui_motion(&ui, &doc, &assets, 400, 80, 0);
        ui_render(&ui, &doc, &assets, rgb, 800, 360);
        ncross = cross_centers(rgb, 800, 360, xs, ys, 8);
        expect(ncross == 1 && xs[0] == 400 && ys[0] == 80, "magnify cross stays on the pointer");
        expect(dp_write_ppm("/tmp/dpaint-cursor.ppm", rgb, 800, 360) == 0, "cursor ppm");
        free(rgb);
    }
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_DRAW;
    doc.brush_d = 1;
    doc.mode = MODE_COLOR;
    doc.grid_on = 1;
    doc.sym_on = 0;
    ui.scale = 1;
    ui.win_w = 800;
    ui.win_h = 360;
    ui.magnify = 0;
    ui.mag_arm = 0;
    ui.prompt = 0;
    ui.gesture = GS_NONE;
    ui.stroking = 0;
    ui.scroll_x = 0;
    ui.scroll_y = 0;
    ui_mouse(&ui, &doc, &assets, 10, 26, 1, 1);
    ui_mouse(&ui, &doc, &assets, 10, 26, 1, 0);
    expect(doc.frames[0].pix[16 * doc.frames[0].w + 10] != 4, "grid skips the raw point");
    expect(doc.frames[0].pix[16 * doc.frames[0].w + 8] == 4, "grid snaps to 8");
    doc.grid_on = 0;
    doc_clear(&doc);
    doc.sym_on = 1;
    ui_mouse(&ui, &doc, &assets, 4, 20, 1, 1);
    ui_mouse(&ui, &doc, &assets, 4, 20, 1, 0);
    expect(doc.frames[0].pix[10 * doc.frames[0].w + 4] == 4, "symmetry source");
    expect(doc.frames[0].pix[10 * doc.frames[0].w + (doc.frames[0].w - 1 - 4)] == 4,
           "symmetry mirror");
    doc.sym_on = 0;
    doc_clear(&doc);
    doc.tool = TOOL_TEXT;
    ui_mouse(&ui, &doc, &assets, 8, 30, 1, 1);
    ui_mouse(&ui, &doc, &assets, 8, 30, 1, 0);
    ui_key(&ui, &doc, &assets, 'H', 'H');
    ui_key(&ui, &doc, &assets, 'i', 'i');
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    {
        int ink = 0;
        int i;
        int n = doc.frames[0].w * doc.frames[0].h;
        for (i = 0; i < n; i++) {
            if (doc.frames[0].pix[i] == 4) {
                ink++;
            }
        }
        expect(ui.text_on == 0, "enter commits text");
        expect(ink > 4, "Hi glyphs");
    }
    doc_clear(&doc);
    doc.tool = TOOL_DRAW;
    doc.mode = MODE_COLOR;
    ui_mouse(&ui, &doc, &assets, 8, 20, 1, 1);
    ui_mouse(&ui, &doc, &assets, 8, 20, 1, 0);
    doc.tool = TOOL_BRUSH;
    ui_mouse(&ui, &doc, &assets, 4, 16, 1, 1);
    ui_motion(&ui, &doc, &assets, 14, 26, 1);
    ui_mouse(&ui, &doc, &assets, 14, 26, 1, 0);
    expect(doc.brush.pix != NULL && doc.brush.w > 3, "brush pickup");
    doc.mode = MODE_MATTE;
    ui_mouse(&ui, &doc, &assets, 30, 20, 1, 1);
    ui_mouse(&ui, &doc, &assets, 30, 20, 1, 0);
    expect(doc.frames[0].pix[9 * doc.frames[0].w + 29] == 4, "matte stamp");
    doc.mode = MODE_COLOR;
    doc.fg = 5;
    ui_mouse(&ui, &doc, &assets, 40, 20, 1, 1);
    ui_mouse(&ui, &doc, &assets, 40, 20, 1, 0);
    expect(doc.frames[0].pix[9 * doc.frames[0].w + 39] == 5, "color mode uses foreground");
    doc.mode = MODE_REPLC;
    ui_mouse(&ui, &doc, &assets, 20, 30, 1, 1);
    ui_mouse(&ui, &doc, &assets, 20, 30, 1, 0);
    expect(doc.frames[0].pix[19 * doc.frames[0].w + 19] == 4, "replc keeps brush color");
    doc.fg = 4;
    doc_clear(&doc);
    doc.fg = 4;
    doc.tool = TOOL_DRAW;
    doc.mode = MODE_COLOR;
    ui_mouse(&ui, &doc, &assets, 6, 18, 1, 1);
    ui_mouse(&ui, &doc, &assets, 6, 18, 1, 0);
    doc_copy_spare(&doc);
    doc_clear(&doc);
    expect(doc.frames[0].pix[8 * doc.frames[0].w + 6] != 4, "page cleared");
    expect(doc.spare.pix[8 * doc.spare.w + 6] == 4, "copy to spare");
    doc_merge(&doc, 1);
    expect(doc.frames[0].pix[8 * doc.spare.w + 6] == 4, "merge in front");
    doc.mode = MODE_SMEAR;
    doc.fg = 7;
    doc.tool = TOOL_DRAW;
    ui_mouse(&ui, &doc, &assets, 6, 18, 1, 1);
    ui_motion(&ui, &doc, &assets, 14, 18, 1);
    ui_mouse(&ui, &doc, &assets, 14, 18, 1, 0);
    {
        int i, n = doc.frames[0].w * doc.frames[0].h;
        int saw = 0, bad = 0;
        for (i = 0; i < n; i++) {
            if (doc.frames[0].pix[i] == 4) {
                saw = 1;
            }
            if (doc.frames[0].pix[i] == 7) {
                bad = 1;
            }
        }
        expect(saw && !bad, "smear moves existing paint");
    }
    doc.mode = MODE_COLOR;
    doc.fg = 4;
    doc_set_format(&doc, FMT_MED);
    expect(doc.frames[0].w == 640 && doc.frames[0].h == 200, "med-res page");
    expect(ui.win_w == 800, "window stays large");
    doc_set_format(&doc, FMT_LACE);
    expect(doc.frames[0].w == 320 && doc.frames[0].h == 400, "interlace page");
    doc_set_format(&doc, FMT_HI);
    expect(doc.frames[0].w == 640 && doc.frames[0].h == 400, "hi-res page");
    doc_set_format(&doc, FMT_LO);
    expect(doc.frames[0].w == 320 && doc.frames[0].h == 200, "lo-res page");
    doc_goto(&doc, 0);
    while (doc.nframes < 10) {
        expect(doc_add_frames(&doc, 1, 1) == 0, "add frame");
        doc_goto(&doc, 0);
    }
    expect(doc.nframes >= 10, "ten frames");
    ui.prompt = 0;
    ui_key(&ui, &doc, &assets, '.', '.');
    ui_key(&ui, &doc, &assets, '.', '.');
    ui_key(&ui, &doc, &assets, '.', '.');
    expect(doc.cur == 3, "period lands on frame 4");
    ui_key(&ui, &doc, &assets, ',', ',');
    expect(doc.cur == 2, "comma previous frame");
    ui.prompt = 1;
    ui.text_len = 0;
    ui.text[0] = 0;
    ui_key(&ui, &doc, &assets, '.', '.');
    expect(doc.cur == 2, "period ignored while typing a path");
    expect(ui.text[0] == '.', "period goes into the path");
    ui.prompt = 0;
    ui.text_len = 0;
    ui.text[0] = 0;
    ui.magnify = 1;
    ui.mag_arm = 0;
    ui.mag_x0 = 4;
    ui.mag_y0 = 4;
    ui.mag_x1 = 20;
    ui.mag_y1 = 16;
    ui_key(&ui, &doc, &assets, '>', '>');
    expect(ui.mag_x1 - ui.mag_x0 < 16, "greater-than increases magnification");
    {
        int zoomed = ui.mag_x1 - ui.mag_x0;
        ui_key(&ui, &doc, &assets, '<', '<');
        expect(ui.mag_x1 - ui.mag_x0 > zoomed, "less-than decreases magnification");
    }
    ui_key(&ui, &doc, &assets, '5', '5');
    expect(doc.play == 2, "play once");
    ui_key(&ui, &doc, &assets, '6', '6');
    expect(doc.play == 3, "ping-pong");
    ui_key(&ui, &doc, &assets, ' ', ' ');
    expect(doc.play == 0, "space stops");
    {
        const char *wolles = "/home/scp1/dev/dpaintiv/wolles/animations/IFF/idoler";
        DpFrame big[DP_MAX_FRAMES];
        int k, wn = 0, wf = 0;
        DpColor wpal[DP_MAX_COLORS];
        DpRange wrg[DP_MAX_RANGES];
        int wnc = 0, wnr = 0, wfps = 0, wcamg = 0;
        char werr[120];
        memset(big, 0, sizeof(big));
        werr[0] = 0;
        expect(iff_load_anim(wolles, big, DP_MAX_FRAMES, &wn, wpal, &wnc, &wcamg,
                             wrg, &wnr, &wfps, werr, 120) == 0, "wolles idoler loads");
        expect(wn > 1, "wolles idoler has frames");
        for (k = 0; k < wn && k < DP_MAX_FRAMES; k++) {
            free(big[k].pix);
        }
        (void)wf;
    }
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_TEXT;
    doc.mode = MODE_COLOR;
    ui.scale = 1;
    ui.win_w = 800;
    ui.win_h = 360;
    ui.magnify = 0;
    ui.mag_arm = 0;
    ui.prompt = 0;
    ui.panel = 0;
    ui.gesture = GS_NONE;
    ui.stroking = 0;
    ui_mouse(&ui, &doc, &assets, 8, 40, 1, 1);
    ui_mouse(&ui, &doc, &assets, 8, 40, 1, 0);
    ui_key(&ui, &doc, &assets, 'a', 'a');
    ui_key(&ui, &doc, &assets, 'b', 'b');
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    {
        int sp = dp_font_char_space(&assets.fonts[0], 'a');
        int w = doc.frames[0].w;
        int h = doc.frames[0].h;
        int x, first = -1, gap = -1, second = -1;
        for (x = 0; x < w; x++) {
            int y, any = 0;
            for (y = 0; y < h; y++) {
                if (doc.frames[0].pix[y * w + x] == 4) {
                    any = 1;
                }
            }
            if (any && first < 0) {
                first = x;
            } else if (!any && first >= 0 && gap < 0) {
                gap = x;
            } else if (any && gap >= 0 && second < 0) {
                second = x;
            }
        }
        expect(sp > 0, "CharSpace of a");
        expect(gap >= 0, "gap between a and b");
        expect(second >= 0 && second - first >= sp, "b starts at least CharSpace after a");
    }
    ui.panel = 1;
    doc_goto(&doc, 0);
    {
        int x, y, n0 = doc.nframes;
        int mh, tw, bh, vw, vh, vh_on, bh0;
        ui_layout(&ui, &mh, &tw, &bh, &vw, &vh);
        vh_on = vh;
        expect(bh > 0 && vh < ui.win_h - mh, "panel shrinks the page");
        ui.panel = 0;
        ui_layout(&ui, &mh, &tw, &bh0, &vw, &vh);
        expect(bh0 == 0 && vh > vh_on, "panel off grows the page");
        ui.panel = 1;
        expect(ui_panel_point(&ui, 3, &x, &y) == 0, "panel next point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(doc.cur == 1, "panel next frame");
        expect(ui_panel_point(&ui, 2, &x, &y) == 0, "panel prev point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(doc.cur == 0, "panel previous frame");
        expect(ui_panel_point(&ui, 20, &x, &y) == 0, "slider point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(doc.cur > 1 && doc.cur < doc.nframes - 1, "slider jumps");
        expect(ui_panel_point(&ui, 7, &x, &y) == 0, "add frame point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(doc.nframes == n0 + 1, "panel add frame");
        doc_goto(&doc, 0);
        expect(ui_panel_point(&ui, 4, &x, &y) == 0, "play once point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(doc.play == 2, "panel play once");
        ui_tick(&ui, &doc, 8000);
        expect(doc.play == 0, "play once stops");
        expect(doc.cur == doc.nframes - 1, "play once ends on the last frame");
    }
    ui_key(&ui, &doc, &assets, '3', '3');
    expect(ui.prompt != 0, "key 3 opens go to");
    ui_key(&ui, &doc, &assets, '2', '2');
    ui_key(&ui, &doc, &assets, KEY_ENTER, 0);
    expect(ui.prompt == 0 && doc.cur == 1, "go to frame 2");
    ui_key(&ui, &doc, &assets, 0xA00 + 'a', 'a');
    expect(ui.panel == 0, "alt-a hides the panel");
    ui_key(&ui, &doc, &assets, 0xA00 + 'a', 'a');
    expect(ui.panel == 1, "alt-a shows the panel");
    {
        Doc hi;
        DpFrame loaded;
        DpColor pal[DP_MAX_COLORS];
        DpRange rg[DP_MAX_RANGES];
        int nc = 0, camg = 0, nr = 0;
        char hp[256];
        char err[80];
        memset(&loaded, 0, sizeof(loaded));
        memset(&hi, 0, sizeof(hi));
        expect(doc_init(&hi, 8, 4, 64, assets.pal) == 0, "64 doc");
        expect(hi.ncolors == 64, "64 colors");
        expect(hi.pal[40].r != hi.pal[0].r || hi.pal[40].g != hi.pal[0].g, "high color is visible");
        hi.frames[0].pix[1] = 40;
        snprintf(hp, sizeof(hp), "%s/c64.ilbm", dir);
        expect(iff_save_ilbm(hp, &hi.frames[0], hi.pal, hi.ncolors, 0,
                             NULL, 0, 0, 0, 0, 0, 1, 0) == 0, "save 64");
        err[0] = 0;
        expect(iff_load_ilbm(hp, &loaded, pal, &nc, &camg, rg, &nr,
                             NULL, NULL, NULL, NULL, err, 80) == 0, "load 64");
        expect(nc == 64 && loaded.pix != NULL && loaded.pix[1] == 40, "color 40 round trip");
        expect(pal[40].r == hi.pal[40].r && pal[40].g == hi.pal[40].g, "cmap 40");
        free(loaded.pix);
        loaded.pix = NULL;
        expect(doc_add_frames(&hi, 1, 0) == 0, "64 anim frame");
        hi.frames[1].pix[2] = 41;
        snprintf(hp, sizeof(hp), "%s/c64.anim", dir);
        {
            DpFrame af[4];
            int an = 0, anc = 0, afps = 0;
            memset(af, 0, sizeof(af));
            expect(iff_save_anim(hp, hi.frames, hi.nframes, hi.pal, hi.ncolors, 0,
                                 NULL, 0, 12, 1, 0, 0, 0, 0) == 0, "save 64 anim");
            expect(iff_load_anim(hp, af, 4, &an, pal, &anc, &camg, rg, &nr, &afps,
                                 err, 80) == 0, "load 64 anim");
            expect(an == 2 && anc == 64, "anim 64 count");
            expect(af[0].pix != NULL && af[0].pix[1] == 40, "anim color 40");
            expect(af[1].pix != NULL && af[1].pix[2] == 41, "anim color 41");
            free(af[0].pix);
            free(af[1].pix);
        }
        doc_free(&hi);
        memset(&hi, 0, sizeof(hi));
        expect(doc_init(&hi, 4, 4, 64, assets.pal) == 0, "slot doc");
        hi.fg = 40;
        snprintf(hp, sizeof(hp), "%s/slot.ilbm", dir);
        expect(iff_save_ilbm(hp, &hi.frames[0], hi.pal, hi.ncolors, 0,
                             NULL, 0, 0, 0, 0, 0, 1, hi.fg) == 0, "save used slot");
        expect(iff_load_ilbm(hp, &loaded, pal, &nc, &camg, rg, &nr,
                             NULL, NULL, NULL, NULL, err, 80) == 0, "load used slot");
        expect(nc == 64, "used slot keeps 64 colors");
        free(loaded.pix);
        doc_free(&hi);
    }
    {
        int fw = doc.frames[0].w;
        int last;
        int x, y;
        uint8_t *rgb;
        DpColor ink, paper, solid;
        int er, eg, eb;
        const uint8_t *p;
        while (doc.nframes < 3) {
            doc_goto(&doc, doc.nframes - 1);
            expect(doc_add_frames(&doc, 1, 1) == 0, "light table frame");
        }
        last = doc.nframes - 1;
        doc.bg = 31;
        doc_goto(&doc, 0);
        doc.frames[0].pix[3 * fw + 3] = 31;
        doc.frames[last].pix[3 * fw + 3] = 4;
        doc.frames[0].pix[5 * fw + 5] = 2;
        doc.frames[last].pix[5 * fw + 5] = 4;
        if (doc.nframes > 1) {
            doc.frames[1].pix[3 * fw + 3] = 6;
        }
        doc.frames[0].pix[7 * fw + 7] = 31;
        doc.frames[1].pix[7 * fw + 7] = 31;
        doc.frames[last].pix[7 * fw + 7] = 31;
        doc.frames[last - 1].pix[7 * fw + 7] = 5;
        ui.lt_on = 0;
        ui.lt_prev = 0;
        ui.lt_next = 0;
        ui.lt_two = 0;
        ui.lt_dim = 1;
        ui.magnify = 0;
        ui.mag_arm = 0;
        ui.scroll_x = 0;
        ui.scroll_y = 0;
        ui.panel = 1;
        ui.scale = 1;
        ui.win_w = 800;
        ui.win_h = 360;
        expect(ui_panel_point(&ui, 10, &x, &y) == 0, "light table point");
        ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
        ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
        expect(ui.lt_on && ui.lt_prev && !ui.lt_next, "light table shows the previous frame");
        ink = doc.pal[4];
        paper = doc.pal[31];
        solid = doc.pal[2];
        er = (ink.r + paper.r) / 2;
        eg = (ink.g + paper.g) / 2;
        eb = (ink.b + paper.b) / 2;
        expect(er != ink.r || eg != ink.g || eb != ink.b, "dim mix differs from the ink");
        expect(er != paper.r || eg != paper.g || eb != paper.b, "dim mix differs from the paper");
        rgb = (uint8_t *)malloc(800u * 360u * 3u);
        expect(rgb != NULL, "light table buffer");
        if (rgb != NULL) {
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            p = rgb + (((10 + 3) * 800 + 3) * 3);
            expect(p[0] == (uint8_t)er && p[1] == (uint8_t)eg && p[2] == (uint8_t)eb,
                   "previous frame shines through the paper");
            p = rgb + (((10 + 5) * 800 + 5) * 3);
            expect(p[0] == solid.r && p[1] == solid.g && p[2] == solid.b,
                   "current frame stays solid");
            ui.lt_dim = 0;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            p = rgb + (((10 + 3) * 800 + 3) * 3);
            expect(p[0] == ink.r && p[1] == ink.g && p[2] == ink.b,
                   "dim off shows the comparison color");
            expect(ui_panel_point(&ui, 12, &x, &y) == 0, "previous point");
            ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
            ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
            expect(ui_panel_point(&ui, 13, &x, &y) == 0, "next point");
            ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
            ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
            expect(ui.lt_next && !ui.lt_prev, "next replaces previous");
            ink = doc.pal[6];
            er = (ink.r + paper.r) / 2;
            eg = (ink.g + paper.g) / 2;
            eb = (ink.b + paper.b) / 2;
            ui.lt_dim = 1;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            p = rgb + (((10 + 3) * 800 + 3) * 3);
            expect(p[0] == (uint8_t)er && p[1] == (uint8_t)eg && p[2] == (uint8_t)eb,
                   "next frame shines through");
            expect(ui_panel_point(&ui, 13, &x, &y) == 0, "next off point");
            ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
            ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
            expect(ui_panel_point(&ui, 11, &x, &y) == 0, "two back point");
            ui_mouse(&ui, &doc, &assets, x, y, 1, 1);
            ui_mouse(&ui, &doc, &assets, x, y, 1, 0);
            expect(ui.lt_two && !ui.lt_next && !ui.lt_prev, "two back is the comparison");
            ink = doc.pal[5];
            er = (ink.r + paper.r) / 2;
            eg = (ink.g + paper.g) / 2;
            eb = (ink.b + paper.b) / 2;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            p = rgb + (((10 + 7) * 800 + 7) * 3);
            expect(p[0] == (uint8_t)er && p[1] == (uint8_t)eg && p[2] == (uint8_t)eb,
                   "two back shines through");
            free(rgb);
        }
        ui.lt_on = 0;
        ui.lt_prev = 0;
        ui.lt_next = 0;
        ui.lt_dim = 1;
        doc.bg = 0;
    }
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_DOT;
    doc.brush_d = 1;
    doc.mode = MODE_MATTE;
    doc.grid_on = 0;
    doc.sym_on = 0;
    ui.prompt = 0;
    ui.text_on = 0;
    ui.magnify = 0;
    ui.mag_arm = 0;
    ui.menu_open = -1;
    ui.scale = 1;
    ui.win_w = 800;
    ui.win_h = 360;
    ui.panel = 0;
    ui_mouse(&ui, &doc, &assets, 12, 24, 1, 1);
    ui_mouse(&ui, &doc, &assets, 12, 24, 1, 0);
    {
        int fw = doc.frames[doc.cur].w;
        int pi = 14 * fw + 12;
        uint8_t *rgb = (uint8_t *)malloc(800u * 360u * 3u);
        int x, left_brush = -1, left_tool = -1;
        expect(doc.frames[doc.cur].pix[pi] == 4, "dab before undo");
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        expect(doc.frames[doc.cur].pix[pi] == 0, "ctrl+z undoes");
        ui_mouse(&ui, &doc, &assets, 12, 24, 1, 1);
        ui_mouse(&ui, &doc, &assets, 12, 24, 1, 0);
        expect(doc.frames[doc.cur].pix[pi] == 4, "dab again");
        ui.prompt = 1;
        ui.text[0] = 'a';
        ui.text[1] = 0;
        ui.text_len = 1;
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        expect(doc.frames[doc.cur].pix[pi] == 4, "ctrl+z ignored in a text field");
        expect(ui.text_len == 1 && ui.text[0] == 'a', "path text stays");
        ui.prompt = 0;
        ui.text_on = 1;
        doc.tool = TOOL_TEXT;
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        expect(doc.frames[doc.cur].pix[pi] == 4, "ctrl+z ignored while typing");
        ui.text_on = 0;
        doc.tool = TOOL_DOT;
        expect(rgb != NULL, "brush width buffer");
        if (rgb != NULL) {
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            for (x = 740; x < 800; x++) {
                int pb = (15 * 800 + x) * 3;
                int pt = (50 * 800 + x) * 3;
                if (left_brush < 0 && rgb[pb] == 230 && rgb[pb + 1] == 230 &&
                    rgb[pb + 2] == 230) {
                    left_brush = x;
                }
                if (left_tool < 0 && rgb[pt] == 230 && rgb[pt + 1] == 230 &&
                    rgb[pt + 2] == 230) {
                    left_tool = x;
                }
            }
            expect(left_brush == left_tool && left_brush == 774,
                   "brush block matches the tool strip");
            free(rgb);
        }
    }
    expect(ui.enh_hover && ui.enh_undo && ui.enh_title, "2026 toggles default on");
    doc_set_undo_hist(&doc, 0);
    doc_set_undo_hist(&doc, 1);
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_DOT;
    doc.mode = MODE_MATTE;
    doc.brush_d = 1;
    doc.grid_on = 0;
    doc.sym_on = 0;
    ui.prompt = 0;
    ui.text_on = 0;
    ui.magnify = 0;
    ui.mag_arm = 0;
    ui.enh_undo = 1;
    ui.enh_open = 0;
    {
        int fw = doc.frames[doc.cur].w;
        int a = 14 * fw + 20;
        int b = 14 * fw + 24;
        int c = 14 * fw + 28;
        int n0 = doc.nframes;
        ui_mouse(&ui, &doc, &assets, 20, 24, 1, 1);
        ui_mouse(&ui, &doc, &assets, 20, 24, 1, 0);
        ui_mouse(&ui, &doc, &assets, 24, 24, 1, 1);
        ui_mouse(&ui, &doc, &assets, 24, 24, 1, 0);
        ui_mouse(&ui, &doc, &assets, 28, 24, 1, 1);
        ui_mouse(&ui, &doc, &assets, 28, 24, 1, 0);
        expect(doc.frames[doc.cur].pix[a] == 4 && doc.frames[doc.cur].pix[b] == 4 &&
               doc.frames[doc.cur].pix[c] == 4, "three strokes");
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        expect(doc.frames[doc.cur].pix[a] == 4, "first stroke remains");
        expect(doc.frames[doc.cur].pix[b] == 0 && doc.frames[doc.cur].pix[c] == 0,
               "two undos remove the later strokes");
        ui_key(&ui, &doc, &assets, KEY_CTRL_Y, 0);
        expect(doc.frames[doc.cur].pix[a] == 4 && doc.frames[doc.cur].pix[b] == 4 &&
               doc.frames[doc.cur].pix[c] == 0, "redo restores the middle stroke");
        if (n0 >= DP_MAX_FRAMES) {
            ui_key(&ui, &doc, &assets, '-', '-');
            n0 = doc.nframes;
        }
        ui_key(&ui, &doc, &assets, '+', '+');
        expect(doc.nframes == n0 + 1, "+ adds one frame");
        ui_key(&ui, &doc, &assets, '-', '-');
        expect(doc.nframes == n0, "- removes one frame");
        ui_key(&ui, &doc, &assets, '=', '=');
        expect(doc.nframes == n0 + 1, "= adds one frame");
        ui_key(&ui, &doc, &assets, '-', '-');
        ui.prompt = 1;
        ui.text[0] = 0;
        ui.text_len = 0;
        ui_key(&ui, &doc, &assets, '+', '+');
        expect(doc.nframes == n0, "+ does not add while typing");
        expect(ui.text_len == 1 && ui.text[0] == '+', "+ is typed into the field");
        ui.prompt = 0;
        ui.text[0] = 0;
        ui.text_len = 0;
    }
    doc_clear(&doc);
    doc.fg = 4;
    doc.bg = 0;
    doc.tool = TOOL_DOT;
    doc.brush_d = 1;
    doc.grid_on = 0;
    doc.sym_on = 0;
    ui.prompt = 0;
    ui.text_on = 0;
    ui.magnify = 0;
    ui.arrange = 0;
    ui.enh_undo = 0;
    doc_set_undo_hist(&doc, 0);
    ui_mouse(&ui, &doc, &assets, 30, 24, 1, 1);
    ui_mouse(&ui, &doc, &assets, 30, 24, 1, 0);
    {
        int fw = doc.frames[doc.cur].w;
        int pi = 14 * fw + 30;
        uint8_t painted = doc.frames[doc.cur].pix[pi];
        expect(painted == 4, "stroke while undo history is off");
        ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
        expect(doc.frames[doc.cur].pix[pi] == painted, "ctrl+z does nothing when history is off");
        doc_undo(&doc);
        expect(doc.frames[doc.cur].pix[pi] == 0, "one-level undo clears the stroke");
        doc_undo(&doc);
        expect(doc.frames[doc.cur].pix[pi] == painted, "second undo restores the stroke");
        ui.enh_undo = 1;
        doc_set_undo_hist(&doc, 1);
    }
    doc.pal[0].r = 0;
    doc.pal[0].g = 0;
    doc.pal[0].b = 255;
    doc.pal[4].r = 255;
    doc.pal[4].g = 255;
    doc.pal[4].b = 0;
    doc_pal_spread(&doc, 0, 4);
    expect(doc.pal[2].g > 200 && doc.pal[2].r < 40 && doc.pal[2].b > 40 &&
           doc.pal[2].b < 200, "spread between blue and yellow is green");
    expect(doc.pal[0].b == 255 && doc.pal[0].r == 0, "spread keeps the first color");
    expect(doc.pal[4].r == 255 && doc.pal[4].b == 0, "spread keeps the last color");
    {
        uint8_t *rgb = (uint8_t *)malloc(800u * 360u * 3u);
        int white_on = 0, white_off = 0, x, y;
        int title_off = 0, title_on = 0, panel_ink = 0;
        expect(rgb != NULL, "toggle buf");
        if (rgb != NULL) {
            ui.scale = 1;
            ui.win_w = 800;
            ui.win_h = 360;
            ui.menu_open = -1;
            ui.prompt = 0;
            ui.arrange = 0;
            ui.about = 0;
            ui.printer = 0;
            ui.enh_open = 0;
            ui.panel = 0;
            ui.hover_id = 16;
            ui.hover_ms = 500;
            ui.enh_hover = 1;
            ui.enh_title = 0;
            if (doc.nframes < 2) {
                doc_add_frames(&doc, 1, 0);
            }
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            for (y = 80; y < 220; y++) {
                for (x = 620; x < 790; x++) {
                    int p = (y * 800 + x) * 3;
                    if (rgb[p] > 250 && rgb[p + 1] > 250 && rgb[p + 2] > 250) {
                        white_on++;
                    }
                }
            }
            for (y = 1; y < 9; y++) {
                for (x = 120; x < 420; x++) {
                    int p = (y * 800 + x) * 3;
                    if (rgb[p] < 20 && rgb[p + 1] < 20 && rgb[p + 2] < 20) {
                        title_off++;
                    }
                }
            }
            ui.enh_hover = 0;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            for (y = 80; y < 220; y++) {
                for (x = 620; x < 790; x++) {
                    int p = (y * 800 + x) * 3;
                    if (rgb[p] > 250 && rgb[p + 1] > 250 && rgb[p + 2] > 250) {
                        white_off++;
                    }
                }
            }
            expect(white_on > white_off + 20, "hover help on draws the white label");
            ui.enh_hover = 1;
            ui.enh_title = 1;
            ui.panel = 0;
            ui.menu_open = -1;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            for (y = 1; y < 9; y++) {
                for (x = 120; x < 420; x++) {
                    int p = (y * 800 + x) * 3;
                    if (rgb[p] < 20 && rgb[p + 1] < 20 && rgb[p + 2] < 20) {
                        title_on++;
                    }
                }
            }
            expect(title_on > title_off, "title frame readout returns when the panel is hidden");
            ui.panel = 1;
            ui.enh_title = 0;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            for (y = 300; y < 360; y++) {
                for (x = 0; x < 80; x++) {
                    int p = (y * 800 + x) * 3;
                    if (rgb[p] < 30 && rgb[p + 1] < 30 && rgb[p + 2] < 30) {
                        panel_ink++;
                    }
                }
            }
            expect(panel_ink > 8, "panel counter stays when the title readout is off");
            ui.enh_hover = 0;
            ui.hover_id = 0;
            ui.panel = 1;
            ui.arrange = 0;
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            expect(dp_write_ppm("/tmp/dp-panel.ppm", rgb, 800, 360) == 0, "panel ppm");
            ui.arrange = 1;
            ui.arr_top = 0;
            ui.arr_sel = 0;
            ui.arr_op = 0;
            memcpy(ui.arr_open, doc.pal, sizeof(doc.pal));
            ui_render(&ui, &doc, &assets, rgb, 800, 360);
            expect(dp_write_ppm("/tmp/dp-arrange.ppm", rgb, 800, 360) == 0, "arrange ppm");
            {
                DpColor saved = doc.pal[1];
                ui.arr_op = 1;
                ui.arr_from = 0;
                ui_mouse(&ui, &doc, &assets, 40 + 16 + 8, 360 - 60 + 12 + 8, 1, 1);
                expect(doc.pal[1].b == doc.pal[0].b && doc.pal[1].r == doc.pal[0].r,
                       "COPY puts the source color on the next well");
                ui_key(&ui, &doc, &assets, KEY_ESC, 0);
                expect(ui.arrange == 0, "esc closes arrange");
                expect(doc.pal[1].r == saved.r && doc.pal[1].g == saved.g &&
                       doc.pal[1].b == saved.b, "esc reverts the palette");
            }
            free(rgb);
        }
    }
    printf("smoke %s (%d fails) ppm %s\n", fails ? "FAILED" : "ok", fails, ppm);
    free(snap);
    doc_free(&doc);
    dp_assets_free(&assets);
    return fails ? 1 : 0;
}
