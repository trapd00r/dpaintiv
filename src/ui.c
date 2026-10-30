#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "dpaint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MENU_H 10
#define BAR_H 4
#define PAL_COLS 4
#define PAL_CELL 6
#define PAL_GAP 1
#define ICON_PX 11
#define GADGET (ICON_PX + 2)
#define TOOL_STRIP (GADGET * 2)
#define BRUSH_CELL_W 6
#define BRUSH_CELL_H 8
#define BRUSH_COLS 4
#define BRUSH_W (BRUSH_CELL_W * BRUSH_COLS + 2)
#define BRUSH_H (BRUSH_CELL_H * 3 + 2)
#define PANEL_H 24
#define PANEL_BTNS 15
#define NTOOL_ROWS 8
#define NSTRIP_ROWS 9
#define BOT_H 0
#define TITLE_GAP 8
#define ITEM_H 10
#define CHROME_R 170
#define CHROME_G 170
#define CHROME_B 170
#define LIST_ROWS 8
#define LIST_ITEM 10

enum {
    CELL_TOOL = 1,
    CELL_NA = 2,
    CELL_MAG = 3,
    CELL_GRID = 4,
    CELL_SYM = 5,
    CELL_ZOOM = 6
};

enum {
    A_NONE = 0,
    A_LOAD_PIC, A_SAVE_PIC, A_DELETE, A_PRINT,
    A_FLIP_H, A_FLIP_V, A_SWAP, A_COPY_SPARE, A_MERGE_F, A_MERGE_B, A_DEL_PAGE,
    A_PAGE, A_SHOW, A_FMT_LO, A_FMT_MED, A_FMT_LACE, A_FMT_HI, A_ABOUT, A_QUIT,
    A_LOAD_BR, A_SAVE_BR, A_RESTORE, A_BR_TO, A_BR_SWAP,
    A_STRETCH, A_HALVE, A_DOUBLE, A_DUBH, A_DUBV,
    A_BFLIP_H, A_BFLIP_V, A_OUTLINE, A_ROT90, A_HANDLE_C, A_HANDLE_K,
    A_MODE,
    A_LOAD_ANIM, A_SAVE_ANIM, A_ADD, A_COPYF, A_DELF, A_SETN, A_RATE, A_RANGE,
    A_PREV, A_NEXT, A_GOTO, A_PLAY, A_ONCE, A_PING, A_COMP, A_EXP,
    A_AB_LOAD, A_AB_SAVE, A_AB_PICK, A_AB_FREE,
    A_ST_MAKE, A_ST_REMAKE, A_ST_PAINT, A_ST_SHOW, A_ST_REV, A_ST_ON, A_ST_FREE,
    A_ST_LOAD, A_ST_SAVE,
    A_CYCLE, A_USEPAL, A_RESTORE_PAL, A_DEFAULT_PAL, A_BG_FG, A_SWAP_FG,
    A_REMAP, A_TRANS, A_FONT, A_PREF,
    A_PANEL_ON, A_PANEL_OFF, A_VER2026, A_ARRANGE
};

typedef struct Item {
    const char *name;
    int act;
    int arg;
    const struct Item *kids;
    int nkids;
    const char *key;
} Item;

static const Item pic_flip[] = {
    {"Horiz", A_FLIP_H, 0, NULL, 0},
    {"Vert", A_FLIP_V, 0, NULL, 0}
};
static const Item pic_spare[] = {
    {"Swap", A_SWAP, 0, NULL, 0, "(j)"},
    {"Copy To Spare", A_COPY_SPARE, 0, NULL, 0},
    {"Merge in front", A_MERGE_F, 0, NULL, 0},
    {"Merge in back", A_MERGE_B, 0, NULL, 0},
    {"Delete this Page", A_DEL_PAGE, 0, NULL, 0}
};
static const Item pic_fmt[] = {
    {"Lo-Res     320x200", A_FMT_LO, 0, NULL, 0},
    {"Med-Res    640x200", A_FMT_MED, 0, NULL, 0},
    {"Interlace  320x400", A_FMT_LACE, 0, NULL, 0},
    {"Hi-Res     640x400", A_FMT_HI, 0, NULL, 0}
};
static const Item picture_items[] = {
    {"Load..", A_LOAD_PIC, 0, NULL, 0},
    {"Save..", A_SAVE_PIC, 0, NULL, 0},
    {"Delete..", A_DELETE, 0, NULL, 0},
    {"Print..", A_PRINT, 0, NULL, 0},
    {"Flip", 0, 0, pic_flip, 2},
    {"Spare", 0, 0, pic_spare, 5},
    {"Page Size..", A_PAGE, 0, NULL, 0},
    {"Show Page", A_SHOW, 0, NULL, 0},
    {"Screen Format..", 0, 0, pic_fmt, 4},
    {"About..", A_ABOUT, 0, NULL, 0},
    {"Quit", A_QUIT, 0, NULL, 0}
};

static const Item br_spare[] = {
    {"Brush->Spare", A_BR_TO, 0, NULL, 0},
    {"Brush<->Spare", A_BR_SWAP, 0, NULL, 0}
};
static const Item br_size[] = {
    {"Stretch", A_STRETCH, 0, NULL, 0},
    {"Halve", A_HALVE, 0, NULL, 0},
    {"Double", A_DOUBLE, 0, NULL, 0},
    {"Double Horiz", A_DUBH, 0, NULL, 0},
    {"Double Vert", A_DUBV, 0, NULL, 0}
};
static const Item br_flip[] = {
    {"Horiz", A_BFLIP_H, 0, NULL, 0},
    {"Vert", A_BFLIP_V, 0, NULL, 0}
};
static const Item br_edge[] = {
    {"Outline", A_OUTLINE, 0, NULL, 0}
};
static const Item br_rot[] = {
    {"90 Degrees", A_ROT90, 0, NULL, 0}
};
static const Item br_handle[] = {
    {"Center", A_HANDLE_C, 0, NULL, 0},
    {"Corner", A_HANDLE_K, 0, NULL, 0}
};
static const Item brush_items[] = {
    {"Load..", A_LOAD_BR, 0, NULL, 0},
    {"Save..", A_SAVE_BR, 0, NULL, 0},
    {"Restore", A_RESTORE, 0, NULL, 0},
    {"Spare", 0, 0, br_spare, 2},
    {"Size", 0, 0, br_size, 5},
    {"Flip", 0, 0, br_flip, 2},
    {"Edge", 0, 0, br_edge, 1},
    {"Rotate", 0, 0, br_rot, 1},
    {"Handle", 0, 0, br_handle, 2}
};

static const Item mode_items[] = {
    {"Matte", A_MODE, MODE_MATTE, NULL, 0, "(f1)"},
    {"Color", A_MODE, MODE_COLOR, NULL, 0, "(f2)"},
    {"Replc", A_MODE, MODE_REPLC, NULL, 0, "(f3)"},
    {"Smear", A_MODE, MODE_SMEAR, NULL, 0, "(f4)"},
    {"Shade", A_MODE, MODE_SHADE, NULL, 0, "(f5)"},
    {"Blend", A_MODE, MODE_BLEND, NULL, 0, "(f6)"},
    {"Cycle", A_MODE, MODE_CYCLE, NULL, 0, "(f7)"},
    {"Smooth", A_MODE, MODE_SMOOTH, NULL, 0, "(f8)"},
    {"Mix", A_MODE, MODE_MIX, NULL, 0},
    {"HBrite", A_MODE, MODE_HBRITE, NULL, 0}
};

static const Item ab_items[] = {
    {"Load..", A_AB_LOAD, 0, NULL, 0},
    {"Save..", A_AB_SAVE, 0, NULL, 0},
    {"Pick up..", A_AB_PICK, 0, NULL, 0},
    {"Free", A_AB_FREE, 0, NULL, 0}
};
static const Item anim_frames[] = {
    {"Add frames..", A_ADD, 0, NULL, 0, "(+)"},
    {"Copy frames..", A_COPYF, 0, NULL, 0},
    {"Delete frames..", A_DELF, 0, NULL, 0, "(-)"},
    {"Set # ..", A_SETN, 0, NULL, 0}
};
static const Item anim_ctrl[] = {
    {"Set Rate..", A_RATE, 0, NULL, 0},
    {"Set Range..", A_RANGE, 0, NULL, 0},
    {"Previous", A_PREV, 0, NULL, 0, "(,)"},
    {"Next", A_NEXT, 0, NULL, 0, "(.)"},
    {"Go to..", A_GOTO, 0, NULL, 0, "(3)"},
    {"Play", A_PLAY, 0, NULL, 0, "(4)"},
    {"Play once", A_ONCE, 0, NULL, 0, "(5)"},
    {"Ping-pong", A_PING, 0, NULL, 0, "(6)"},
    {"Panel On", A_PANEL_ON, 0, NULL, 0, "(alt+a)"},
    {"Panel Off", A_PANEL_OFF, 0, NULL, 0, "(alt+a)"}
};
static const Item anim_method[] = {
    {"Expanded", A_EXP, 0, NULL, 0},
    {"Compressed", A_COMP, 0, NULL, 0}
};
static const Item anim_items[] = {
    {"AnimBrush", 0, 0, ab_items, 4},
    {"Load..", A_LOAD_ANIM, 0, NULL, 0},
    {"Save..", A_SAVE_ANIM, 0, NULL, 0},
    {"Frames", 0, 0, anim_frames, 4},
    {"Control", 0, 0, anim_ctrl, 10},
    {"Method", 0, 0, anim_method, 2}
};

static const Item st_items[] = {
    {"Make..", A_ST_MAKE, 0, NULL, 0},
    {"Remake", A_ST_REMAKE, 0, NULL, 0},
    {"Paint On/Off", A_ST_PAINT, 0, NULL, 0},
    {"Show", A_ST_SHOW, 0, NULL, 0},
    {"Reverse", A_ST_REV, 0, NULL, 0},
    {"On/Off", A_ST_ON, 0, NULL, 0},
    {"Free", A_ST_FREE, 0, NULL, 0},
    {"Load..", A_ST_LOAD, 0, NULL, 0},
    {"Save..", A_ST_SAVE, 0, NULL, 0}
};
static const Item effect_items[] = {
    {"Stencil", 0, 0, st_items, 9},
    {"LightTable", A_NONE, 1, NULL, 0},
    {"Background", A_NONE, 1, NULL, 0},
    {"AntiAlias", A_NONE, 1, NULL, 0},
    {"Translucency", A_NONE, 1, NULL, 0},
    {"Perspective", A_NONE, 1, NULL, 0}
};

static const Item pal_items[] = {
    {"Arrange", A_ARRANGE, 0, NULL, 0},
    {"Use Brush Palette", A_USEPAL, 0, NULL, 0},
    {"Restore Palette", A_RESTORE_PAL, 0, NULL, 0},
    {"Default Palette", A_DEFAULT_PAL, 0, NULL, 0}
};
static const Item color_items[] = {
    {"Cycle", A_CYCLE, 0, NULL, 0},
    {"Palette", 0, 0, pal_items, 4},
    {"BG -> FG", A_BG_FG, 0, NULL, 0},
    {"BG <-> FG", A_SWAP_FG, 0, NULL, 0},
    {"Remap ", A_REMAP, 0, NULL, 0},
    {"Change Transparency", A_TRANS, 0, NULL, 0}
};

static const Item pref_items[] = {
    {"Coords", A_PREF, PREF_COORDS, NULL, 0},
    {"Fast FB", A_PREF, PREF_FASTFB, NULL, 0},
    {"MultiCycle", A_PREF, PREF_MULTICYCLE, NULL, 0},
    {"Be Square", A_PREF, PREF_SQUARE, NULL, 0},
    {"Workbench", A_PREF, PREF_WORKBENCH, NULL, 0},
    {"ExclBrush", A_PREF, PREF_EXCL, NULL, 0},
    {"AutoTransp", A_PREF, PREF_AUTOTRANSP, NULL, 0},
    {"No Icons", A_PREF, PREF_NOICONS, NULL, 0},
    {"AutoGrid", A_PREF, PREF_AUTOGRID, NULL, 0},
    {"OriginUL", A_PREF, PREF_ORIGINUL, NULL, 0},
    {"FastAdjust", A_PREF, PREF_FASTADJUST, NULL, 0},
    {"2026 version..", A_VER2026, 0, NULL, 0}
};

static char font_name[DP_MAX_FONTS][32];
static Item font_faces[DP_MAX_FONTS];
static Item font_items[] = {
    {"Choose Font", 0, 0, NULL, 0}
};

static void sync_fonts(const DpAssets *a)
{
    int i, n;
    if (a == NULL) {
        return;
    }
    n = a->nfonts;
    if (n > DP_MAX_FONTS) {
        n = DP_MAX_FONTS;
    }
    for (i = 0; i < n; i++) {
        snprintf(font_name[i], sizeof(font_name[i]), "%s",
                 a->fonts[i].face[0] ? a->fonts[i].face : "Font");
        font_faces[i].name = font_name[i];
        font_faces[i].act = A_FONT;
        font_faces[i].arg = i;
        font_faces[i].kids = NULL;
        font_faces[i].nkids = 0;
    }
    font_items[0].name = "Choose Font";
    font_items[0].kids = font_faces;
    font_items[0].nkids = n;
}

typedef struct {
    const char *title;
    const Item *items;
    int n;
} MenuBar;

static const MenuBar bars[] = {
    {"Picture", picture_items, 11},
    {"Brush", brush_items, 9},
    {"Mode", mode_items, 10},
    {"Anim", anim_items, 6},
    {"Color", color_items, 6},
    {"Effect", effect_items, 6},
    {"Font", font_items, 1},
    {"Prefs", pref_items, 12}
};
static const int nbars = 8;

static const int cell_kind[NTOOL_ROWS][2] = {
    {CELL_TOOL, CELL_TOOL},
    {CELL_TOOL, CELL_TOOL},
    {CELL_TOOL, CELL_TOOL},
    {CELL_TOOL, CELL_TOOL},
    {CELL_TOOL, CELL_TOOL},
    {CELL_TOOL, CELL_TOOL},
    {CELL_GRID, CELL_SYM},
    {CELL_MAG, CELL_ZOOM}
};

static const int cell_tool[NTOOL_ROWS][2] = {
    {TOOL_DOT, TOOL_DRAW},
    {TOOL_LINE, TOOL_CURVE},
    {TOOL_FILL, TOOL_AIR},
    {TOOL_RECT, TOOL_FRECT},
    {TOOL_CIRCLE, TOOL_ELLIPSE},
    {TOOL_BRUSH, TOOL_TEXT},
    {0, 0},
    {0, 0}
};

static int sc(const Ui *u)
{
    return u->scale < 1 ? 1 : u->scale;
}

static int log_w(const Ui *u)
{
    int w = u->win_w / sc(u);
    return w > 0 ? w : 1;
}

static int log_h(const Ui *u)
{
    int h = u->win_h / sc(u);
    return h > 0 ? h : 1;
}

static int pal_col_x(int col)
{
    int pair = col / 2;
    int sub = col & 1;
    return pair * (PAL_CELL * 2 + PAL_GAP) + sub * (PAL_CELL + PAL_GAP);
}

static int pal_col_at(int local_x)
{
    int stride = PAL_CELL * 2 + PAL_GAP;
    int pair, within;
    if (local_x < 0 || stride < 1) {
        return -1;
    }
    pair = local_x / stride;
    within = local_x % stride;
    if (pair < 0 || pair >= PAL_COLS / 2) {
        return -1;
    }
    if (within < PAL_CELL) {
        return pair * 2;
    }
    return pair * 2 + 1;
}

static void strip_metrics(const Ui *u, int ncolors,
                          int *tool_x, int *tool_w,
                          int *img_x, int *img_y, int *img_w, int *img_h,
                          int *brush_h,
                          int *pal_x, int *pal_y, int *pal_w, int *pal_h,
                          int *cell)
{
    int lw = log_w(u);
    int rows, cw, palw, colw, tools, bh, ih, xcol, xtool;
    if (ncolors < 1) {
        ncolors = 1;
    }
    rows = (ncolors + PAL_COLS - 1) / PAL_COLS;
    if (rows < 1) {
        rows = 1;
    }
    cw = PAL_CELL;
    if (cw < 1) {
        cw = 1;
    }
    tools = TOOL_STRIP;
    if (lw < 1) {
        lw = 1;
    }
    if (tools > lw) {
        tools = lw;
    }
    bh = BRUSH_H;
    ih = bh + NSTRIP_ROWS * GADGET;
    palw = tools;
    colw = tools;
    if (colw > lw) {
        colw = lw;
    }
    xcol = lw - colw;
    xtool = lw - tools;
    if (xtool < 0) {
        xtool = 0;
    }
    if (tool_x) {
        *tool_x = xcol;
    }
    if (tool_w) {
        *tool_w = colw;
    }
    if (img_x) {
        *img_x = xtool;
    }
    if (img_y) {
        *img_y = MENU_H;
    }
    if (img_w) {
        *img_w = tools;
    }
    if (img_h) {
        *img_h = ih;
    }
    if (brush_h) {
        *brush_h = bh;
    }
    if (pal_x) {
        *pal_x = xtool;
    }
    if (pal_y) {
        *pal_y = MENU_H + ih + BAR_H;
    }
    if (pal_w) {
        *pal_w = palw;
    }
    if (pal_h) {
        *pal_h = rows * cw;
    }
    if (cell) {
        *cell = cw;
    }
}

static void strip_row_y(int img_h, int brush_h, int row, int *y0, int *y1)
{
    int rest = img_h - brush_h;
    int base, extra, i, y, rh;
    if (rest < NSTRIP_ROWS) {
        rest = NSTRIP_ROWS;
    }
    base = rest / NSTRIP_ROWS;
    extra = rest % NSTRIP_ROWS;
    y = brush_h;
    for (i = 0; i < NSTRIP_ROWS; i++) {
        rh = base + (i >= NSTRIP_ROWS - extra ? 1 : 0);
        if (i == row) {
            *y0 = y;
            *y1 = y + rh;
            return;
        }
        y += rh;
    }
    *y0 = brush_h;
    *y1 = img_h;
}

static void chrome_geom(const Ui *u, int ncolors,
                        int *tool_x, int *tool_w, int *ind_y,
                        int *pal_x, int *pal_y, int *pal_w, int *pal_h,
                        int *cell_w, int *cell_h, int *cols, int *rows)
{
    int ih = 0, cw = PAL_CELL, py = MENU_H;
    int nc = ncolors < 1 ? 1 : ncolors;
    strip_metrics(u, nc, tool_x, tool_w, NULL, NULL, NULL, &ih, NULL,
                  pal_x, pal_y, pal_w, pal_h, &cw);
    py = MENU_H + ih;
    if (ind_y) {
        *ind_y = py;
    }
    if (cell_w) {
        *cell_w = cw;
    }
    if (cell_h) {
        *cell_h = cw;
    }
    if (cols) {
        *cols = PAL_COLS;
    }
    if (rows) {
        *rows = (nc + PAL_COLS - 1) / PAL_COLS;
    }
}

void ui_layout(const Ui *u, int *menu_h, int *tool_w, int *bot_h,
               int *view_w, int *view_h)
{
    int tw = TOOL_STRIP;
    chrome_geom(u, 32, NULL, &tw, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    *menu_h = MENU_H;
    *tool_w = tw;
    *bot_h = u->panel ? PANEL_H : 0;
    *view_w = log_w(u) - tw;
    *view_h = log_h(u) - MENU_H - *bot_h;
    if (*view_w < 1) {
        *view_w = 1;
    }
    if (*view_h < 1) {
        *view_h = 1;
    }
}

int ui_palette_box(const Ui *u, int ncolors, int *x, int *y, int *w, int *h)
{
    chrome_geom(u, ncolors, NULL, NULL, NULL, x, y, w, h, NULL, NULL, NULL, NULL);
    return 0;
}

void ui_init(Ui *u)
{
    memset(u, 0, sizeof(*u));
    u->menu_open = -1;
    u->menu_item = -1;
    u->menu_sub = -1;
    u->scale = 1;
    u->prefs = PREF_COORDS | PREF_ORIGINUL;
    u->win_w = 640;
    u->win_h = 400;
    u->os_cursor_hidden = 1;
    u->lt_dim = 1;
    u->enh_hover = 1;
    u->enh_undo = 1;
    u->enh_title = 1;
}

static const DpFont *font(const DpAssets *a, const Ui *u)
{
    (void)u;
    if (a->nfonts < 1) {
        return NULL;
    }
    return &a->fonts[0];
}

static const DpFont *text_font(const DpAssets *a, const Ui *u)
{
    if (a->nfonts < 1) {
        return NULL;
    }
    if (u->font_i >= 0 && u->font_i < a->nfonts) {
        return &a->fonts[u->font_i];
    }
    return &a->fonts[0];
}

static void fill_rect(uint8_t *rgb, int w, int h, int x, int y, int rw, int rh,
                      uint8_t r, uint8_t g, uint8_t b)
{
    int yy, xx;
    for (yy = 0; yy < rh; yy++) {
        int py = y + yy;
        if (py < 0 || py >= h) {
            continue;
        }
        for (xx = 0; xx < rw; xx++) {
            int px = x + xx;
            int di;
            if (px < 0 || px >= w) {
                continue;
            }
            di = (py * w + px) * 3;
            rgb[di] = r;
            rgb[di + 1] = g;
            rgb[di + 2] = b;
        }
    }
}

static int item_checked(const Ui *u, const Doc *d, const Item *it)
{
    if (it->act == A_MODE) {
        return d->mode == it->arg;
    }
    if (it->act == A_PREF) {
        return (u->prefs & it->arg) != 0;
    }
    if (it->act == A_COMP) {
        return d->method_compressed;
    }
    if (it->act == A_EXP) {
        return !d->method_compressed;
    }
    if (it->act == A_HANDLE_C) {
        return d->handle_center;
    }
    if (it->act == A_HANDLE_K) {
        return !d->handle_center;
    }
    if (it->act == A_ST_ON || it->act == A_ST_PAINT) {
        return d->stencil_on;
    }
    if (it->act == A_CYCLE) {
        return d->nranges > 0 && d->ranges[0].active;
    }
    return 0;
}

static int menu_width(const DpFont *f, const Item *items, int n)
{
    int i, m = 40;
    for (i = 0; i < n; i++) {
        int w = dp_font_width(f, items[i].name) + 18;
        if (items[i].key != NULL && items[i].key[0] != 0) {
            w += dp_font_width(f, items[i].key) + 10;
        }
        if (w > m) {
            m = w;
        }
    }
    return m;
}

static void draw_item_text(const DpFont *f, uint8_t *rgb, int w, int h,
                           int x, int y, int mw, const Item *it, int hi, int s)
{
    uint8_t c = hi ? 255 : 0;
    dp_font_rgb(f, rgb, w, h, (x + 8) * s, (y + 1) * s, it->name, c, c, c, s);
    if (it->key != NULL && it->key[0] != 0) {
        int kw = dp_font_width(f, it->key);
        dp_font_rgb(f, rgb, w, h, (x + mw - kw - 4) * s, (y + 1) * s,
                    it->key, c, c, c, s);
    }
}

static int bar_text_x(const DpFont *f, int index)
{
    int x = 4;
    int i;
    if (index < 0) {
        return x;
    }
    for (i = 0; i < index && i < nbars; i++) {
        x += dp_font_width(f, bars[i].title) + TITLE_GAP;
    }
    return x;
}

static void menu_column(const DpFont *f, int which, int lw,
                        int *mx, int *my, int *mw, int *mh)
{
    const MenuBar *mb = &bars[which];
    *mw = menu_width(f, mb->items, mb->n);
    *mh = mb->n * ITEM_H;
    *mx = bar_text_x(f, which) - 2;
    *my = MENU_H;
    if (*mx + *mw > lw) {
        *mx = lw - *mw;
    }
    if (*mx < 0) {
        *mx = 0;
    }
}

static void submenu_column(const DpFont *f, int which, int item, int lw,
                           int *sx, int *sy, int *sw, int *sh)
{
    const MenuBar *mb = &bars[which];
    const Item *it = &mb->items[item];
    int mx, my, mw, mh;
    menu_column(f, which, lw, &mx, &my, &mw, &mh);
    *sw = menu_width(f, it->kids, it->nkids);
    *sh = it->nkids * ITEM_H;
    *sx = mx + mw;
    *sy = my + item * ITEM_H;
    if (*sx + *sw > lw) {
        *sx = mx - *sw;
    }
    if (*sx < 0) {
        *sx = 0;
    }
}

int ui_menu_count(void)
{
    return nbars;
}

const char *ui_menu_title(int index)
{
    if (index < 0 || index >= nbars) {
        return NULL;
    }
    return bars[index].title;
}

int ui_menu_title_x(const DpFont *f, int index)
{
    return bar_text_x(f, index);
}

int ui_menu_box(const Ui *u, const DpFont *f, int *x, int *y, int *w, int *h)
{
    int lw;
    if (u->menu_open < 0 || u->menu_open >= nbars || f == NULL) {
        return -1;
    }
    lw = u->win_w / (u->scale < 1 ? 1 : u->scale);
    if (lw < 1) {
        lw = 1;
    }
    menu_column(f, u->menu_open, lw, x, y, w, h);
    return 0;
}

int ui_menu_subbox(const Ui *u, const DpFont *f, int *x, int *y, int *w, int *h)
{
    int lw;
    const MenuBar *mb;
    if (u->menu_open < 0 || u->menu_open >= nbars || f == NULL) {
        return -1;
    }
    mb = &bars[u->menu_open];
    if (u->menu_item < 0 || u->menu_item >= mb->n || mb->items[u->menu_item].nkids < 1) {
        return -1;
    }
    lw = u->win_w / (u->scale < 1 ? 1 : u->scale);
    if (lw < 1) {
        lw = 1;
    }
    submenu_column(f, u->menu_open, u->menu_item, lw, x, y, w, h);
    return 0;
}

static void put_px(uint8_t *rgb, int w, int h, int x, int y,
                   uint8_t r, uint8_t g, uint8_t b)
{
    int di;
    if (x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }
    di = (y * w + x) * 3;
    rgb[di] = r;
    rgb[di + 1] = g;
    rgb[di + 2] = b;
}

static const DpFrame *shown(const Ui *u, const Doc *d)
{
    if (u->show_spare) {
        return &d->spare;
    }
    return &d->frames[d->cur];
}

static const char *req_title(int act)
{
    switch (act) {
    case A_LOAD_PIC:
        return "Load Picture";
    case A_SAVE_PIC:
        return "Save Picture";
    case A_LOAD_BR:
        return "Load Brush";
    case A_SAVE_BR:
        return "Save Brush";
    case A_LOAD_ANIM:
        return "Load Anim";
    case A_SAVE_ANIM:
        return "Save Anim";
    case A_AB_LOAD:
        return "Load AnimBrush";
    case A_AB_SAVE:
        return "Save AnimBrush";
    case A_ST_LOAD:
        return "Load Stencil";
    case A_ST_SAVE:
        return "Save Stencil";
    case A_DELETE:
        return "Delete File";
    case A_PAGE:
        return "Page Size";
    case A_STRETCH:
        return "Stretch";
    case A_ADD:
        return "Add Frames";
    case A_COPYF:
        return "Copy Frames";
    case A_DELF:
        return "Delete Frames";
    case A_SETN:
        return "Set Count";
    case A_RATE:
        return "Frame Rate";
    case A_GOTO:
        return "Go To Frame";
    case A_RANGE:
        return "Play Range";
    case -1:
        return "Not in this build";
    default:
        return "Requester";
    }
}

static int file_act(int act)
{
    switch (act) {
    case A_LOAD_PIC:
    case A_SAVE_PIC:
    case A_DELETE:
    case A_LOAD_BR:
    case A_SAVE_BR:
    case A_LOAD_ANIM:
    case A_SAVE_ANIM:
    case A_AB_LOAD:
    case A_AB_SAVE:
    case A_ST_LOAD:
    case A_ST_SAVE:
        return 1;
    default:
        return 0;
    }
}

static void req_box(const Ui *u, int *x, int *y, int *w, int *h,
                    int *okx, int *oky, int *cx, int *cy, int *bw, int *bh)
{
    int lw = log_w(u);
    int lh = log_h(u);
    *w = 240;
    *h = 58;
    if (file_act(u->prompt)) {
        *w = 268;
        *h = 148;
    }
    if (*w > lw - 8) {
        *w = lw - 8;
    }
    if (*w < 80) {
        *w = lw > 4 ? lw - 2 : lw;
    }
    *x = (lw - *w) / 2;
    *y = (lh - *h) / 2;
    if (*x < 0) {
        *x = 0;
    }
    if (*y < MENU_H + 2) {
        *y = MENU_H + 2;
    }
    *bw = 54;
    *bh = 12;
    *oky = *y + *h - *bh - 6;
    *okx = *x + 10;
    *cx = *x + *w - 10 - *bw;
    *cy = *oky;
}

typedef struct {
    char name[96];
    int is_dir;
} Dent;

static int dent_cmp(const void *va, const void *vb)
{
    const Dent *a = va;
    const Dent *b = vb;
    if (a->is_dir != b->is_dir) {
        return b->is_dir - a->is_dir;
    }
    return strcmp(a->name, b->name);
}

static int scan_dir(const char *dir, Dent *ents, int max)
{
    DIR *dp;
    struct dirent *de;
    int n = 0;
    if (max < 1) {
        return 0;
    }
    snprintf(ents[0].name, sizeof(ents[0].name), "..");
    ents[0].is_dir = 1;
    n = 1;
    dp = opendir(dir != NULL && dir[0] != 0 ? dir : ".");
    if (dp == NULL) {
        return n;
    }
    while ((de = readdir(dp)) != NULL && n < max) {
        char path[640];
        struct stat st;
        size_t len;
        if (de->d_name[0] == '.') {
            continue;
        }
        len = strlen(de->d_name);
        if (len > 5 && strcmp(de->d_name + len - 5, ".info") == 0) {
            continue;
        }
        snprintf(ents[n].name, sizeof(ents[n].name), "%s", de->d_name);
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        ents[n].is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
        n++;
    }
    closedir(dp);
    if (n > 1) {
        qsort(ents + 1, (size_t)(n - 1), sizeof(Dent), dent_cmp);
    }
    return n;
}

static void list_geom(const Ui *u, int *x, int *y, int *w, int *h)
{
    int rx, ry, rw, rh, okx, oky, cx, cy, bw, bh;
    req_box(u, &rx, &ry, &rw, &rh, &okx, &oky, &cx, &cy, &bw, &bh);
    *x = rx + 8;
    *y = ry + 26;
    *w = rw - 16;
    *h = LIST_ROWS * LIST_ITEM;
    (void)okx;
    (void)oky;
    (void)cx;
    (void)cy;
    (void)bw;
    (void)bh;
}

static void dir_up(char *dir)
{
    char *s = strrchr(dir, '/');
    if (s == NULL) {
        snprintf(dir, 512, ".");
        return;
    }
    if (s == dir) {
        dir[1] = 0;
        return;
    }
    *s = 0;
}

static void clear_field(Ui *u)
{
    u->text_len = 0;
    u->text[0] = 0;
}

static void dir_enter(Ui *u, const char *name)
{
    char next[512];
    if (strcmp(name, "..") == 0) {
        dir_up(u->dir);
    } else {
        snprintf(next, sizeof(next), "%s/%s", u->dir, name);
        snprintf(u->dir, sizeof(u->dir), "%s", next);
    }
    u->list_scroll = 0;
    u->list_sel = -1;
    clear_field(u);
}

int ui_prompt_pick(Ui *u, const char *name)
{
    Dent ents[180];
    int n, i;
    if (u == NULL || name == NULL || !file_act(u->prompt)) {
        return -1;
    }
    n = scan_dir(u->dir, ents, 180);
    for (i = 0; i < n; i++) {
        if (strcmp(ents[i].name, name) == 0) {
            snprintf(u->text, sizeof(u->text), "%s", name);
            u->text_len = (int)strlen(u->text);
            u->list_sel = i;
            if (i < u->list_scroll) {
                u->list_scroll = i;
            }
            if (i >= u->list_scroll + LIST_ROWS) {
                u->list_scroll = i - LIST_ROWS + 1;
            }
            return 0;
        }
    }
    return -1;
}

int ui_prompt_button(const Ui *u, int which, int *x, int *y)
{
    int rx, ry, rw, rh, okx, oky, cx, cy, bw, bh;
    if (u == NULL || u->prompt == 0 || x == NULL || y == NULL) {
        return -1;
    }
    req_box(u, &rx, &ry, &rw, &rh, &okx, &oky, &cx, &cy, &bw, &bh);
    if (which == 0) {
        *x = okx + bw / 2;
        *y = oky + bh / 2;
    } else {
        *x = cx + bw / 2;
        *y = cy + bh / 2;
    }
    return 0;
}

static void mag_view(const Ui *u, int pw, int ph, int vw, int vh,
                     int *left_w, int *gap, int *ox, int *oy,
                     int *rwdt, int *rhgt);

static int icon_on(const DpAssets *a, int id, int x, int y)
{
    if (a == NULL || id < 0 || id >= a->nicons) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= 11 || y >= 11) {
        return 0;
    }
    /* Bit 1 is the gray button. The symbol is the clear bits. */
    return a->icon[id][y][x] ? 0 : 1;
}

static int letter_on(int which, int x, int y)
{
    static const char *u_bits[7] = {
        "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#####"
    };
    static const char *c_bits[7] = {
        ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."
    };
    const char *row;
    if (x < 0 || y < 0 || x >= 5 || y >= 7) {
        return 0;
    }
    row = (which == 0 ? u_bits : c_bits)[y];
    return row[x] == '#';
}

static int brush_ink(int n, int cx, int cy)
{
    switch (n) {
    case 1:
        return cx == 3 && cy == 4;
    case 2:
        return (cy == 3 && cx >= 2 && cx <= 3) ||
               (cy == 4 && cx >= 1 && cx <= 4) ||
               (cy == 5 && cx >= 2 && cx <= 3);
    case 3:
        return (cy == 2 && cx >= 2 && cx <= 3) ||
               (cy >= 3 && cy <= 5 && cx >= 1 && cx <= 4) ||
               (cy == 6 && cx >= 2 && cx <= 3);
    case 4:
        return (cy == 1 && cx >= 2 && cx <= 3) ||
               (cy >= 2 && cy <= 5 && cx >= 1 && cx <= 4) ||
               (cy == 6 && cx >= 2 && cx <= 3);
    case 5:
        return cx >= 2 && cx <= 3 && cy >= 3 && cy <= 4;
    case 6:
        return cx >= 1 && cx <= 3 && cy >= 2 && cy <= 4;
    case 7:
        return cx >= 1 && cx <= 4 && cy >= 2 && cy <= 5;
    case 8:
        return cx >= 1 && cx <= 4 && cy >= 1 && cy <= 5;
    case 9:
        return (cx == 6 && cy == 3) || (cx == 5 && cy == 5) || (cx == 7 && cy == 5);
    case 10:
        return (cx == 4 && cy == 2) || (cx == 8 && cy == 2) ||
               (cx == 6 && cy == 4) ||
               (cx == 4 && cy == 6) || (cx == 8 && cy == 6);
    default:
        return 0;
    }
}

static void brush_pixel(const Doc *d, int dx, int dy,
                        int *cr, int *cg, int *cb, int *invert)
{
    int col, row, n, x0, y0, cx, cy, cw, ink;
    *cr = CHROME_R;
    *cg = CHROME_G;
    *cb = CHROME_B;
    *invert = 0;
    if (dx == 0 || dy == 0) {
        *cr = 230;
        *cg = 230;
        *cb = 230;
        return;
    }
    if (dx == BRUSH_W - 1 || dy == BRUSH_H - 1) {
        *cr = 100;
        *cg = 100;
        *cb = 100;
        return;
    }
    row = (dy - 1) / BRUSH_CELL_H;
    if (row > 2) {
        row = 2;
    }
    if (row < 2) {
        col = (dx - 1) / BRUSH_CELL_W;
        if (col > 3) {
            col = 3;
        }
        n = row * 4 + col + 1;
        cw = BRUSH_CELL_W;
        x0 = 1 + col * BRUSH_CELL_W;
    } else {
        col = (dx - 1) >= (BRUSH_W - 2) / 2 ? 1 : 0;
        n = 9 + col;
        cw = (BRUSH_W - 2) / 2;
        x0 = 1 + col * cw;
    }
    y0 = 1 + row * BRUSH_CELL_H;
    cx = dx - x0;
    cy = dy - y0;
    if (n == d->brush_d) {
        *invert = 1;
    }
    if (cx == cw - 1 || cy == BRUSH_CELL_H - 1) {
        *cr = 100;
        *cg = 100;
        *cb = 100;
        return;
    }
    if (cx == 0 || cy == 0) {
        *cr = 230;
        *cg = 230;
        *cb = 230;
        return;
    }
    ink = brush_ink(n, cx, cy);
    if (ink) {
        *cr = 0;
        *cg = 0;
        *cb = 0;
    }
}

static void tool_color(const DpAssets *a, const Doc *d,
                       int dx, int dy, int iw, int ih, int bhh,
                       const int *row_y0, const int *row_y1,
                       int mark_row, int mark_col, int mag_row, int mag_col,
                       int *cr, int *cg, int *cb, int *invert)
{
    static const int tool_icon[8][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 18},
        {8, 9}, {10, 11}, {12, 13}, {14, 15}
    };
    int x0, x1, y0, y1, cw, ch, sca, ox, oy, lx, ly, ink, edge;
    int bcol, br, sr;
    *cr = CHROME_R;
    *cg = CHROME_G;
    *cb = CHROME_B;
    *invert = 0;
    (void)ih;
    (void)bhh;
    if (iw < 1) {
        return;
    }
    {
        bcol = dx * 2 / iw;
        if (bcol > 1) {
            bcol = 1;
        }
        sr = -1;
        for (br = 0; br < NSTRIP_ROWS; br++) {
            if (dy >= row_y0[br] && dy < row_y1[br]) {
                sr = br;
                break;
            }
        }
        if (sr < 0) {
            return;
        }
        x0 = bcol * iw / 2;
        x1 = (bcol + 1) * iw / 2;
        y0 = row_y0[sr];
        y1 = row_y1[sr];
        if ((sr == mark_row && bcol == mark_col) ||
            (sr == mag_row && bcol == mag_col) ||
            (sr == 6 && bcol == 0 && d->grid_on) ||
            (sr == 6 && bcol == 1 && d->sym_on)) {
            *invert = 1;
        }
        cw = x1 - x0;
        ch = y1 - y0;
        sca = cw / 11;
        if (ch / 11 < sca) {
            sca = ch / 11;
        }
        if (sca < 1) {
            sca = 1;
        }
        ox = x0 + (cw - 11 * sca) / 2;
        oy = y0 + (ch - 11 * sca) / 2;
        lx = dx - ox;
        ly = dy - oy;
        ink = 0;
        if (lx >= 0 && ly >= 0 && lx < 11 * sca && ly < 11 * sca) {
            int id = sr < NTOOL_ROWS ? tool_icon[sr][bcol] : (bcol == 0 ? 16 : 17);
            if (a != NULL && id < a->nicons) {
                ink = icon_on(a, id, lx / sca, ly / sca);
            } else if (sr >= NTOOL_ROWS) {
                int gx = (cw - 5) / 2;
                int gy = (ch - 7) / 2;
                ink = letter_on(bcol, dx - x0 - gx, dy - y0 - gy);
            }
        }
    }
    edge = 0;
    if (dx == x0 || dy == y0) {
        edge = 1;
    }
    if (dx == x1 - 1 || dy == y1 - 1) {
        edge = 2;
    }
    if (edge == 1) {
        *cr = 230;
        *cg = 230;
        *cb = 230;
    } else if (edge == 2) {
        *cr = 100;
        *cg = 100;
        *cb = 100;
    } else if (ink) {
        *cr = 0;
        *cg = 0;
        *cb = 0;
    }
}

static void sample_page(const Ui *u, const Doc *d, int px, int py, DpColor *c)
{
    const DpFrame *fr = &d->frames[d->cur];
    int idx = 0;
    int onion = 0;
    if (px >= 0 && py >= 0 && px < fr->w && py < fr->h && fr->pix != NULL) {
        idx = fr->pix[py * fr->w + px];
    }
    if (u->lt_on && d->nframes > 1 && idx == (uint8_t)d->bg && fr->pix != NULL) {
        int which[3];
        int n = 0;
        int k;
        if (u->lt_two) {
            which[n++] = d->cur - 2;
        }
        if (u->lt_prev) {
            which[n++] = d->cur - 1;
        }
        if (u->lt_next) {
            which[n++] = d->cur + 1;
        }
        for (k = n - 1; k >= 0; k--) {
            int fi = which[k];
            const DpFrame *src;
            int o;
            while (fi < 0) {
                fi += d->nframes;
            }
            while (fi >= d->nframes) {
                fi -= d->nframes;
            }
            src = &d->frames[fi];
            if (src->pix == NULL || src->w != fr->w || src->h != fr->h) {
                continue;
            }
            o = src->pix[py * fr->w + px];
            if (o != (uint8_t)d->bg) {
                idx = o;
                onion = 1;
                break;
            }
        }
    }
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= d->ncolors) {
        idx = d->ncolors - 1;
    }
    *c = d->pal[idx];
    if (onion && u->lt_dim) {
        DpColor bg = d->pal[d->bg >= 0 && d->bg < d->ncolors ? d->bg : 0];
        c->r = (uint8_t)((c->r + bg.r) / 2);
        c->g = (uint8_t)((c->g + bg.g) / 2);
        c->b = (uint8_t)((c->b + bg.b) / 2);
    }
}

static void panel_boxes(const Ui *u, int *bar_y, int *bar_w,
                        int *arr_l, int *arr_r, int *sl_x, int *sl_w, int *btn_x)
{
    int mh, tw, bh, vw, vh;
    ui_layout(u, &mh, &tw, &bh, &vw, &vh);
    *bar_y = mh + vh;
    *bar_w = vw;
    *arr_l = 48;
    *btn_x = vw - 4 - (PANEL_BTNS * 13 + 14);
    if (*btn_x < *arr_l + 70) {
        *btn_x = *arr_l + 70;
    }
    *arr_r = *btn_x - 16;
    *sl_x = *arr_l + 14;
    *sl_w = *arr_r - *sl_x - 2;
    if (*sl_w < 8) {
        *sl_w = 8;
    }
    (void)bar_w;
}

static int panel_btn_x(int btn_x, int i)
{
    int x = btn_x + i * 13;
    if (i >= 10) {
        x += 14;
    }
    return x;
}

static int panel_hot(const Ui *u, const Doc *d, int i)
{
    if (i == 0) {
        return d->play == 1 && d->play_dir < 0;
    }
    if (i == 1) {
        return d->play == 2 && d->play_dir < 0;
    }
    if (i == 4) {
        return d->play == 2 && d->play_dir > 0;
    }
    if (i == 5) {
        return d->play == 1 && d->play_dir > 0;
    }
    if (i == 6) {
        return d->play == 3;
    }
    if (i == 10) {
        return u->lt_on;
    }
    if (i == 11) {
        return u->lt_two;
    }
    if (i == 12) {
        return u->lt_prev;
    }
    if (i == 13) {
        return u->lt_next;
    }
    if (i == 14) {
        return u->lt_dim;
    }
    return 0;
}

static int picon_on(int kind, int x, int y)
{
    static const unsigned char bits[15][8] = {
        {0x08, 0x0c, 0x5e, 0x91, 0x83, 0x7e, 0x00, 0x00},
        {0x10, 0x30, 0x70, 0xf0, 0x70, 0x30, 0x10, 0x00},
        {0x12, 0x32, 0x72, 0xf2, 0x72, 0x32, 0x12, 0x00},
        {0x48, 0x4c, 0x4e, 0x4f, 0x4e, 0x4c, 0x48, 0x00},
        {0x08, 0x0c, 0x0e, 0x0f, 0x0e, 0x0c, 0x08, 0x00},
        {0x10, 0x30, 0x7a, 0x89, 0xc1, 0x7e, 0x00, 0x00},
        {0x42, 0x66, 0x76, 0x7e, 0x76, 0x66, 0x42, 0x00},
        {0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x09, 0x0d, 0x7f, 0x0d, 0x09, 0x00, 0x00, 0x00},
        {0x18, 0x24, 0x24, 0x18, 0x08, 0x1c, 0x08, 0x1c},
        {0x3c, 0x42, 0x06, 0x0c, 0x18, 0x22, 0x7e, 0x00},
        {0x7c, 0x42, 0x42, 0x7c, 0x40, 0x40, 0x40, 0x00},
        {0x42, 0x62, 0x52, 0x4a, 0x46, 0x42, 0x00, 0x00},
        {0x78, 0x44, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00}
    };
    if (kind < 0 || kind >= PANEL_BTNS || x < 0 || y < 0 || x >= 8 || y >= 8) {
        return 0;
    }
    return (bits[kind][y] & (0x80u >> x)) ? 1 : 0;
}

static void draw_panel(const Ui *u, const Doc *d, const DpAssets *a,
                       uint8_t *rgb, int w, int h)
{
    int s = sc(u);
    int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x;
    int i, x, y;
    char label[24];
    const DpFont *fnt;
    if (!u->panel) {
        return;
    }
    panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
    fill_rect(rgb, w, h, 0, bar_y * s, bar_w * s, PANEL_H * s, 170, 170, 170);
    fill_rect(rgb, w, h, 0, bar_y * s, bar_w * s, s, 230, 230, 230);
    fill_rect(rgb, w, h, 0, (bar_y + PANEL_H - 1) * s, bar_w * s, s, 100, 100, 100);
    snprintf(label, sizeof(label), "%d/%d", d->cur + 1, d->nframes < 1 ? 1 : d->nframes);
    fnt = a->nfonts > 0 ? &a->fonts[0] : NULL;
    if (fnt != NULL) {
        dp_font_rgb(fnt, rgb, w, h, 4 * s, (bar_y + 8) * s, label, 0, 0, 0, s);
    }
    for (i = 0; i < 2; i++) {
        int ax = i == 0 ? arr_l : arr_r;
        int hot = 0;
        fill_rect(rgb, w, h, ax * s, (bar_y + 6) * s, 12 * s, 12 * s, 170, 170, 170);
        for (y = 0; y < 7; y++) {
            int span = y <= 3 ? y : 6 - y;
            for (x = 0; x < 7; x++) {
                int on = i == 0 ? (x <= span) : (x >= 6 - span && x <= 6);
                if (on) {
                    int cr = hot ? 255 : 0;
                    fill_rect(rgb, w, h, (ax + 2 + x) * s, (bar_y + 8 + y) * s, s, s, cr, cr, cr);
                }
            }
        }
        (void)hot;
    }
    {
        int span = d->nframes < 2 ? 1 : d->nframes - 1;
        int knob = d->cur * (sl_w - 4) / span;
        if (knob < 0) {
            knob = 0;
        }
        fill_rect(rgb, w, h, sl_x * s, (bar_y + 10) * s, sl_w * s, 4 * s, 80, 80, 80);
        fill_rect(rgb, w, h, (sl_x + knob) * s, (bar_y + 7) * s, 4 * s, 10 * s, 210, 0, 0);
    }
    for (i = 0; i < PANEL_BTNS; i++) {
        int bx = panel_btn_x(btn_x, i);
        int hot = panel_hot(u, d, i);
        int bg = hot ? 40 : 170;
        fill_rect(rgb, w, h, bx * s, (bar_y + 6) * s, 12 * s, 12 * s,
                  (uint8_t)bg, (uint8_t)bg, (uint8_t)bg);
        fill_rect(rgb, w, h, bx * s, (bar_y + 6) * s, 12 * s, s, hot ? 0 : 230, hot ? 0 : 230, hot ? 0 : 230);
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                if (picon_on(i, x, y)) {
                    int cr = hot ? 255 : 0;
                    fill_rect(rgb, w, h, (bx + 2 + x) * s, (bar_y + 8 + y) * s, s, s, cr, cr, cr);
                }
            }
        }
    }
}

static void draw_hover(const Ui *u, const Doc *d, const DpAssets *a,
                       uint8_t *rgb, int w, int h);
static void draw_enh(const Ui *u, const DpAssets *a, uint8_t *rgb, int w, int h);
static void draw_arrange(const Ui *u, const Doc *d, const DpAssets *a,
                         uint8_t *rgb, int w, int h);

void ui_render(const Ui *u, const Doc *d, const DpAssets *a,
               uint8_t *rgb, int w, int h)
{
    const DpFont *f = font(a, u);
    int s = sc(u);
    int lw = log_w(u);
    int lh = log_h(u);
    int vw, vh, i;
    const DpFrame *fr;
    char line[160];
    int mh, tw, bh;

    sync_fonts(a);
    ui_layout(u, &mh, &tw, &bh, &vw, &vh);
    fill_rect(rgb, w, h, 0, 0, w, h, CHROME_R, CHROME_G, CHROME_B);
    fill_rect(rgb, w, h, 0, 0, lw * s, MENU_H * s, CHROME_R, CHROME_G, CHROME_B);

    if (u->menu_open < 0) {
        if (f != NULL) {
            const char *mode = dp_mode_name(d->mode);
            int name_w = dp_font_width(f, "DeluxePaint");
            dp_font_rgb(f, rgb, w, h, 4 * s, 1 * s, "DeluxePaint", 0, 0, 0, s);
            dp_font_rgb(f, rgb, w, h, (4 + name_w + 10) * s, 1 * s, mode, 0, 0, 0, s);
            if (!u->panel && u->enh_title && d->nframes > 1) {
                char frbuf[32];
                int mode_w = dp_font_width(f, mode);
                snprintf(frbuf, sizeof(frbuf), "Frame: %d/%d", d->cur + 1, d->nframes);
                dp_font_rgb(f, rgb, w, h, (4 + name_w + 10 + mode_w + 10) * s, 1 * s,
                            frbuf, 0, 0, 0, s);
            }
        }
    } else {
        for (i = 0; i < nbars && f != NULL; i++) {
            int tx = bar_text_x(f, i);
            int twid = dp_font_width(f, bars[i].title);
            if (u->menu_open == i) {
                fill_rect(rgb, w, h, (tx - 2) * s, 0, (twid + 4) * s, MENU_H * s,
                          255, 255, 255);
                dp_font_rgb(f, rgb, w, h, tx * s, 1 * s, bars[i].title, 0, 0, 0, s);
            } else {
                dp_font_rgb(f, rgb, w, h, tx * s, 1 * s, bars[i].title, 0, 0, 0, s);
            }
        }
    }
    {
        int gw = 7;
        int gh = 6;
        int gy = 2;
        int x2 = lw - 3 - gw;
        int x1 = x2 - 3 - gw;
        if (x1 > 0) {
            fill_rect(rgb, w, h, x1 * s, gy * s, gw * s, gh * s, 210, 0, 0);
            fill_rect(rgb, w, h, x2 * s, gy * s, gw * s, gh * s, 210, 0, 0);
            if (gw > 2 && gh > 2) {
                fill_rect(rgb, w, h, (x2 + 1) * s, (gy + 1) * s, (gw - 2) * s,
                          (gh - 2) * s, 0, 0, 0);
            }
        }
    }

    fr = shown(u, d);
    {
        DpColor paper = d->pal[d->bg >= 0 && d->bg < d->ncolors ? d->bg : 0];
        fill_rect(rgb, w, h, 0, MENU_H * s, vw * s, vh * s, paper.r, paper.g, paper.b);
    }
    if (u->magnify && fr->w > 0 && fr->h > 0) {
        int gap, left_w, ox, oy, rect_w, rect_h, right_w, sy, sx;
        mag_view(u, fr->w, fr->h, vw, vh, &left_w, &gap, &ox, &oy,
                 &rect_w, &rect_h);
        right_w = vw - left_w - gap;
        if (right_w < 1) {
            right_w = 1;
        }
        for (sy = 0; sy < vh; sy++) {
            int py = sy * fr->h / vh;
            for (sx = 0; sx < left_w; sx++) {
                int px = sx * fr->w / left_w;
                DpColor c;
                sample_page(u, d, px, py, &c);
                put_px(rgb, w, h, sx * s, (MENU_H + sy) * s, c.r, c.g, c.b);
                if (s > 1) {
                    int yy, xx;
                    for (yy = 0; yy < s; yy++) {
                        for (xx = 0; xx < s; xx++) {
                            put_px(rgb, w, h, sx * s + xx, (MENU_H + sy) * s + yy,
                                   c.r, c.g, c.b);
                        }
                    }
                }
            }
        }
        for (sy = 0; sy < vh; sy++) {
            int gx;
            for (gx = 0; gx < gap; gx++) {
                int on = (sy + gx) & 1;
                int cr = on ? 170 : 0;
                fill_rect(rgb, w, h, (left_w + gx) * s, (MENU_H + sy) * s, s, s,
                          (uint8_t)cr, (uint8_t)cr, (uint8_t)cr);
            }
        }
        {
            int x0s = ox * left_w / fr->w;
            int y0s = oy * vh / fr->h;
            int x1s = (u->mag_x1 + 1) * left_w / fr->w - 1;
            int y1s = (u->mag_y1 + 1) * vh / fr->h - 1;
            int k;
            if (x1s < x0s) {
                x1s = x0s;
            }
            if (y1s < y0s) {
                y1s = y0s;
            }
            for (k = x0s; k <= x1s; k++) {
                fill_rect(rgb, w, h, k * s, (MENU_H + y0s) * s, s, s, 255, 255, 255);
                fill_rect(rgb, w, h, k * s, (MENU_H + y1s) * s, s, s, 255, 255, 255);
            }
            for (k = y0s; k <= y1s; k++) {
                fill_rect(rgb, w, h, x0s * s, (MENU_H + k) * s, s, s, 255, 255, 255);
                fill_rect(rgb, w, h, x1s * s, (MENU_H + k) * s, s, s, 255, 255, 255);
            }
        }
        for (sy = 0; sy < vh; sy++) {
            int py = oy + sy * rect_h / vh;
            if (py < 0) {
                py = 0;
            }
            if (py >= fr->h) {
                py = fr->h - 1;
            }
            for (sx = 0; sx < right_w; sx++) {
                int px = ox + sx * rect_w / right_w;
                DpColor c;
                if (px < 0) {
                    px = 0;
                }
                if (px >= fr->w) {
                    px = fr->w - 1;
                }
                sample_page(u, d, px, py, &c);
                fill_rect(rgb, w, h, (left_w + gap + sx) * s, (MENU_H + sy) * s,
                          s, s, c.r, c.g, c.b);
            }
        }
    } else {
        for (i = 0; i < vh; i++) {
            int py = i + u->scroll_y;
            int row;
            if (py < 0 || py >= fr->h) {
                continue;
            }
            for (row = 0; row < vw; row++) {
                int px = row + u->scroll_x;
                DpColor c;
                int sx, sy;
                if (px < 0 || px >= fr->w) {
                    continue;
                }
                sample_page(u, d, px, py, &c);
                if (d->stencil_show && d->stencil != NULL &&
                    py < d->frames[d->cur].h && px < d->frames[d->cur].w &&
                    d->stencil[py * d->frames[d->cur].w + px]) {
                    c.r = 255;
                }
                for (sy = 0; sy < s; sy++) {
                    for (sx = 0; sx < s; sx++) {
                        put_px(rgb, w, h, row * s + sx, (MENU_H + i) * s + sy,
                               c.r, c.g, c.b);
                    }
                }
            }
        }
        if (u->mag_arm && fr->w > 0 && fr->h > 0) {
            int x0 = u->mag_x0;
            int y0 = u->mag_y0;
            int x1 = u->mag_x1;
            int y1 = u->mag_y1;
            int x, y;
            if (x0 > x1) {
                int t = x0;
                x0 = x1;
                x1 = t;
            }
            if (y0 > y1) {
                int t = y0;
                y0 = y1;
                y1 = t;
            }
            for (x = x0; x <= x1; x++) {
                int sx = x - u->scroll_x;
                int sy0 = y0 - u->scroll_y;
                int sy1 = y1 - u->scroll_y;
                if (sx < 0 || sx >= vw) {
                    continue;
                }
                if (sy0 >= 0 && sy0 < vh) {
                    fill_rect(rgb, w, h, sx * s, (MENU_H + sy0) * s, s, s,
                              255, 255, 255);
                }
                if (sy1 >= 0 && sy1 < vh && sy1 != sy0) {
                    fill_rect(rgb, w, h, sx * s, (MENU_H + sy1) * s, s, s,
                              255, 255, 255);
                }
            }
            for (y = y0; y <= y1; y++) {
                int sy = y - u->scroll_y;
                int sx0 = x0 - u->scroll_x;
                int sx1 = x1 - u->scroll_x;
                if (sy < 0 || sy >= vh) {
                    continue;
                }
                if (sx0 >= 0 && sx0 < vw) {
                    fill_rect(rgb, w, h, sx0 * s, (MENU_H + sy) * s, s, s,
                              255, 255, 255);
                }
                if (sx1 >= 0 && sx1 < vw && sx1 != sx0) {
                    fill_rect(rgb, w, h, sx1 * s, (MENU_H + sy) * s, s, s,
                              255, 255, 255);
                }
            }
        }
    }

    {
        int tx, twide, ix, iy, iw, ih, bhh, px0, py0, cw;
        int dx, dy, mark_row, mark_col, mag_row, mag_col, brow;
        int row_y0[NSTRIP_ROWS], row_y1[NSTRIP_ROWS];
        DpColor fgc = d->pal[d->fg >= 0 && d->fg < d->ncolors ? d->fg : 0];
        strip_metrics(u, d->ncolors, &tx, &twide, &ix, &iy, &iw, &ih, &bhh,
                      &px0, &py0, NULL, NULL, &cw);
        mark_row = -1;
        mark_col = -1;
        mag_row = -1;
        mag_col = -1;
        for (brow = 0; brow < NTOOL_ROWS; brow++) {
            int bcol;
            for (bcol = 0; bcol < 2; bcol++) {
                if (cell_kind[brow][bcol] == CELL_TOOL &&
                    cell_tool[brow][bcol] == d->tool) {
                    mark_row = brow;
                    mark_col = bcol;
                }
                if (cell_kind[brow][bcol] == CELL_MAG &&
                    (u->magnify || u->mag_arm)) {
                    mag_row = brow;
                    mag_col = bcol;
                }
            }
        }
        for (brow = 0; brow < NSTRIP_ROWS; brow++) {
            strip_row_y(ih, bhh, brow, &row_y0[brow], &row_y1[brow]);
        }
        fill_rect(rgb, w, h, tx * s, MENU_H * s, twide * s, (lh - MENU_H) * s,
                  CHROME_R, CHROME_G, CHROME_B);
        {
            int bx = ix + iw - BRUSH_W;
            int bdx, bdy;
            if (bx < 0) {
                bx = 0;
            }
            for (bdy = 0; bdy < bhh && bdy < BRUSH_H; bdy++) {
                for (bdx = 0; bdx < BRUSH_W; bdx++) {
                    int invert = 0;
                    int cr, cg, cb;
                    brush_pixel(d, bdx, bdy, &cr, &cg, &cb, &invert);
                    if (invert) {
                        cr = 255 - cr;
                        cg = 255 - cg;
                        cb = 255 - cb;
                    }
                    fill_rect(rgb, w, h, (bx + bdx) * s, (iy + bdy) * s, s, s,
                              (uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
                }
            }
        }
        for (dy = bhh; dy < ih; dy++) {
            for (dx = 0; dx < iw; dx++) {
                int invert = 0;
                int cr, cg, cb;
                tool_color(a, d, dx, dy, iw, ih, bhh, row_y0, row_y1,
                           mark_row, mark_col, mag_row, mag_col,
                           &cr, &cg, &cb, &invert);
                if (invert) {
                    cr = 255 - cr;
                    cg = 255 - cg;
                    cb = 255 - cb;
                }
                fill_rect(rgb, w, h, (ix + dx) * s, (iy + dy) * s, s, s,
                          (uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
            }
        }
        {
            int bw = iw;
            int bx, by;
            if (bw < 1) {
                bw = 1;
            }
            bx = px0;
            by = iy + ih;
            fill_rect(rgb, w, h, bx * s, by * s, bw * s, BAR_H * s, 0, 0, 0);
            if (BAR_H > 2 && bw > 2) {
                fill_rect(rgb, w, h, (bx + 1) * s, (by + 1) * s, (bw - 2) * s,
                          (BAR_H - 2) * s, fgc.r, fgc.g, fgc.b);
            }
        }
        for (i = 0; i < d->ncolors; i++) {
            int ccol = i % PAL_COLS;
            int crow = i / PAL_COLS;
            int cx = px0 + pal_col_x(ccol);
            int cy = py0 + crow * cw;
            int rows = (d->ncolors + PAL_COLS - 1) / PAL_COLS;
            if (crow >= rows) {
                break;
            }
            fill_rect(rgb, w, h, cx * s, cy * s, cw * s, cw * s,
                      d->pal[i].r, d->pal[i].g, d->pal[i].b);
        }
    }

    draw_panel(u, d, a, rgb, w, h);
    if (u->menu_open >= 0 && u->menu_open < nbars && f != NULL) {
        const MenuBar *mb = &bars[u->menu_open];
        int mw, mx, my, mhgt;
        menu_column(f, u->menu_open, lw, &mx, &my, &mw, &mhgt);
        fill_rect(rgb, w, h, mx * s, my * s, mw * s, mhgt * s, 255, 255, 255);
        for (i = 0; i < mb->n; i++) {
            int hi = (u->menu_item == i);
            if (hi) {
                fill_rect(rgb, w, h, mx * s, (my + i * ITEM_H) * s, mw * s, ITEM_H * s,
                          0, 0, 0);
            }
            if (item_checked(u, d, &mb->items[i])) {
                fill_rect(rgb, w, h, (mx + 2) * s, (my + i * ITEM_H + 3) * s,
                          4 * s, 4 * s, hi ? 255 : 0, hi ? 255 : 0, hi ? 255 : 0);
            }
            draw_item_text(f, rgb, w, h, mx, my + i * ITEM_H, mw, &mb->items[i], hi, s);
        }
        if (u->menu_item >= 0 && u->menu_item < mb->n &&
            mb->items[u->menu_item].nkids > 0) {
            const Item *it = &mb->items[u->menu_item];
            int sw, sx, sy, sh;
            submenu_column(f, u->menu_open, u->menu_item, lw, &sx, &sy, &sw, &sh);
            fill_rect(rgb, w, h, sx * s, sy * s, sw * s, sh * s, 255, 255, 255);
            for (i = 0; i < it->nkids; i++) {
                int hi = u->menu_sub == i;
                if (hi) {
                    fill_rect(rgb, w, h, sx * s, (sy + i * ITEM_H) * s, sw * s,
                              ITEM_H * s, 0, 0, 0);
                }
                if (item_checked(u, d, &it->kids[i])) {
                    fill_rect(rgb, w, h, (sx + 2) * s, (sy + i * ITEM_H + 3) * s,
                              4 * s, 4 * s, hi ? 255 : 0, hi ? 255 : 0, hi ? 255 : 0);
                }
                draw_item_text(f, rgb, w, h, sx, sy + i * ITEM_H, sw, &it->kids[i], hi, s);
            }
        }
    }

    if (u->about && f != NULL) {
        int n = 0;
        const char *const *lines = dp_about_lines(&n);
        int bw = 280;
        int bhgt = 20 + n * 12;
        int bx = (lw - bw) / 2;
        int by = (lh - bhgt) / 2;
        fill_rect(rgb, w, h, bx * s, by * s, bw * s, bhgt * s, 240, 240, 240);
        for (i = 0; i < n; i++) {
            dp_font_rgb(f, rgb, w, h, (bx + 8) * s, (by + 8 + i * 12) * s,
                        lines[i], 0, 0, 0, s);
        }
    }
    if (u->printer && f != NULL) {
        fill_rect(rgb, w, h, 40 * s, 40 * s, 180 * s, 28 * s, 240, 240, 240);
        dp_font_rgb(f, rgb, w, h, 48 * s, 48 * s, "Printer not Ready", 0, 0, 0, s);
    }
    if (u->prompt && f != NULL) {
        int rx, ry, rw, rh, okx, oky, cx, cy, bw, bh;
        const char *title = req_title(u->prompt);
        req_box(u, &rx, &ry, &rw, &rh, &okx, &oky, &cx, &cy, &bw, &bh);
        fill_rect(rgb, w, h, rx * s, ry * s, rw * s, rh * s, 204, 204, 204);
        fill_rect(rgb, w, h, rx * s, ry * s, rw * s, 12 * s, 20, 70, 180);
        dp_font_rgb(f, rgb, w, h, (rx + 6) * s, (ry + 2) * s, title, 255, 255, 255, s);
        if (file_act(u->prompt)) {
            Dent ents[180];
            int n, lx0, ly0, lw0, lh0, row;
            const char *tail = u->dir;
            n = scan_dir(u->dir, ents, 180);
            if (strlen(tail) > 32) {
                tail += strlen(tail) - 32;
            }
            dp_font_rgb(f, rgb, w, h, (rx + 8) * s, (ry + 14) * s, tail, 0, 0, 0, s);
            list_geom(u, &lx0, &ly0, &lw0, &lh0);
            fill_rect(rgb, w, h, lx0 * s, ly0 * s, lw0 * s, lh0 * s, 255, 255, 255);
            for (row = 0; row < LIST_ROWS; row++) {
                int idx = u->list_scroll + row;
                int hi = idx == u->list_sel;
                const char *nm;
                int cr, cg, cb;
                if (idx < 0 || idx >= n) {
                    break;
                }
                nm = ents[idx].is_dir && strcmp(ents[idx].name, "..") == 0
                         ? "Parent"
                         : ents[idx].name;
                if (hi) {
                    fill_rect(rgb, w, h, lx0 * s, (ly0 + row * LIST_ITEM) * s,
                              (lw0 - 10) * s, LIST_ITEM * s, 0, 0, 0);
                    cr = cg = cb = 255;
                } else if (ents[idx].is_dir) {
                    cr = 20;
                    cg = 40;
                    cb = 180;
                } else {
                    cr = cg = cb = 0;
                }
                snprintf(line, sizeof(line), "%.28s", nm);
                dp_font_rgb(f, rgb, w, h, (lx0 + 2) * s, (ly0 + row * LIST_ITEM + 1) * s,
                            line, (uint8_t)cr, (uint8_t)cg, (uint8_t)cb, s);
            }
            fill_rect(rgb, w, h, (rx + 8) * s, (oky - 16) * s, (rw - 16) * s, 12 * s,
                      255, 255, 255);
            snprintf(line, sizeof(line), "%.40s", u->text);
            dp_font_rgb(f, rgb, w, h, (rx + 10) * s, (oky - 14) * s, line, 0, 0, 0, s);
        } else if (u->prompt > 0) {
            fill_rect(rgb, w, h, (rx + 8) * s, (ry + 16) * s, (rw - 16) * s, 12 * s,
                      255, 255, 255);
            snprintf(line, sizeof(line), "%.80s", u->text);
            dp_font_rgb(f, rgb, w, h, (rx + 10) * s, (ry + 18) * s, line, 0, 0, 0, s);
        } else {
            dp_font_rgb(f, rgb, w, h, (rx + 8) * s, (ry + 18) * s,
                        "Cancel to return to the page.", 0, 0, 0, s);
        }
        fill_rect(rgb, w, h, okx * s, oky * s, bw * s, bh * s, 20, 70, 180);
        fill_rect(rgb, w, h, cx * s, cy * s, bw * s, bh * s, 20, 70, 180);
        dp_font_rgb(f, rgb, w, h, (okx + 16) * s, (oky + 2) * s, "OK", 255, 255, 255, s);
        dp_font_rgb(f, rgb, w, h, (cx + 4) * s, (cy + 2) * s, "Cancel", 255, 255, 255, s);
    }
    if (u->pointer_seen && u->over_page && !u->prompt && u->menu_open < 0 &&
        !u->about && !u->printer) {
        int k;
        int cx = u->pointer_x;
        int cy = u->pointer_y;
        for (k = -4; k <= 4; k++) {
            if (k == 0) {
                continue;
            }
            put_px(rgb, w, h, cx + k, cy, 255, 102, 0);
            put_px(rgb, w, h, cx, cy + k, 255, 102, 0);
        }
    }
    draw_hover(u, d, a, rgb, w, h);
    draw_enh(u, a, rgb, w, h);
    draw_arrange(u, d, a, rgb, w, h);
}

static int need_brush(Doc *d)
{
    if (d->brush.pix == NULL) {
        doc_status(d, "Must have user-defined brush.");
        return -1;
    }
    return 0;
}

static int page_w(const Doc *d)
{
    return d->frames[d->cur].w;
}

static int page_h(const Doc *d)
{
    return d->frames[d->cur].h;
}

static void zoom_place(Ui *u, const Doc *d, int px, int py)
{
    int pw = page_w(d);
    int ph = page_h(d);
    int rw, rh, x0, y0;
    if (pw < 1) {
        pw = 1;
    }
    if (ph < 1) {
        ph = 1;
    }
    rw = pw / 6;
    rh = ph / 5;
    if (rw < 8) {
        rw = pw < 8 ? pw : 8;
    }
    if (rh < 8) {
        rh = ph < 8 ? ph : 8;
    }
    if (rw > pw) {
        rw = pw;
    }
    if (rh > ph) {
        rh = ph;
    }
    x0 = px - rw / 2;
    y0 = py - rh / 2;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x0 + rw > pw) {
        x0 = pw - rw;
    }
    if (y0 + rh > ph) {
        y0 = ph - rh;
    }
    u->mag_x0 = x0;
    u->mag_y0 = y0;
    u->mag_x1 = x0 + rw - 1;
    u->mag_y1 = y0 + rh - 1;
}

static void mag_step(Ui *u, Doc *d, int closer)
{
    int rw, rh, cx, cy, pw, ph;
    if (!u->magnify) {
        return;
    }
    pw = page_w(d);
    ph = page_h(d);
    rw = u->mag_x1 - u->mag_x0 + 1;
    rh = u->mag_y1 - u->mag_y0 + 1;
    cx = u->mag_x0 + rw / 2;
    cy = u->mag_y0 + rh / 2;
    if (closer) {
        rw = rw * 2 / 3;
        rh = rh * 2 / 3;
        if (rw < 4) {
            rw = 4;
        }
        if (rh < 4) {
            rh = 4;
        }
    } else {
        rw = rw * 3 / 2;
        rh = rh * 3 / 2;
        if (rw > pw) {
            rw = pw;
        }
        if (rh > ph) {
            rh = ph;
        }
    }
    u->mag_x0 = cx - rw / 2;
    u->mag_y0 = cy - rh / 2;
    if (u->mag_x0 < 0) {
        u->mag_x0 = 0;
    }
    if (u->mag_y0 < 0) {
        u->mag_y0 = 0;
    }
    if (u->mag_x0 + rw > pw) {
        u->mag_x0 = pw - rw;
    }
    if (u->mag_y0 + rh > ph) {
        u->mag_y0 = ph - rh;
    }
    u->mag_x1 = u->mag_x0 + rw - 1;
    u->mag_y1 = u->mag_y0 + rh - 1;
    doc_status(d, closer ? "Zoom in" : "Zoom out");
}

static void grid_snap(const Doc *d, int *x, int *y)
{
    if (!d->grid_on || x == NULL || y == NULL) {
        return;
    }
    *x = (*x / 8) * 8;
    *y = (*y / 8) * 8;
}

static void stencil_make(Doc *d)
{
    int w = page_w(d);
    int h = page_h(d);
    int i, n = w * h;
    uint8_t *m = (uint8_t *)malloc((size_t)n);
    if (m == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        m[i] = d->frames[d->cur].pix[i] == (uint8_t)d->fg ? 1 : 0;
    }
    free(d->stencil);
    d->stencil = m;
    d->stencil_on = 1;
    doc_status(d, "Make Stencil");
}

static int form_is_anim(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char id[12];
    if (fp == NULL) {
        return 0;
    }
    if (fread(id, 1, 12, fp) != 12) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return memcmp(id, "FORM", 4) == 0 && memcmp(id + 8, "ANIM", 4) == 0;
}

static void load_picture(Doc *d, const char *path)
{
    DpFrame fr;
    DpColor pal[DP_MAX_COLORS];
    DpRange rg[DP_MAX_RANGES];
    int nc = 0, camg = 0, nr = 0;
    char err[120];
    memset(&fr, 0, sizeof(fr));
    memset(pal, 0, sizeof(pal));
    memset(rg, 0, sizeof(rg));
    err[0] = 0;
    if (iff_load_ilbm(path, &fr, pal, &nc, &camg, rg, &nr,
                      NULL, NULL, NULL, NULL, err, (int)sizeof(err)) != 0) {
        doc_status(d, err[0] ? err : "Can't Load");
        return;
    }
    if (doc_adopt_frame(d, &fr, pal, nc, camg, rg, nr) != 0) {
        free(fr.pix);
        doc_status(d, "Not enough memory");
        return;
    }
    doc_status(d, "Load");
}

static void png_sibling(const char *path, char *out, int cap)
{
    const char *slash, *dot;
    int n;
    snprintf(out, (size_t)cap, "%s", path);
    slash = strrchr(out, '/');
    slash = slash != NULL ? slash + 1 : out;
    dot = strrchr(slash, '.');
    if (dot != NULL && (strcmp(dot, ".ilbm") == 0 || strcmp(dot, ".iff") == 0 ||
                        strcmp(dot, ".lbm") == 0 || strcmp(dot, ".pic") == 0)) {
        out[dot - out] = 0;
    }
    n = (int)strlen(out);
    if (n + 4 < cap) {
        memcpy(out + n, ".png", 5);
    }
}

static int used_slot(const Doc *d)
{
    int m = 0;
    int i;
    if (d->fg > m) {
        m = d->fg;
    }
    if (d->bg > m) {
        m = d->bg;
    }
    for (i = 0; i < d->nranges; i++) {
        if (d->ranges[i].high > m) {
            m = d->ranges[i].high;
        }
    }
    return m;
}

static void save_picture(Doc *d, const char *path)
{
    char png[512];
    const DpFrame *fr = &d->frames[d->cur];
    if (iff_save_ilbm(path, fr, d->pal, d->ncolors, d->camg,
                      d->ranges, d->nranges, 0, 0, 0, 0, 1, used_slot(d)) != 0) {
        doc_status(d, "Can't Save");
        return;
    }
    png_sibling(path, png, (int)sizeof(png));
    if (iff_write_png(png, fr->pix, fr->w, fr->h, d->pal, d->ncolors) != 0) {
        doc_status(d, "Can't write PNG");
        return;
    }
    doc_status(d, "Save");
}

static void load_anim(Doc *d, const char *path)
{
    DpFrame frames[DP_MAX_FRAMES];
    DpColor pal[DP_MAX_COLORS];
    DpRange rg[DP_MAX_RANGES];
    int n = 0, nc = 0, camg = 0, nr = 0, fps = 12;
    char err[120];
    memset(frames, 0, sizeof(frames));
    err[0] = 0;
    if (iff_load_anim(path, frames, DP_MAX_FRAMES, &n, pal, &nc, &camg,
                      rg, &nr, &fps, err, (int)sizeof(err)) != 0) {
        doc_status(d, err[0] ? err : "Can't Load");
        return;
    }
    if (doc_adopt_anim(d, frames, n, pal, nc, camg, rg, nr, fps) != 0) {
        int i;
        for (i = 0; i < n; i++) {
            free(frames[i].pix);
        }
        doc_status(d, "Not enough memory");
        return;
    }
    doc_status(d, "Load");
}

static void save_anim(Doc *d, const char *path)
{
    if (iff_save_anim(path, d->frames, d->nframes, d->pal, d->ncolors, d->camg,
                      d->ranges, d->nranges, d->fps, d->method_compressed,
                      0, 0, 0, used_slot(d)) != 0) {
        doc_status(d, "Can't Save");
        return;
    }
    doc_status(d, "Save");
}

static void load_brush(Doc *d, const char *path)
{
    DpFrame fr;
    DpColor pal[DP_MAX_COLORS];
    int nc = 0, grab = 0, gx = 0, gy = 0, trans = 0;
    char err[120];
    memset(&fr, 0, sizeof(fr));
    err[0] = 0;
    if (iff_load_ilbm(path, &fr, pal, &nc, NULL, NULL, NULL,
                      &grab, &gx, &gy, &trans, err, (int)sizeof(err)) != 0) {
        doc_status(d, err[0] ? err : "Can't Load");
        return;
    }
    brush_free(&d->brush);
    d->brush.pix = fr.pix;
    d->brush.w = fr.w;
    d->brush.h = fr.h;
    d->brush.trans = (uint8_t)trans;
    d->brush.hx = grab ? gx : fr.w / 2;
    d->brush.hy = grab ? gy : fr.h / 2;
    d->brush.ncolors = nc;
    d->brush.has_pal = nc > 0;
    memcpy(d->brush.pal, pal, sizeof(pal));
    if (brush_clone(&d->brush_orig, &d->brush) == 0) {
        d->brush_orig_ok = 1;
    }
    doc_status(d, "Load");
}

static void save_brush(Doc *d, const char *path)
{
    DpFrame fr;
    if (need_brush(d) != 0) {
        return;
    }
    fr.w = d->brush.w;
    fr.h = d->brush.h;
    fr.pix = d->brush.pix;
    if (iff_save_ilbm(path, &fr, d->brush.has_pal ? d->brush.pal : d->pal,
                      d->brush.has_pal ? d->brush.ncolors : d->ncolors,
                      d->camg, NULL, 0, 1, d->brush.hx, d->brush.hy,
                      d->brush.trans, 1, 0) != 0) {
        doc_status(d, "Can't Save");
        return;
    }
    doc_status(d, "Save");
}

static void pick_animbrush(Doc *d)
{
    int i, n, x, y;
    if (!d->pick_ok) {
        doc_status(d, "Must have user-defined brush.");
        return;
    }
    n = d->nframes - d->cur;
    if (n > 16) {
        n = 16;
    }
    if (n < 1) {
        return;
    }
    for (i = 0; i < d->nabrush; i++) {
        brush_free(&d->abrush[i]);
    }
    d->nabrush = 0;
    for (i = 0; i < n; i++) {
        DpBrush *b = &d->abrush[i];
        uint8_t *p = (uint8_t *)malloc((size_t)d->pick_w * (size_t)d->pick_h);
        if (p == NULL) {
            break;
        }
        memset(p, 0, (size_t)d->pick_w * (size_t)d->pick_h);
        for (y = 0; y < d->pick_h; y++) {
            int sy = d->pick_y + y;
            if (sy < 0 || sy >= d->frames[d->cur + i].h) {
                continue;
            }
            for (x = 0; x < d->pick_w; x++) {
                int sx = d->pick_x + x;
                if (sx < 0 || sx >= d->frames[d->cur + i].w) {
                    continue;
                }
                p[y * d->pick_w + x] =
                    d->frames[d->cur + i].pix[sy * d->frames[d->cur + i].w + sx];
            }
        }
        memset(b, 0, sizeof(*b));
        b->pix = p;
        b->w = d->pick_w;
        b->h = d->pick_h;
        b->hx = d->brush.hx;
        b->hy = d->brush.hy;
        b->trans = d->brush.trans;
        d->nabrush++;
    }
    d->abrush_i = 0;
    doc_status(d, "Pick up AnimBrush");
}

static int parse_pair(const char *s, int *a, int *b)
{
    char x = 0;
    if (sscanf(s, "%d %c %d", a, &x, b) == 3) {
        return 0;
    }
    if (sscanf(s, "%d %d", a, b) == 2) {
        return 0;
    }
    return -1;
}

static void begin_prompt(Ui *u, Doc *d, int act, const char *label)
{
    u->prompt = act;
    u->text[0] = 0;
    u->text_len = 0;
    u->menu_open = -1;
    u->menu_hold = 0;
    u->menu_item = -1;
    u->menu_sub = -1;
    u->stroking = 0;
    u->gesture = GS_NONE;
    u->list_scroll = 0;
    u->list_sel = -1;
    if (file_act(act)) {
        if (u->lastdir[0] != 0) {
            snprintf(u->dir, sizeof(u->dir), "%s", u->lastdir);
        } else if (getcwd(u->dir, sizeof(u->dir)) == NULL) {
            snprintf(u->dir, sizeof(u->dir), ".");
        }
    }
    doc_status(d, label);
}

static void finish_prompt(Ui *u, Doc *d)
{
    int act = u->prompt;
    int n = 0, a = 0, b = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s", u->text);
    if (file_act(act) && path[0] != 0 && path[0] != '/' && u->dir[0] != 0) {
        char joined[512];
        snprintf(joined, sizeof(joined), "%s/%s", u->dir, path);
        snprintf(path, sizeof(path), "%s", joined);
    }
    u->prompt = 0;
    switch (act) {
    case A_LOAD_PIC:
        if (form_is_anim(path)) {
            load_anim(d, path);
        } else {
            load_picture(d, path);
        }
        u->scroll_x = 0;
        u->scroll_y = 0;
        break;
    case A_SAVE_PIC:
        save_picture(d, path);
        break;
    case A_DELETE:
        if (remove(path) != 0) {
            doc_status(d, "No such file found.");
        } else {
            doc_status(d, "Delete");
        }
        break;
    case A_LOAD_BR:
        load_brush(d, path);
        break;
    case A_SAVE_BR:
        save_brush(d, path);
        break;
    case A_LOAD_ANIM:
    case A_AB_LOAD:
        load_anim(d, path);
        if (act == A_LOAD_ANIM && d->nframes > 1) {
            u->panel = 1;
        }
        break;
    case A_SAVE_ANIM:
        save_anim(d, path);
        break;
    case A_AB_SAVE:
        if (d->nabrush < 1) {
            doc_status(d, "Must have an AnimBrush");
        } else {
            DpFrame tmp[16];
            int i;
            for (i = 0; i < d->nabrush; i++) {
                tmp[i].w = d->abrush[i].w;
                tmp[i].h = d->abrush[i].h;
                tmp[i].pix = d->abrush[i].pix;
            }
            if (iff_save_anim(path, tmp, d->nabrush, d->pal, d->ncolors, d->camg,
                              NULL, 0, d->fps, 1, 1, d->abrush[0].hx,
                              d->abrush[0].hy, 0) != 0) {
                doc_status(d, "Can't Save");
            } else {
                doc_status(d, "Save");
            }
        }
        break;
    case A_PAGE:
        if (parse_pair(path, &a, &b) == 0 && a >= 1 && b >= 1) {
            if (doc_resize(d, a, b, d->ncolors) == 0) {
                doc_status(d, "Set Page Size");
            }
        }
        break;
    case A_STRETCH:
        if (need_brush(d) == 0 && parse_pair(path, &a, &b) == 0) {
            brush_scale(&d->brush, a, b);
            doc_status(d, "Stretch");
        }
        break;
    case A_ADD:
    case A_COPYF:
        n = atoi(path);
        if (n < 1) {
            n = 1;
        }
        doc_add_frames(d, n, act == A_ADD ? 0 : 0);
        if (d->nframes > 1) {
            u->panel = 1;
        }
        break;
    case A_DELF:
        n = atoi(path);
        doc_delete_frames(d, n < 1 ? 1 : n);
        break;
    case A_SETN:
        n = atoi(path);
        if (n < 1) {
            n = 1;
        }
        while (d->nframes < n && d->nframes < DP_MAX_FRAMES) {
            if (doc_add_frames(d, 1, 1) != 0) {
                break;
            }
        }
        while (d->nframes > n) {
            d->cur = d->nframes - 1;
            if (doc_delete_frames(d, 1) != 0) {
                break;
            }
        }
        doc_status(d, "Set # ..");
        if (d->nframes > 1) {
            u->panel = 1;
        }
        break;
    case A_RATE:
        n = atoi(path);
        if (n < 1) {
            n = 1;
        }
        if (n > 60) {
            n = 60;
        }
        d->fps = n;
        doc_status(d, "Set Rate..");
        break;
    case A_GOTO:
        n = atoi(path);
        doc_goto(d, n - 1);
        doc_status(d, "Go to Frame");
        break;
    case A_RANGE:
        if (parse_pair(path, &a, &b) == 0) {
            if (a > b) {
                int t = a;
                a = b;
                b = t;
            }
            d->range_a = a - 1;
            d->range_b = b - 1;
            if (d->range_a < 0) {
                d->range_a = 0;
            }
            if (d->range_b >= d->nframes) {
                d->range_b = d->nframes - 1;
            }
            doc_status(d, "Set Play Range");
        }
        break;
    case A_ST_LOAD: {
        uint8_t *m = NULL;
        int w = 0, h = 0;
        char err[80];
        err[0] = 0;
        if (iff_load_stencil(path, &m, &w, &h, err, (int)sizeof(err)) != 0) {
            doc_status(d, err[0] ? err : "Not a stencil file");
        } else if (w != page_w(d) || h != page_h(d)) {
            free(m);
            doc_status(d, "Wrong size stencil");
        } else {
            free(d->stencil);
            d->stencil = m;
            d->stencil_on = 1;
            doc_status(d, "Stencil");
        }
        break;
    }
    case A_ST_SAVE:
        if (d->stencil == NULL) {
            doc_status(d, "No Stencil is defined");
        } else if (iff_save_stencil(path, d->stencil, page_w(d), page_h(d)) != 0) {
            doc_status(d, "Can't Save");
        } else {
            doc_status(d, "Save");
        }
        break;
    case A_NONE:
    default:
        if (act == 9001) {
            break;
        }
        break;
    }
    if (act == 8001 || act == 8002) {
        uint8_t buf[DP_DPPS_LEN];
        char err[80];
        if (act == 8002) {
            if (iff_save_prsp(path, d->dpps) != 0) {
                doc_status(d, "Can't Save");
            } else {
                doc_status(d, "Save");
            }
        } else if (iff_load_prsp(path, buf, err, (int)sizeof(err)) != 0) {
            doc_status(d, err[0] ? err : "Bad IFF Form");
        } else {
            memcpy(d->dpps, buf, DP_DPPS_LEN);
            doc_status(d, "Load");
        }
    }
    if ((act == A_LOAD_PIC || act == A_SAVE_PIC || act == A_LOAD_BR ||
         act == A_SAVE_BR || act == A_LOAD_ANIM || act == A_SAVE_ANIM ||
         act == A_AB_LOAD || act == A_AB_SAVE) &&
        (strcmp(d->status, "Load") == 0 || strcmp(d->status, "Save") == 0)) {
        char kept[512];
        char *slash;
        snprintf(kept, sizeof(kept), "%s", path);
        slash = strrchr(kept, '/');
        if (slash != NULL) {
            if (slash == kept) {
                kept[1] = 0;
            } else {
                *slash = 0;
            }
            snprintf(u->lastdir, sizeof(u->lastdir), "%s", kept);
        }
    }
}

static void do_act(Ui *u, Doc *d, DpAssets *a, int act, int arg)
{
    int i, n;
    u->menu_open = -1;
    switch (act) {
    case A_NONE:
        begin_prompt(u, d, -1, "Not in this build");
        break;
    case A_LOAD_PIC:
        begin_prompt(u, d, act, "Load");
        break;
    case A_SAVE_PIC:
        begin_prompt(u, d, act, "Save");
        break;
    case A_DELETE:
        begin_prompt(u, d, act, "Delete File");
        break;
    case A_PRINT:
        u->printer = 1;
        break;
    case A_FLIP_H:
        doc_checkpoint(d);
        doc_flip_page(d, 1);
        doc_status(d, "Horiz");
        break;
    case A_FLIP_V:
        doc_checkpoint(d);
        doc_flip_page(d, 0);
        doc_status(d, "Vert");
        break;
    case A_SWAP:
        doc_swap_spare(d);
        break;
    case A_COPY_SPARE:
        doc_copy_spare(d);
        break;
    case A_MERGE_F:
        doc_checkpoint(d);
        doc_merge(d, 1);
        break;
    case A_MERGE_B:
        doc_checkpoint(d);
        doc_merge(d, 0);
        break;
    case A_DEL_PAGE:
        doc_delete_frames(d, 1);
        break;
    case A_PAGE:
        begin_prompt(u, d, act, "Type in size:");
        break;
    case A_SHOW:
        u->show_spare = !u->show_spare;
        doc_status(d, "Show Page");
        break;
    case A_FMT_LO:
        doc_set_format(d, FMT_LO);
        break;
    case A_FMT_MED:
        doc_set_format(d, FMT_MED);
        break;
    case A_FMT_LACE:
        doc_set_format(d, FMT_LACE);
        break;
    case A_FMT_HI:
        doc_set_format(d, FMT_HI);
        break;
    case A_ABOUT:
        u->about = 1;
        break;
    case A_QUIT:
        u->quit = 1;
        break;
    case A_LOAD_BR:
        begin_prompt(u, d, act, "Load");
        break;
    case A_SAVE_BR:
        begin_prompt(u, d, act, "Save");
        break;
    case A_RESTORE:
        if (d->brush_orig_ok && brush_clone(&d->brush, &d->brush_orig) == 0) {
            doc_status(d, "Restore");
        } else {
            doc_status(d, "Must have user-defined brush.");
        }
        break;
    case A_BR_TO:
        if (need_brush(d) == 0 && brush_clone(&d->brush_spare, &d->brush) == 0) {
            d->brush_spare_ok = 1;
            doc_status(d, "Brush->Spare");
        }
        break;
    case A_BR_SWAP:
        if (d->brush_spare_ok) {
            DpBrush t = d->brush;
            d->brush = d->brush_spare;
            d->brush_spare = t;
            doc_status(d, "Brush<->Spare");
        }
        break;
    case A_STRETCH:
        begin_prompt(u, d, act, "Width:");
        break;
    case A_HALVE:
        if (need_brush(d) == 0) {
            brush_scale(&d->brush, d->brush.w / 2 > 0 ? d->brush.w / 2 : 1,
                        d->brush.h / 2 > 0 ? d->brush.h / 2 : 1);
            doc_status(d, "Halve");
        }
        break;
    case A_DOUBLE:
        if (need_brush(d) == 0) {
            brush_scale(&d->brush, d->brush.w * 2, d->brush.h * 2);
            doc_status(d, "Double");
        }
        break;
    case A_DUBH:
        if (need_brush(d) == 0) {
            brush_scale(&d->brush, d->brush.w * 2, d->brush.h);
            doc_status(d, "Double Horiz");
        }
        break;
    case A_DUBV:
        if (need_brush(d) == 0) {
            brush_scale(&d->brush, d->brush.w, d->brush.h * 2);
            doc_status(d, "Double Vert");
        }
        break;
    case A_BFLIP_H:
        if (need_brush(d) == 0) {
            brush_flip(&d->brush, 1);
            doc_status(d, "Horiz");
        }
        break;
    case A_BFLIP_V:
        if (need_brush(d) == 0) {
            brush_flip(&d->brush, 0);
            doc_status(d, "Vert");
        }
        break;
    case A_OUTLINE:
        if (need_brush(d) == 0) {
            brush_outline(&d->brush, d->fg);
            doc_status(d, "Outline");
        }
        break;
    case A_ROT90:
        if (need_brush(d) == 0) {
            brush_rot90(&d->brush);
            doc_status(d, "90 Degrees");
        }
        break;
    case A_HANDLE_C:
        d->handle_center = 1;
        if (d->brush.pix) {
            d->brush.hx = d->brush.w / 2;
            d->brush.hy = d->brush.h / 2;
        }
        doc_status(d, "Center");
        break;
    case A_HANDLE_K:
        d->handle_center = 0;
        if (d->brush.pix) {
            d->brush.hx = 0;
            d->brush.hy = 0;
        }
        doc_status(d, "Corner");
        break;
    case A_MODE:
        d->mode = arg;
        doc_status(d, dp_mode_name(arg));
        break;
    case A_LOAD_ANIM:
        begin_prompt(u, d, act, "Load");
        break;
    case A_SAVE_ANIM:
        begin_prompt(u, d, act, "Save");
        break;
    case A_ADD:
        begin_prompt(u, d, act, "# of Frames:");
        break;
    case A_COPYF:
        begin_prompt(u, d, act, "Copy frames..");
        break;
    case A_DELF:
        begin_prompt(u, d, act, "Delete frames..");
        break;
    case A_SETN:
        begin_prompt(u, d, act, "Count:");
        break;
    case A_RATE:
        begin_prompt(u, d, act, "Set frames per second");
        break;
    case A_RANGE:
        begin_prompt(u, d, act, "Set Play Range");
        break;
    case A_PREV:
        doc_goto(d, d->cur - 1);
        doc_status(d, "Previous");
        break;
    case A_NEXT:
        doc_goto(d, d->cur + 1);
        doc_status(d, "Next");
        break;
    case A_GOTO:
        begin_prompt(u, d, act, "number:");
        break;
    case A_PLAY:
        d->play = d->play == 1 ? 0 : 1;
        d->play_dir = 1;
        doc_status(d, d->play ? "Play" : "Play");
        break;
    case A_ONCE:
        d->play = 2;
        d->play_dir = 1;
        doc_status(d, "Play once");
        break;
    case A_PING:
        d->play = 3;
        d->play_dir = 1;
        doc_status(d, "Ping-pong");
        break;
    case A_PANEL_ON:
        u->panel = 1;
        doc_status(d, "Panel On");
        break;
    case A_PANEL_OFF:
        u->panel = 0;
        doc_status(d, "Panel Off");
        break;
    case A_COMP:
        d->method_compressed = 1;
        doc_status(d, "Compressed");
        break;
    case A_EXP:
        d->method_compressed = 0;
        doc_status(d, "Expanded");
        break;
    case A_AB_PICK:
        pick_animbrush(d);
        break;
    case A_AB_FREE:
        for (i = 0; i < d->nabrush; i++) {
            brush_free(&d->abrush[i]);
        }
        d->nabrush = 0;
        doc_status(d, "Free");
        break;
    case A_AB_LOAD:
        begin_prompt(u, d, act, "Load");
        break;
    case A_AB_SAVE:
        begin_prompt(u, d, act, "Save");
        break;
    case A_ST_MAKE:
    case A_ST_REMAKE:
        stencil_make(d);
        break;
    case A_ST_PAINT:
    case A_ST_ON:
        d->stencil_on = !d->stencil_on;
        doc_status(d, d->stencil_on ? "On/Off" : "On/Off");
        break;
    case A_ST_SHOW:
        d->stencil_show = !d->stencil_show;
        doc_status(d, "Show");
        break;
    case A_ST_REV:
        if (d->stencil != NULL) {
            n = page_w(d) * page_h(d);
            for (i = 0; i < n; i++) {
                d->stencil[i] = d->stencil[i] ? 0 : 1;
            }
            doc_status(d, "Reverse");
        } else {
            doc_status(d, "No Stencil is defined");
        }
        break;
    case A_ST_FREE:
        free(d->stencil);
        d->stencil = NULL;
        d->stencil_on = 0;
        doc_status(d, "Free");
        break;
    case A_ST_LOAD:
        begin_prompt(u, d, act, "Load");
        break;
    case A_ST_SAVE:
        begin_prompt(u, d, act, "Save");
        break;
    case A_CYCLE:
        if (d->nranges < 1) {
            d->nranges = 1;
            d->ranges[0].low = 20;
            d->ranges[0].high = d->ncolors - 1;
        }
        d->ranges[0].active = !d->ranges[0].active;
        doc_status(d, "Cycle");
        break;
    case A_USEPAL:
        if (d->brush.has_pal) {
            memcpy(d->pal, d->brush.pal, sizeof(d->pal));
            d->ncolors = d->brush.ncolors > 1 ? d->brush.ncolors : d->ncolors;
            doc_status(d, "Use Brush Palette");
        }
        break;
    case A_RESTORE_PAL:
        memcpy(d->pal, d->pal_saved, sizeof(d->pal));
        doc_status(d, "Restore Palette");
        break;
    case A_DEFAULT_PAL:
        if (a != NULL && a->pal_ok) {
            memcpy(d->pal, a->pal, sizeof(d->pal));
            doc_status(d, "Default Palette");
        }
        break;
    case A_BG_FG:
        doc_checkpoint(d);
        n = page_w(d) * page_h(d);
        for (i = 0; i < n; i++) {
            if (d->frames[d->cur].pix[i] == (uint8_t)d->bg) {
                d->frames[d->cur].pix[i] = (uint8_t)d->fg;
            }
        }
        doc_status(d, "BG -> FG");
        break;
    case A_SWAP_FG:
        doc_checkpoint(d);
        n = page_w(d) * page_h(d);
        for (i = 0; i < n; i++) {
            uint8_t c = d->frames[d->cur].pix[i];
            if (c == (uint8_t)d->bg) {
                d->frames[d->cur].pix[i] = (uint8_t)d->fg;
            } else if (c == (uint8_t)d->fg) {
                d->frames[d->cur].pix[i] = (uint8_t)d->bg;
            }
        }
        doc_status(d, "BG <-> FG");
        break;
    case A_REMAP:
        doc_status(d, "Remap ");
        break;
    case A_TRANS:
        if (d->brush.pix) {
            d->brush.trans = (uint8_t)d->fg;
            doc_status(d, "Change Transparency");
        }
        break;
    case A_FONT:
        if (a != NULL && arg >= 0 && arg < a->nfonts) {
            u->font_i = arg;
            doc_status(d, a->fonts[arg].face[0] ? a->fonts[arg].face : "Choose Font");
        }
        break;
    case A_PREF:
        u->prefs ^= arg;
        doc_status(d, "Prefs");
        break;
    case A_VER2026:
        u->enh_edit_hover = u->enh_hover;
        u->enh_edit_undo = u->enh_undo;
        u->enh_edit_title = u->enh_title;
        u->enh_open = 1;
        break;
    case A_ARRANGE:
        memcpy(u->arr_open, d->pal, sizeof(d->pal));
        u->arr_undo_ok = 0;
        u->arr_top = 0;
        u->arr_sel = d->fg;
        u->arr_op = 0;
        u->arr_from = d->fg;
        u->arrange = 1;
        doc_status(d, "Arrange");
        break;
    default:
        break;
    }
}

static int hit_bar(const Ui *u, const DpAssets *a, int lx)
{
    const DpFont *f = font(a, u);
    int i;
    if (f == NULL) {
        return -1;
    }
    for (i = 0; i < nbars; i++) {
        int x = bar_text_x(f, i);
        int twid = dp_font_width(f, bars[i].title) + TITLE_GAP;
        if (lx >= x && lx < x + twid) {
            return i;
        }
    }
    return -1;
}

static void mag_view(const Ui *u, int pw, int ph, int vw, int vh,
                     int *left_w, int *gap, int *ox, int *oy,
                     int *rwdt, int *rhgt)
{
    int x0 = u->mag_x0;
    int y0 = u->mag_y0;
    int x1 = u->mag_x1;
    int y1 = u->mag_y1;
    int rw, rh;
    *gap = 4;
    *left_w = vw / 2;
    if (*left_w < 1) {
        *left_w = 1;
    }
    if (*left_w > vw - 5) {
        *left_w = vw - 5;
    }
    if (*left_w < 1) {
        *left_w = 1;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (pw > 0 && x1 >= pw) {
        x1 = pw - 1;
    }
    if (ph > 0 && y1 >= ph) {
        y1 = ph - 1;
    }
    if (x1 < x0) {
        x1 = x0;
    }
    if (y1 < y0) {
        y1 = y0;
    }
    rw = x1 - x0 + 1;
    rh = y1 - y0 + 1;
    if (rw < 1) {
        rw = 1;
    }
    if (rh < 1) {
        rh = 1;
    }
    *ox = x0;
    *oy = y0;
    if (rwdt) {
        *rwdt = rw;
    }
    if (rhgt) {
        *rhgt = rh;
    }
    (void)vh;
}

static void page_pos(const Ui *u, const Doc *d, int mx, int my, int *px, int *py, int *over)
{
    int s = sc(u);
    int lx = mx / s;
    int ly = my / s;
    int vw, vh, mh, tw, bh;
    int pw, ph;
    ui_layout(u, &mh, &tw, &bh, &vw, &vh);
    pw = d->frames[d->cur].w;
    ph = d->frames[d->cur].h;
    *over = 0;
    if (u->magnify && pw > 0 && ph > 0 &&
        lx >= 0 && ly >= MENU_H && lx < vw && ly < MENU_H + vh) {
        int left_w, gap, ox, oy, rect_w, rect_h, right_w;
        mag_view(u, pw, ph, vw, vh, &left_w, &gap, &ox, &oy, &rect_w, &rect_h);
        if (lx < left_w) {
            *px = lx * pw / left_w;
            *py = (ly - MENU_H) * ph / vh;
            if (*py >= ph) {
                *py = ph - 1;
            }
            *over = 1;
            return;
        }
        right_w = vw - left_w - gap;
        if (right_w < 1) {
            right_w = 1;
        }
        if (lx >= left_w + gap) {
            int relx = lx - left_w - gap;
            int rely = ly - MENU_H;
            if (relx < 0) {
                relx = 0;
            }
            if (rely < 0) {
                rely = 0;
            }
            *px = ox + relx * rect_w / right_w;
            *py = oy + rely * rect_h / vh;
            if (*px >= pw) {
                *px = pw - 1;
            }
            if (*py >= ph) {
                *py = ph - 1;
            }
            *over = 1;
            return;
        }
    }
    if (lx >= 0 && ly >= MENU_H && lx < vw && ly < MENU_H + vh) {
        *px = lx + u->scroll_x;
        *py = (ly - MENU_H) + u->scroll_y;
        *over = 1;
    } else {
        *px = 0;
        *py = 0;
    }
}

static void bezier(Doc *d, int x0, int y0, int cx, int cy, int x1, int y1, int color)
{
    int i, px = x0, py = y0;
    for (i = 1; i <= 32; i++) {
        int t = i;
        int u = 32 - i;
        int x = (u * u * x0 + 2 * u * t * cx + t * t * x1) / (32 * 32);
        int y = (u * u * y0 + 2 * u * t * cy + t * t * y1) / (32 * 32);
        doc_line(d, px, py, x, y, color);
        px = x;
        py = y;
    }
}

static void paint_move(Ui *u, Doc *d, const DpAssets *a, int px, int py)
{
    int color = u->use_bg ? d->bg : d->fg;
    const DpFont *f;
    if (u->gesture == GS_FREE) {
        if (d->mode == MODE_SMEAR) {
            doc_smear(d, u->lx, u->ly, px, py);
        } else if (d->tool == TOOL_DOT) {
            int dx = px - u->lx;
            int dy = py - u->ly;
            if (dx * dx + dy * dy >= 4 || (px == u->ax && py == u->ay)) {
                doc_dab(d, px, py, color);
                u->lx = px;
                u->ly = py;
            }
        } else if (d->tool == TOOL_DRAW) {
            doc_line(d, u->lx, u->ly, px, py, color);
            u->lx = px;
            u->ly = py;
        } else if (d->tool == TOOL_AIR) {
            doc_air(d, px, py, color);
            u->lx = px;
            u->ly = py;
        }
        return;
    }
    doc_preview_restore(d);
    if (u->gesture == GS_LINE) {
        doc_line(d, u->ax, u->ay, px, py, color);
    } else if (u->gesture == GS_RECT) {
        doc_rect(d, u->ax, u->ay, px, py, color, d->tool == TOOL_FRECT);
    } else if (u->gesture == GS_CIRC) {
        int rx = px - u->ax;
        int ry = py - u->ay;
        int rad;
        if (rx < 0) {
            rx = -rx;
        }
        if (ry < 0) {
            ry = -ry;
        }
        rad = rx > ry ? rx : ry;
        doc_ellipse(d, u->ax, u->ay, rad, rad, color, 0);
    } else if (u->gesture == GS_ELLIP) {
        int rx = px - u->ax;
        int ry = py - u->ay;
        if (rx < 0) {
            rx = -rx;
        }
        if (ry < 0) {
            ry = -ry;
        }
        doc_ellipse(d, u->ax, u->ay, rx, ry, color, 0);
    } else if (u->gesture == GS_CURVE) {
        doc_line(d, u->ax, u->ay, px, py, color);
    } else if (u->gesture == GS_BEND) {
        bezier(d, u->curve_x0, u->curve_y0, px, py, u->curve_x1, u->curve_y1, color);
    } else if (u->gesture == GS_PICK) {
        doc_rect(d, u->ax, u->ay, px, py, color, 0);
    }
    if (d->tool == TOOL_TEXT && u->text_on) {
        f = text_font(a, u);
        if (f != NULL) {
            dp_font_index(f, d->frames[d->cur].pix, page_w(d), page_h(d),
                          u->text_px, u->text_py - f->baseline, u->text, (uint8_t)color);
        }
    }
}

static void start_gesture(Ui *u, Doc *d, int px, int py)
{
    doc_checkpoint(d);
    u->stroking = 1;
    u->ax = u->lx = px;
    u->ay = u->ly = py;
    if (d->tool == TOOL_LINE) {
        u->gesture = GS_LINE;
    } else if (d->tool == TOOL_RECT || d->tool == TOOL_FRECT) {
        u->gesture = GS_RECT;
    } else if (d->tool == TOOL_CIRCLE) {
        u->gesture = GS_CIRC;
    } else if (d->tool == TOOL_ELLIPSE) {
        u->gesture = GS_ELLIP;
    } else if (d->tool == TOOL_CURVE) {
        if (u->gesture == GS_BEND) {
            return;
        }
        u->gesture = GS_CURVE;
    } else if (d->tool == TOOL_BRUSH) {
        u->gesture = GS_PICK;
    } else if (d->tool == TOOL_FILL) {
        u->gesture = GS_NONE;
        doc_fill(d, px, py, u->use_bg ? d->bg : d->fg);
        u->stroking = 0;
    } else if (d->tool == TOOL_TEXT) {
        u->gesture = GS_NONE;
        u->text_on = 1;
        u->text_px = px;
        u->text_py = py;
        u->text[0] = 0;
        u->text_len = 0;
        u->stroking = 0;
    } else {
        u->gesture = GS_FREE;
        if (d->tool == TOOL_AIR) {
            doc_air(d, px, py, u->use_bg ? d->bg : d->fg);
        } else if (d->mode != MODE_SMEAR) {
            doc_dab(d, px, py, u->use_bg ? d->bg : d->fg);
        }
    }
}

static int menu_pick(const Ui *u, const DpAssets *a, int lx, int ly, int *act, int *arg)
{
    const DpFont *f = font(a, u);
    const MenuBar *mb;
    int mx0, my0, mw, mhgt, it;
    if (u->menu_open < 0 || u->menu_open >= nbars || f == NULL) {
        return 0;
    }
    mb = &bars[u->menu_open];
    if (u->menu_item >= 0 && u->menu_item < mb->n && mb->items[u->menu_item].nkids > 0) {
        int sx, sy, sw, sh, k;
        submenu_column(f, u->menu_open, u->menu_item, log_w(u), &sx, &sy, &sw, &sh);
        if (lx >= sx && lx < sx + sw && ly >= sy && ly < sy + sh) {
            k = (ly - sy) / ITEM_H;
            if (k < 0) {
                k = 0;
            }
            if (k >= mb->items[u->menu_item].nkids) {
                k = mb->items[u->menu_item].nkids - 1;
            }
            *act = mb->items[u->menu_item].kids[k].act;
            *arg = mb->items[u->menu_item].kids[k].arg;
            return 1;
        }
    }
    menu_column(f, u->menu_open, log_w(u), &mx0, &my0, &mw, &mhgt);
    if (lx >= mx0 && lx < mx0 + mw && ly >= my0 && ly < my0 + mhgt) {
        const Item *item;
        it = (ly - my0) / ITEM_H;
        if (it < 0 || it >= mb->n) {
            return 0;
        }
        item = &mb->items[it];
        if (item->nkids > 0) {
            return 0;
        }
        *act = item->act;
        *arg = item->arg;
        return 1;
    }
    return 0;
}

static int palette_index_at(const Ui *u, const Doc *d, int lx, int ly)
{
    int px, py, pw, ph, cw, ch, cols, rows, col, row, idx;
    chrome_geom(u, d->ncolors, NULL, NULL, NULL, &px, &py, &pw, &ph, &cw, &ch, &cols, &rows);
    if (cw < 1 || ch < 1) {
        return -1;
    }
    if (lx < px || ly < py || lx >= px + pw || ly >= py + ph) {
        return -1;
    }
    col = pal_col_at(lx - px);
    row = (ly - py) / ch;
    if (col < 0 || row < 0 || col >= cols || row >= rows) {
        return -1;
    }
    idx = row * cols + col;
    if (idx < 0 || idx >= d->ncolors) {
        return -1;
    }
    return idx;
}

static int brush_size_at(const Ui *u, int lx, int ly)
{
    int ix, iy, iw, ih, bh, bx, rx, ry, col, row;
    strip_metrics(u, 32, NULL, NULL, &ix, &iy, &iw, &ih, &bh,
                  NULL, NULL, NULL, NULL, NULL);
    if (iw < 1 || bh < 1) {
        return 0;
    }
    bx = ix + iw - BRUSH_W;
    if (bx < 0) {
        bx = 0;
    }
    if (lx < bx || lx >= bx + BRUSH_W || ly < iy || ly >= iy + BRUSH_H) {
        return 0;
    }
    rx = lx - bx;
    ry = ly - iy;
    if (rx < 1) {
        rx = 1;
    }
    if (ry < 1) {
        ry = 1;
    }
    if (rx > BRUSH_W - 2) {
        rx = BRUSH_W - 2;
    }
    if (ry > BRUSH_H - 2) {
        ry = BRUSH_H - 2;
    }
    row = (ry - 1) / BRUSH_CELL_H;
    if (row > 2) {
        row = 2;
    }
    if (row >= 2) {
        return (rx - 1) < (BRUSH_W - 2) / 2 ? 9 : 10;
    }
    col = (rx - 1) / BRUSH_CELL_W;
    if (col > 3) {
        col = 3;
    }
    return row * 4 + col + 1;
}

static int tool_cell_at(const Ui *u, int lx, int ly, int *tool, int *undo, int *clearb, int *special)
{
    int ix, iy, iw, ih, bh, rel, row, col, kind, y0, y1;
    *tool = -1;
    *undo = 0;
    *clearb = 0;
    *special = 0;
    strip_metrics(u, 32, NULL, NULL, &ix, &iy, &iw, &ih, &bh,
                  NULL, NULL, NULL, NULL, NULL);
    if (iw < 1 || ih < 1) {
        return 0;
    }
    if (lx < ix || lx >= ix + iw || ly < iy + bh || ly >= iy + ih) {
        return 0;
    }
    rel = ly - iy;
    col = (lx - ix) * 2 / iw;
    if (col < 0) {
        col = 0;
    }
    if (col > 1) {
        col = 1;
    }
    for (row = 0; row < NSTRIP_ROWS; row++) {
        strip_row_y(ih, bh, row, &y0, &y1);
        if (rel >= y0 && rel < y1) {
            break;
        }
    }
    if (row >= NSTRIP_ROWS) {
        return 0;
    }
    if (row < NTOOL_ROWS) {
        kind = cell_kind[row][col];
        if (kind == CELL_TOOL) {
            *tool = cell_tool[row][col];
        } else if (kind == CELL_MAG) {
            *special = CELL_MAG;
        } else if (kind == CELL_GRID) {
            *special = CELL_GRID;
        } else if (kind == CELL_SYM) {
            *special = CELL_SYM;
        } else if (kind == CELL_ZOOM) {
            *special = CELL_ZOOM;
        } else {
            *special = CELL_NA;
        }
        return 1;
    }
    if (col == 0) {
        *undo = 1;
    } else {
        *clearb = 1;
    }
    return 1;
}

static int file_click(Ui *u, Doc *d, int lx, int ly)
{
    Dent ents[180];
    int n, x, y, w, h, row, idx;
    if (!file_act(u->prompt)) {
        return 0;
    }
    n = scan_dir(u->dir, ents, 180);
    list_geom(u, &x, &y, &w, &h);
    if (lx < x || lx >= x + w || ly < y || ly >= y + h) {
        return 0;
    }
    if (lx >= x + w - 10) {
        if (ly < y + h / 2) {
            if (u->list_scroll > 0) {
                u->list_scroll--;
            }
        } else if (u->list_scroll + LIST_ROWS < n) {
            u->list_scroll++;
        }
        return 1;
    }
    row = (ly - y) / LIST_ITEM;
    idx = u->list_scroll + row;
    if (idx < 0 || idx >= n) {
        return 1;
    }
    if (ents[idx].is_dir) {
        dir_enter(u, ents[idx].name);
        return 1;
    }
    if (u->list_sel == idx && strcmp(u->text, ents[idx].name) == 0) {
        finish_prompt(u, d);
        return 1;
    }
    u->list_sel = idx;
    snprintf(u->text, sizeof(u->text), "%s", ents[idx].name);
    u->text_len = (int)strlen(u->text);
    return 1;
}

static void list_move(Ui *u, int delta)
{
    Dent ents[180];
    int n = scan_dir(u->dir, ents, 180);
    int i = u->list_sel;
    if (n < 1) {
        return;
    }
    if (i < 0) {
        i = 0;
    } else {
        i += delta;
    }
    if (i < 0) {
        i = 0;
    }
    if (i >= n) {
        i = n - 1;
    }
    u->list_sel = i;
    if (!ents[i].is_dir) {
        snprintf(u->text, sizeof(u->text), "%s", ents[i].name);
        u->text_len = (int)strlen(u->text);
    }
    if (i < u->list_scroll) {
        u->list_scroll = i;
    }
    if (i >= u->list_scroll + LIST_ROWS) {
        u->list_scroll = i - LIST_ROWS + 1;
    }
}

static int path_is_dir(const char *path)
{
    struct stat st;
    return path != NULL && path[0] != 0 && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void open_listed_dir(Ui *u, const char *path)
{
    char *resolved = realpath(path, NULL);
    if (resolved != NULL) {
        snprintf(u->dir, sizeof(u->dir), "%s", resolved);
        free(resolved);
    } else {
        snprintf(u->dir, sizeof(u->dir), "%s", path);
    }
    u->list_scroll = 0;
    u->list_sel = -1;
    clear_field(u);
}

static void accept_prompt(Ui *u, Doc *d)
{
    Dent ents[180];
    int n;
    char path[512];
    if (!file_act(u->prompt)) {
        finish_prompt(u, d);
        return;
    }
    n = scan_dir(u->dir, ents, 180);
    if (u->list_sel >= 0 && u->list_sel < n && ents[u->list_sel].is_dir) {
        dir_enter(u, ents[u->list_sel].name);
        return;
    }
    if (u->list_sel >= 0 && u->list_sel < n) {
        snprintf(u->text, sizeof(u->text), "%s", ents[u->list_sel].name);
        u->text_len = (int)strlen(u->text);
        finish_prompt(u, d);
        return;
    }
    if (u->text[0] == 0) {
        finish_prompt(u, d);
        return;
    }
    if (u->text[0] == '/') {
        snprintf(path, sizeof(path), "%s", u->text);
    } else {
        snprintf(path, sizeof(path), "%s/%s", u->dir, u->text);
    }
    if (path_is_dir(path)) {
        open_listed_dir(u, path);
        return;
    }
    finish_prompt(u, d);
}

#define HV_BRUSH 1
#define HV_UNDO 16
#define HV_CLR 17
#define HV_GRID 18
#define HV_SYM 19
#define HV_MAG 20
#define HV_ZOOM 21
#define HV_TOOL 32
#define HV_PANEL 64
#define HV_FIRST 80
#define HV_LAST 81
#define HV_FRAME 82
#define HV_COLOR 100

static void cell_origin(const Ui *u, int row, int col, int *x, int *y)
{
    int ix, iy, iw, ih, bh, y0, y1;
    strip_metrics(u, 32, NULL, NULL, &ix, &iy, &iw, &ih, &bh,
                  NULL, NULL, NULL, NULL, NULL);
    strip_row_y(ih, bh, row, &y0, &y1);
    *x = ix + (iw / 2) * col;
    *y = iy + y0;
}

static int find_tool_cell(int tool, int *row, int *col)
{
    int r, c;
    for (r = 0; r < NTOOL_ROWS; r++) {
        for (c = 0; c < 2; c++) {
            if (cell_kind[r][c] == CELL_TOOL && cell_tool[r][c] == tool) {
                *row = r;
                *col = c;
                return 1;
            }
        }
    }
    return 0;
}

static int hover_id_at(const Ui *u, const Doc *d, int lx, int ly)
{
    int bsize, tool, undo, clearb, special, idx;
    int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x, i;
    if (u->prompt || u->about || u->printer || u->menu_open >= 0 || u->enh_open ||
        u->arrange) {
        return 0;
    }
    bsize = brush_size_at(u, lx, ly);
    if (bsize > 0) {
        return HV_BRUSH + bsize;
    }
    tool = -1;
    undo = 0;
    clearb = 0;
    special = 0;
    if (tool_cell_at(u, lx, ly, &tool, &undo, &clearb, &special)) {
        if (undo) {
            return HV_UNDO;
        }
        if (clearb) {
            return HV_CLR;
        }
        if (special == CELL_GRID) {
            return HV_GRID;
        }
        if (special == CELL_SYM) {
            return HV_SYM;
        }
        if (special == CELL_MAG) {
            return HV_MAG;
        }
        if (special == CELL_ZOOM) {
            return HV_ZOOM;
        }
        if (tool >= 0) {
            return HV_TOOL + tool;
        }
        return 0;
    }
    idx = palette_index_at(u, d, lx, ly);
    if (idx >= 0) {
        return HV_COLOR + idx;
    }
    if (!u->panel) {
        return 0;
    }
    panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
    if (ly < bar_y || ly >= bar_y + PANEL_H || lx < 0 || lx >= bar_w) {
        return 0;
    }
    if (lx >= arr_l && lx < arr_l + 12) {
        return HV_FIRST;
    }
    if (lx >= arr_r && lx < arr_r + 12) {
        return HV_LAST;
    }
    if (lx >= sl_x && lx < sl_x + sl_w) {
        return HV_FRAME;
    }
    for (i = 0; i < PANEL_BTNS; i++) {
        int bx = panel_btn_x(btn_x, i);
        if (lx >= bx && lx < bx + 12) {
            return HV_PANEL + i;
        }
    }
    return 0;
}

static const char *tool_tip(int tool)
{
    switch (tool) {
    case TOOL_DOT:
        return "Dotted";
    case TOOL_DRAW:
        return "Continuous";
    case TOOL_LINE:
        return "Line";
    case TOOL_CURVE:
        return "Curve";
    case TOOL_FILL:
        return "Fill";
    case TOOL_AIR:
        return "Airbrush";
    case TOOL_RECT:
        return "Rectangle";
    case TOOL_FRECT:
        return "Filled Rectangle";
    case TOOL_CIRCLE:
        return "Circle";
    case TOOL_ELLIPSE:
        return "Ellipse";
    case TOOL_BRUSH:
        return "Brush (b)";
    case TOOL_TEXT:
        return "Text";
    default:
        return NULL;
    }
}

static int hover_info(const Ui *u, const Doc *d, int id,
                      char *text, int n, int *ax, int *ay, int *above)
{
    static const char *panel_tip[PANEL_BTNS] = {
        "Play backward (shift+4)",
        "Play backward once (shift+5)",
        "Previous (,)",
        "Next (.)",
        "Play once (5)",
        "Play (4)",
        "Ping-pong (6)",
        "Add frame (+)",
        "Delete frame (-)",
        "Go to frame (3)",
        "Light table",
        "Two back",
        "Table previous",
        "Table next",
        "Dim"
    };
    *above = 0;
    if (id <= 0 || text == NULL || n < 8) {
        return 0;
    }
    if (id > HV_BRUSH && id < HV_BRUSH + 11) {
        int size = id - HV_BRUSH;
        int ix, iy, iw, ih, bh, bx, col, row;
        snprintf(text, (size_t)n, "Size %d", size);
        strip_metrics(u, 32, NULL, NULL, &ix, &iy, &iw, &ih, &bh,
                      NULL, NULL, NULL, NULL, NULL);
        bx = ix + iw - BRUSH_W;
        if (bx < 0) {
            bx = 0;
        }
        if (size >= 9) {
            *ax = bx + 1 + (size - 9) * ((BRUSH_W - 2) / 2);
            *ay = iy + 1 + 2 * BRUSH_CELL_H;
            return 1;
        }
        col = (size - 1) % 4;
        row = (size - 1) / 4;
        *ax = bx + 1 + col * BRUSH_CELL_W;
        *ay = iy + 1 + row * BRUSH_CELL_H;
        return 1;
    }
    if (id == HV_UNDO || id == HV_CLR || id == HV_GRID || id == HV_SYM ||
        id == HV_MAG || id == HV_ZOOM || (id >= HV_TOOL && id < HV_TOOL + TOOL_COUNT)) {
        int row = 0, col = 0;
        const char *tip = NULL;
        if (id == HV_UNDO) {
            tip = "Undo (ctrl+z)";
            row = NTOOL_ROWS;
            col = 0;
        } else if (id == HV_CLR) {
            tip = "Clear";
            row = NTOOL_ROWS;
            col = 1;
        } else if (id == HV_GRID) {
            tip = "Grid";
            row = 6;
            col = 0;
        } else if (id == HV_SYM) {
            tip = "Symmetry (/)";
            row = 6;
            col = 1;
        } else if (id == HV_MAG) {
            tip = "Magnify";
            row = 7;
            col = 0;
        } else if (id == HV_ZOOM) {
            tip = "Zoom (>)";
            row = 7;
            col = 1;
        } else {
            tip = tool_tip(id - HV_TOOL);
            if (tip == NULL || !find_tool_cell(id - HV_TOOL, &row, &col)) {
                return 0;
            }
        }
        snprintf(text, (size_t)n, "%s", tip);
        cell_origin(u, row, col, ax, ay);
        return 1;
    }
    if (id == HV_FIRST || id == HV_LAST || id == HV_FRAME ||
        (id >= HV_PANEL && id < HV_PANEL + PANEL_BTNS)) {
        int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x;
        panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
        *above = 1;
        if (id == HV_FIRST) {
            snprintf(text, (size_t)n, "%s", "First (shift+1)");
            *ax = arr_l;
            *ay = bar_y + 6;
        } else if (id == HV_LAST) {
            snprintf(text, (size_t)n, "%s", "Last (shift+2)");
            *ax = arr_r;
            *ay = bar_y + 6;
        } else if (id == HV_FRAME) {
            snprintf(text, (size_t)n, "%s", "Frame");
            *ax = sl_x;
            *ay = bar_y + 6;
        } else {
            int i = id - HV_PANEL;
            snprintf(text, (size_t)n, "%s", panel_tip[i]);
            *ax = panel_btn_x(btn_x, i);
            *ay = bar_y + 6;
        }
        return 1;
    }
    if (id >= HV_COLOR) {
        int idx = id - HV_COLOR;
        int px, py, cw;
        if (idx < 0 || idx >= d->ncolors) {
            return 0;
        }
        snprintf(text, (size_t)n, "color %d", idx);
        chrome_geom(u, d->ncolors, NULL, NULL, NULL, &px, &py, NULL, NULL,
                    &cw, NULL, NULL, NULL);
        *ax = px + pal_col_x(idx % PAL_COLS);
        *ay = py + (idx / PAL_COLS) * cw;
        return 1;
    }
    return 0;
}

static void hover_track(Ui *u, const Doc *d, int mx, int my)
{
    int s = sc(u);
    int id;
    if (s < 1) {
        s = 1;
    }
    id = hover_id_at(u, d, mx / s, my / s);
    if (id != u->hover_id) {
        u->hover_id = id;
        u->hover_ms = 0;
    }
}

static void draw_hover(const Ui *u, const Doc *d, const DpAssets *a,
                       uint8_t *rgb, int w, int h)
{
    char text[64];
    int ax, ay, above, tw, bw, bh, bx, by, lw, lh, s;
    const DpFont *f;
    if (!u->enh_hover || u->hover_id == 0 || u->hover_ms < 400) {
        return;
    }
    if (u->prompt || u->about || u->printer || u->menu_open >= 0 || u->enh_open ||
        u->arrange) {
        return;
    }
    if (!hover_info(u, d, u->hover_id, text, (int)sizeof(text), &ax, &ay, &above)) {
        return;
    }
    f = font(a, u);
    if (f == NULL || f->ysize < 1) {
        return;
    }
    s = sc(u);
    tw = dp_font_width(f, text);
    bw = tw + 4;
    bh = f->ysize + 2;
    lw = log_w(u);
    lh = log_h(u);
    if (above) {
        bx = ax + 12 - bw;
        by = ay - bh - 1;
    } else {
        bx = ax - bw - 2;
        by = ay;
    }
    if (bx < 1) {
        bx = 1;
    }
    if (by < 1) {
        by = 1;
    }
    if (bx + bw > lw - 1) {
        bx = lw - bw - 1;
    }
    if (by + bh > lh - 1) {
        by = lh - bh - 1;
    }
    if (bx < 0) {
        bx = 0;
    }
    if (by < 0) {
        by = 0;
    }
    fill_rect(rgb, w, h, bx * s, by * s, bw * s, bh * s, 0, 0, 0);
    if (bw > 2 && bh > 2) {
        fill_rect(rgb, w, h, (bx + 1) * s, (by + 1) * s, (bw - 2) * s,
                  (bh - 2) * s, 255, 255, 255);
    }
    dp_font_rgb(f, rgb, w, h, (bx + 2) * s, (by + 1) * s, text, 0, 0, 0, s);
}

static const char *const enh_name[3] = {
    "Hover help",
    "Undo history",
    "Title frame readout"
};

static void enh_geom(const Ui *u, int *bx, int *by, int *bw, int *bh,
                     int *okx, int *oky, int *cx, int *cy, int *bbw, int *bbh)
{
    int lw = log_w(u);
    int lh = log_h(u);
    *bw = 196;
    *bh = 86;
    *bx = (lw - *bw) / 2;
    if (*bx < 4) {
        *bx = 4;
    }
    *by = (lh - *bh) / 2;
    if (*by < 12) {
        *by = 12;
    }
    *bbw = 44;
    *bbh = 12;
    *oky = *by + *bh - 18;
    *cy = *oky;
    *okx = *bx + 14;
    *cx = *bx + *bw - 14 - *bbw;
}

static int enh_flag(const Ui *u, int i)
{
    if (i == 0) {
        return u->enh_edit_hover;
    }
    if (i == 1) {
        return u->enh_edit_undo;
    }
    return u->enh_edit_title;
}

static void enh_toggle(Ui *u, int i)
{
    int *p = &u->enh_edit_title;
    if (i == 0) {
        p = &u->enh_edit_hover;
    } else if (i == 1) {
        p = &u->enh_edit_undo;
    }
    *p = !*p;
}

static void enh_apply(Ui *u, Doc *d)
{
    int was = u->enh_undo;
    u->enh_hover = u->enh_edit_hover ? 1 : 0;
    u->enh_undo = u->enh_edit_undo ? 1 : 0;
    u->enh_title = u->enh_edit_title ? 1 : 0;
    if (u->enh_undo != was) {
        doc_set_undo_hist(d, u->enh_undo);
    }
    u->enh_open = 0;
}

static void draw_enh(const Ui *u, const DpAssets *a, uint8_t *rgb, int w, int h)
{
    const DpFont *f;
    int s, i;
    int bx = 0, by = 0, bw = 0, bh = 0;
    int okx = 0, oky = 0, cx = 0, cy = 0, bbw = 0, bbh = 0;
    if (!u->enh_open) {
        return;
    }
    f = font(a, u);
    if (f == NULL) {
        return;
    }
    s = sc(u);
    enh_geom(u, &bx, &by, &bw, &bh, &okx, &oky, &cx, &cy, &bbw, &bbh);
    fill_rect(rgb, w, h, bx * s, by * s, bw * s, bh * s, 204, 204, 204);
    fill_rect(rgb, w, h, bx * s, by * s, bw * s, 12 * s, 20, 70, 180);
    dp_font_rgb(f, rgb, w, h, (bx + 6) * s, (by + 2) * s, "2026 version",
                255, 255, 255, s);
    for (i = 0; i < 3; i++) {
        int y = by + 18 + i * 16;
        int on = enh_flag(u, i);
        fill_rect(rgb, w, h, (bx + 8) * s, y * s, 9 * s, 9 * s, 255, 255, 255);
        fill_rect(rgb, w, h, (bx + 8) * s, y * s, 9 * s, s, 0, 0, 0);
        fill_rect(rgb, w, h, (bx + 8) * s, (y + 8) * s, 9 * s, s, 0, 0, 0);
        fill_rect(rgb, w, h, (bx + 8) * s, y * s, s, 9 * s, 0, 0, 0);
        fill_rect(rgb, w, h, (bx + 16) * s, y * s, s, 9 * s, 0, 0, 0);
        if (on) {
            fill_rect(rgb, w, h, (bx + 10) * s, (y + 2) * s, 5 * s, 5 * s, 0, 0, 0);
        }
        dp_font_rgb(f, rgb, w, h, (bx + 22) * s, (y + 1) * s, enh_name[i],
                    0, 0, 0, s);
    }
    fill_rect(rgb, w, h, okx * s, oky * s, bbw * s, bbh * s, 20, 70, 180);
    fill_rect(rgb, w, h, cx * s, cy * s, bbw * s, bbh * s, 20, 70, 180);
    dp_font_rgb(f, rgb, w, h, (okx + 16) * s, (oky + 2) * s, "OK", 255, 255, 255, s);
    dp_font_rgb(f, rgb, w, h, (cx + 4) * s, (cy + 2) * s, "Cancel", 255, 255, 255, s);
}

static int enh_click(Ui *u, Doc *d, int lx, int ly)
{
    int bx = 0, by = 0, bw = 0, bh = 0;
    int okx = 0, oky = 0, cx = 0, cy = 0, bbw = 0, bbh = 0, i;
    enh_geom(u, &bx, &by, &bw, &bh, &okx, &oky, &cx, &cy, &bbw, &bbh);
    if (lx >= okx && lx < okx + bbw && ly >= oky && ly < oky + bbh) {
        enh_apply(u, d);
        return 1;
    }
    if (lx >= cx && lx < cx + bbw && ly >= cy && ly < cy + bbh) {
        u->enh_open = 0;
        return 1;
    }
    for (i = 0; i < 3; i++) {
        int y = by + 18 + i * 16;
        if (lx >= bx + 8 && lx < bx + bw - 8 && ly >= y && ly < y + 12) {
            enh_toggle(u, i);
            return 1;
        }
    }
    return 1;
}

static void place_pointer(Ui *u, Doc *d, int mx, int my)
{
    int px, py, over;
    page_pos(u, d, mx, my, &px, &py, &over);
    u->pointer_x = mx;
    u->pointer_y = my;
    u->pointer_seen = 1;
    u->over_page = over && px >= 0 && py >= 0 && px < page_w(d) && py < page_h(d);
    u->cursor_cross = (u->over_page && !u->prompt && u->menu_open < 0 &&
                       !u->about && !u->printer && !u->enh_open && !u->arrange) ? 1 : 0;
    if (u->over_page) {
        u->mpx = px;
        u->mpy = py;
    }
    hover_track(u, d, mx, my);
}

static void panel_slider(Doc *d, int lx, int sl_x, int sl_w)
{
    int span = d->nframes < 1 ? 1 : d->nframes;
    int fi;
    if (sl_w < 1) {
        sl_w = 1;
    }
    fi = (lx - sl_x) * span / sl_w;
    if (fi < 0) {
        fi = 0;
    }
    if (fi >= d->nframes) {
        fi = d->nframes - 1;
    }
    doc_goto(d, fi);
}

static void panel_do(Ui *u, Doc *d, int i)
{
    if (i == 0) {
        if (d->play == 1 && d->play_dir < 0) {
            d->play = 0;
        } else {
            d->play = 1;
            d->play_dir = -1;
            d->play_acc = 0;
        }
    } else if (i == 1) {
        d->play = 2;
        d->play_dir = -1;
        d->play_acc = 0;
    } else if (i == 2) {
        int n = d->cur - 1;
        if (n < 0) {
            n = d->nframes - 1;
        }
        doc_goto(d, n);
    } else if (i == 3) {
        int n = d->cur + 1;
        if (n >= d->nframes) {
            n = 0;
        }
        doc_goto(d, n);
    } else if (i == 4) {
        d->play = 2;
        d->play_dir = 1;
        d->play_acc = 0;
    } else if (i == 5) {
        if (d->play == 1 && d->play_dir > 0) {
            d->play = 0;
        } else {
            d->play = 1;
            d->play_dir = 1;
            d->play_acc = 0;
        }
    } else if (i == 6) {
        if (d->play == 3) {
            d->play = 0;
        } else {
            d->play = 3;
            d->play_dir = 1;
            d->play_acc = 0;
        }
    } else if (i == 7) {
        doc_add_frames(d, 1, 0);
    } else if (i == 8) {
        doc_delete_frames(d, 1);
    } else if (i == 9) {
        begin_prompt(u, d, A_GOTO, "number:");
    } else if (i == 10) {
        u->lt_on = !u->lt_on;
        if (u->lt_on && !u->lt_prev && !u->lt_next && !u->lt_two) {
            u->lt_prev = 1;
        }
    } else if (i == 11) {
        u->lt_two = !u->lt_two;
        if (u->lt_two) {
            u->lt_on = 1;
        }
    } else if (i == 12) {
        u->lt_prev = !u->lt_prev;
        if (u->lt_prev) {
            u->lt_on = 1;
        }
    } else if (i == 13) {
        u->lt_next = !u->lt_next;
        if (u->lt_next) {
            u->lt_on = 1;
        }
    } else if (i == 14) {
        u->lt_dim = !u->lt_dim;
    }
}

static int panel_hit(Ui *u, Doc *d, int lx, int ly)
{
    int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x, i;
    if (!u->panel) {
        return 0;
    }
    panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
    if (ly < bar_y || ly >= bar_y + PANEL_H || lx < 0 || lx >= bar_w) {
        return 0;
    }
    if (lx >= arr_l && lx < arr_l + 12) {
        doc_goto(d, 0);
        return 1;
    }
    if (lx >= arr_r && lx < arr_r + 12) {
        doc_goto(d, d->nframes > 0 ? d->nframes - 1 : 0);
        return 1;
    }
    if (lx >= sl_x && lx < sl_x + sl_w) {
        panel_slider(d, lx, sl_x, sl_w);
        u->panel_drag = 1;
        return 1;
    }
    for (i = 0; i < PANEL_BTNS; i++) {
        int bx = panel_btn_x(btn_x, i);
        if (lx >= bx && lx < bx + 12) {
            panel_do(u, d, i);
            return 1;
        }
    }
    return 1;
}

int ui_panel_point(const Ui *u, int which, int *x, int *y)
{
    int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x;
    if (u == NULL || x == NULL || y == NULL) {
        return -1;
    }
    panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
    if (which >= 0 && which < PANEL_BTNS) {
        *x = panel_btn_x(btn_x, which) + 6;
        *y = bar_y + 12;
        return 0;
    }
    if (which == 20) {
        *x = sl_x + sl_w / 2;
        *y = bar_y + 12;
        return 0;
    }
    if (which == 21) {
        *x = arr_l + 6;
        *y = bar_y + 12;
        return 0;
    }
    if (which == 22) {
        *x = arr_r + 6;
        *y = bar_y + 12;
        return 0;
    }
    return -1;
}

static const char *const arr_lbl[8] = {
    "COPY", "EX", "SPREAD", "PICK", "DELETE", "REVERT", "UNDO", "OK"
};

static int arr_limit(const Doc *d)
{
    int n = d->ncolors - 32;
    if (n < 0) {
        n = 0;
    }
    return (n / 16) * 16;
}

static void arr_geom(const Ui *u, int *x, int *y, int *w, int *h,
                     int *cell, int *wx, int *wy)
{
    int lw = log_w(u);
    int lh = log_h(u);
    int c = (lw - 56) / 16;
    if (c < 6) {
        c = 6;
    }
    if (c > 16) {
        c = 16;
    }
    *cell = c;
    *w = lw;
    *h = 12 + c * 2 + 16;
    *x = 0;
    *y = lh - *h;
    if (*y < MENU_H + 2) {
        *y = MENU_H + 2;
    }
    *wx = 40;
    *wy = *y + 12;
}

static void arr_btns(const Ui *u, const DpFont *f, int y, int h,
                     int *bx, int *bw, int *by, int *bh)
{
    int i, x;
    int lw = log_w(u);
    *bh = 12;
    *by = y + h - 15;
    x = 4;
    for (i = 0; i < 5; i++) {
        int tw = f != NULL ? dp_font_width(f, arr_lbl[i]) + 6 : 28;
        bx[i] = x;
        bw[i] = tw;
        x += tw + 3;
    }
    x = lw - 4;
    for (i = 7; i >= 5; i--) {
        int tw = f != NULL ? dp_font_width(f, arr_lbl[i]) + 8 : 32;
        x -= tw;
        bx[i] = x;
        bw[i] = tw;
        x -= 4;
    }
}

static void arr_remember(Ui *u, const Doc *d)
{
    memcpy(u->arr_undo, d->pal, sizeof(d->pal));
    u->arr_undo_ok = 1;
}

static void arr_delete(Doc *d, int a, int b)
{
    int lo, hi, count, n, i;
    if (a > b) {
        int t = a;
        a = b;
        b = t;
    }
    if (a < 0) {
        a = 0;
    }
    if (b >= d->ncolors) {
        b = d->ncolors - 1;
    }
    if (a > b) {
        return;
    }
    lo = a;
    hi = b;
    count = hi - lo + 1;
    n = d->ncolors - hi - 1;
    if (n > 0) {
        memmove(&d->pal[lo], &d->pal[hi + 1], (size_t)n * sizeof(DpColor));
    }
    for (i = d->ncolors - count; i < d->ncolors; i++) {
        d->pal[i].r = 0;
        d->pal[i].g = 0;
        d->pal[i].b = 0;
    }
}

static void arr_apply_well(Ui *u, Doc *d, int idx)
{
    if (u->arr_op == 1) {
        arr_remember(u, d);
        d->pal[idx] = d->pal[u->arr_from];
        doc_status(d, "COPY");
    } else if (u->arr_op == 2) {
        DpColor t;
        arr_remember(u, d);
        t = d->pal[idx];
        d->pal[idx] = d->pal[u->arr_from];
        d->pal[u->arr_from] = t;
        doc_status(d, "EX");
    } else if (u->arr_op == 3) {
        arr_remember(u, d);
        doc_pal_spread(d, u->arr_from, idx);
        doc_status(d, "SPREAD");
    } else if (u->arr_op == 5) {
        arr_remember(u, d);
        arr_delete(d, u->arr_from, idx);
        doc_status(d, "DELETE");
    }
    u->arr_op = 0;
    u->arr_sel = idx;
    if (idx >= 0 && idx < d->ncolors) {
        d->fg = idx;
    }
}

static int arr_click(Ui *u, Doc *d, DpAssets *a, int lx, int ly, int button)
{
    int x, y, w, h, cell, wx, wy, i, idx;
    int bx[8], bw[8], by, bh;
    const DpFont *f = font(a, u);
    arr_geom(u, &x, &y, &w, &h, &cell, &wx, &wy);
    if (u->arr_op == 4 && ly < y) {
        int px, py, over;
        page_pos(u, d, lx * sc(u), ly * sc(u), &px, &py, &over);
        if (over && px >= 0 && py >= 0 && px < page_w(d) && py < page_h(d)) {
            idx = d->frames[d->cur].pix[py * page_w(d) + px];
            if (idx < 0 || idx >= d->ncolors) {
                idx = 0;
            }
            if (button == 3) {
                d->bg = idx;
            } else {
                d->fg = idx;
            }
            u->arr_sel = idx;
            u->arr_op = 0;
            u->arr_top = (idx / 16) * 16;
            if (u->arr_top > arr_limit(d)) {
                u->arr_top = arr_limit(d);
            }
            doc_status(d, "PICK");
            return 1;
        }
    }
    if (lx >= wx && lx < wx + cell * 16 && ly >= wy && ly < wy + cell * 2) {
        int col = (lx - wx) / cell;
        int row = (ly - wy) / cell;
        idx = u->arr_top + row * 16 + col;
        if (idx >= 0 && idx < d->ncolors) {
            if (u->arr_op == 1 || u->arr_op == 2 || u->arr_op == 3 || u->arr_op == 5) {
                arr_apply_well(u, d, idx);
            } else if (button == 3) {
                d->bg = idx;
                u->arr_sel = idx;
            } else {
                d->fg = idx;
                u->arr_sel = idx;
            }
        }
        return 1;
    }
    {
        int ax = wx + cell * 16 + 2;
        int top = arr_limit(d);
        if (lx >= ax && lx < ax + 12 && ly >= wy && ly < wy + cell) {
            if (u->arr_top > 0) {
                u->arr_top -= 16;
            }
            return 1;
        }
        if (lx >= ax && lx < ax + 12 && ly >= wy + cell && ly < wy + cell * 2) {
            if (u->arr_top < top) {
                u->arr_top += 16;
            }
            return 1;
        }
    }
    arr_btns(u, f, y, h, bx, bw, &by, &bh);
    for (i = 0; i < 8; i++) {
        if (lx >= bx[i] && lx < bx[i] + bw[i] && ly >= by && ly < by + bh) {
            if (i == 0) {
                u->arr_op = 1;
                u->arr_from = u->arr_sel;
                doc_status(d, "COPY");
            } else if (i == 1) {
                u->arr_op = 2;
                u->arr_from = u->arr_sel;
                doc_status(d, "EX");
            } else if (i == 2) {
                u->arr_op = 3;
                u->arr_from = u->arr_sel;
                doc_status(d, "SPREAD");
            } else if (i == 3) {
                u->arr_op = 4;
                doc_status(d, "PICK");
            } else if (i == 4) {
                u->arr_op = 5;
                u->arr_from = u->arr_sel;
                doc_status(d, "DELETE");
            } else if (i == 5) {
                arr_remember(u, d);
                memcpy(d->pal, u->arr_open, sizeof(d->pal));
                doc_status(d, "REVERT");
            } else if (i == 6 && u->arr_undo_ok) {
                DpColor tmp[DP_MAX_COLORS];
                memcpy(tmp, d->pal, sizeof(tmp));
                memcpy(d->pal, u->arr_undo, sizeof(d->pal));
                memcpy(u->arr_undo, tmp, sizeof(tmp));
                doc_status(d, "UNDO");
            } else if (i == 7) {
                u->arrange = 0;
                u->arr_op = 0;
                doc_status(d, "OK");
            }
            return 1;
        }
    }
    return 1;
}

static void draw_arrange(const Ui *u, const Doc *d, const DpAssets *a,
                         uint8_t *rgb, int w, int hgt)
{
    const DpFont *f;
    int s, x, y, rw, rh, cell, wx, wy, i, row, col;
    int bx[8], bw[8], by, bh;
    char num[16];
    DpColor fg, bg;
    if (!u->arrange) {
        return;
    }
    f = font(a, u);
    s = sc(u);
    arr_geom(u, &x, &y, &rw, &rh, &cell, &wx, &wy);
    fill_rect(rgb, w, hgt, x * s, y * s, rw * s, rh * s, 176, 176, 176);
    fill_rect(rgb, w, hgt, x * s, y * s, rw * s, s, 230, 230, 230);
    fill_rect(rgb, w, hgt, x * s, (y + rh - 1) * s, rw * s, s, 90, 90, 90);
    snprintf(num, sizeof(num), "%d", u->arr_top / 16);
    if (f != NULL) {
        dp_font_rgb(f, rgb, w, hgt, 4 * s, (y + 1) * s, num, 0, 0, 0, s);
    }
    fg = d->pal[u->arr_sel >= 0 && u->arr_sel < d->ncolors ? u->arr_sel : 0];
    bg = d->pal[d->bg >= 0 && d->bg < d->ncolors ? d->bg : 0];
    fill_rect(rgb, w, hgt, 16 * s, (y + 1) * s, 10 * s, 8 * s, bg.r, bg.g, bg.b);
    fill_rect(rgb, w, hgt, 4 * s, (wy) * s, 30 * s, (cell * 2) * s, fg.r, fg.g, fg.b);
    fill_rect(rgb, w, hgt, 4 * s, wy * s, 30 * s, s, 255, 255, 255);
    fill_rect(rgb, w, hgt, 4 * s, wy * s, s, cell * 2 * s, 255, 255, 255);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 16; col++) {
            int idx = u->arr_top + row * 16 + col;
            int cx = wx + col * cell;
            int cy = wy + row * cell;
            DpColor c;
            if (idx < 0 || idx >= d->ncolors) {
                fill_rect(rgb, w, hgt, cx * s, cy * s, cell * s, cell * s, 80, 80, 80);
                continue;
            }
            c = d->pal[idx];
            fill_rect(rgb, w, hgt, cx * s, cy * s, cell * s, cell * s, c.r, c.g, c.b);
            if (idx == u->arr_sel) {
                fill_rect(rgb, w, hgt, cx * s, cy * s, cell * s, s, 255, 255, 255);
                fill_rect(rgb, w, hgt, cx * s, cy * s, s, cell * s, 255, 255, 255);
            }
        }
    }
    {
        int ax = wx + cell * 16 + 2;
        fill_rect(rgb, w, hgt, ax * s, wy * s, 12 * s, cell * s, 200, 200, 200);
        fill_rect(rgb, w, hgt, ax * s, (wy + cell) * s, 12 * s, cell * s, 140, 140, 140);
        if (f != NULL) {
            dp_font_rgb(f, rgb, w, hgt, (ax + 2) * s, (wy + 1) * s, "^", 0, 0, 0, s);
            dp_font_rgb(f, rgb, w, hgt, (ax + 2) * s, (wy + cell + 1) * s, "v", 0, 0, 0, s);
        }
    }
    arr_btns(u, f, y, rh, bx, bw, &by, &bh);
    for (i = 0; i < 8; i++) {
        int hot = (i == 0 && u->arr_op == 1) || (i == 1 && u->arr_op == 2) ||
                  (i == 2 && u->arr_op == 3) || (i == 3 && u->arr_op == 4) ||
                  (i == 4 && u->arr_op == 5);
        fill_rect(rgb, w, hgt, bx[i] * s, by * s, bw[i] * s, bh * s,
                  hot ? 40 : 200, hot ? 40 : 200, hot ? 40 : 200);
        if (f != NULL) {
            dp_font_rgb(f, rgb, w, hgt, (bx[i] + 3) * s, (by + 2) * s, arr_lbl[i],
                        hot ? 255 : 0, hot ? 255 : 0, hot ? 255 : 0, s);
        }
    }
}

void ui_mouse(Ui *u, Doc *d, DpAssets *a, int mx, int my, int button, int down)
{
    sync_fonts(a);
    int s = sc(u);
    int lx = mx / s;
    int ly = my / s;
    int px, py, over;
    int color_button = button == 3 ? 1 : 0;
    int tool = -1, undo = 0, clearb = 0, special = 0;

    place_pointer(u, d, mx, my);

    if (u->arrange) {
        if (down) {
            arr_click(u, d, a, lx, ly, button);
        }
        return;
    }

    if (u->enh_open) {
        if (down && button == 1) {
            enh_click(u, d, lx, ly);
        }
        return;
    }

    if (u->prompt) {
        if (down) {
            int rx, ry, rw, rh, okx, oky, cx, cy, bw, bh;
            req_box(u, &rx, &ry, &rw, &rh, &okx, &oky, &cx, &cy, &bw, &bh);
            if (lx >= cx && lx < cx + bw && ly >= cy && ly < cy + bh) {
                u->prompt = 0;
                u->text_len = 0;
                u->text[0] = 0;
                doc_status(d, "CANCEL");
                return;
            }
            if (lx >= okx && lx < okx + bw && ly >= oky && ly < oky + bh) {
                accept_prompt(u, d);
                return;
            }
            file_click(u, d, lx, ly);
        }
        return;
    }
    if (u->about || u->printer) {
        if (down) {
            u->about = 0;
            u->printer = 0;
        }
        return;
    }

    if (!down) {
        u->panel_drag = 0;
    } else if (button == 1 && panel_hit(u, d, lx, ly)) {
        return;
    }

    if (!down) {
        if (button == 3 && u->menu_hold) {
            int act = 0, arg = 0;
            u->menu_hold = 0;
            if (menu_pick(u, a, lx, ly, &act, &arg)) {
                do_act(u, d, a, act, arg);
            } else {
                u->menu_open = -1;
                u->menu_item = -1;
                u->menu_sub = -1;
            }
            return;
        }
        if (u->gesture == GS_CURVE) {
            u->curve_x0 = u->ax;
            u->curve_y0 = u->ay;
            u->curve_x1 = u->lx;
            u->curve_y1 = u->ly;
            u->gesture = GS_BEND;
            return;
        }
        if (u->gesture == GS_PICK) {
            int dx = u->lx - u->ax;
            int dy = u->ly - u->ay;
            if (dx < 0) {
                dx = -dx;
            }
            if (dy < 0) {
                dy = -dy;
            }
            doc_preview_restore(d);
            if (dx < 3 && dy < 3) {
                if (d->nabrush > 0) {
                    doc_stamp_brush(d, &d->abrush[d->abrush_i], u->ax, u->ay, u->use_bg);
                    d->abrush_i = (d->abrush_i + 1) % d->nabrush;
                } else if (d->brush.pix != NULL) {
                    doc_stamp_brush(d, &d->brush, u->ax, u->ay, u->use_bg);
                } else {
                    doc_dab(d, u->ax, u->ay, u->use_bg ? d->bg : d->fg);
                }
            } else {
                doc_pickup(d, u->ax, u->ay, u->lx, u->ly);
            }
            u->gesture = GS_NONE;
            u->stroking = 0;
            return;
        }
        if (u->gesture == GS_BEND) {
            u->gesture = GS_NONE;
            u->stroking = 0;
            return;
        }
        if (u->gesture == GS_ZOOM) {
            int x0 = u->ax;
            int y0 = u->ay;
            int x1 = u->lx;
            int y1 = u->ly;
            u->gesture = GS_NONE;
            u->stroking = 0;
            if (x0 > x1) {
                int t = x0;
                x0 = x1;
                x1 = t;
            }
            if (y0 > y1) {
                int t = y0;
                y0 = y1;
                y1 = t;
            }
            if (x0 < 0) {
                x0 = 0;
            }
            if (y0 < 0) {
                y0 = 0;
            }
            if (x1 >= page_w(d)) {
                x1 = page_w(d) - 1;
            }
            if (y1 >= page_h(d)) {
                y1 = page_h(d) - 1;
            }
            if (x1 > x0 || y1 > y0) {
                u->mag_x0 = x0;
                u->mag_y0 = y0;
                u->mag_x1 = x1;
                u->mag_y1 = y1;
                u->magnify = 1;
                u->mag_arm = 0;
                doc_status(d, "Magnify");
            }
            return;
        }
        u->gesture = GS_NONE;
        u->stroking = 0;
        return;
    }

    if (button == 3 && ly < MENU_H) {
        int b = hit_bar(u, a, lx);
        u->menu_hold = 1;
        u->menu_open = b >= 0 ? b : 0;
        u->menu_item = -1;
        u->menu_sub = -1;
        u->stroking = 0;
        u->gesture = GS_NONE;
        return;
    }
    if (button == 1 && ly < MENU_H) {
        return;
    }
    if (button == 1 && down) {
        int bsize = brush_size_at(u, lx, ly);
        if (bsize > 0) {
            d->brush_d = bsize;
            doc_status(d, "Brush");
            return;
        }
    }
    {
        int idx = palette_index_at(u, d, lx, ly);
        if (idx >= 0 && (button == 1 || button == 3)) {
            if (color_button) {
                d->bg = idx;
            } else {
                d->fg = idx;
            }
            return;
        }
    }
    if (tool_cell_at(u, lx, ly, &tool, &undo, &clearb, &special)) {
        if (special == CELL_ZOOM) {
            if (button == 1) {
                mag_step(u, d, 1);
            } else if (button == 3) {
                mag_step(u, d, 0);
            }
            return;
        }
        if (button != 1) {
            return;
        }
        if (special == CELL_GRID) {
            d->grid_on = !d->grid_on;
            doc_status(d, d->grid_on ? "Grid" : "Grid off");
            return;
        }
        if (special == CELL_SYM) {
            d->sym_on = !d->sym_on;
            doc_status(d, d->sym_on ? "Symmetry" : "Symmetry off");
            return;
        }
        if (special == CELL_MAG) {
            if (u->magnify || u->mag_arm) {
                u->magnify = 0;
                u->mag_arm = 0;
                if (u->gesture == GS_ZOOM) {
                    u->gesture = GS_NONE;
                    u->stroking = 0;
                }
                doc_status(d, "Zoom off");
            } else {
                int pw = page_w(d);
                int ph = page_h(d);
                if (pw < 1) {
                    pw = 1;
                }
                if (ph < 1) {
                    ph = 1;
                }
                if (u->over_page) {
                    zoom_place(u, d, u->mpx, u->mpy);
                } else {
                    zoom_place(u, d, pw / 2, ph / 2);
                }
                u->mag_arm = 1;
                u->magnify = 0;
                doc_status(d, "Magnify");
            }
            return;
        }
        if (special == CELL_NA) {
            begin_prompt(u, d, -1, "Not in this build");
            return;
        }
        if (tool >= 0) {
            d->tool = tool;
            u->text_on = 0;
            doc_status(d, dp_tool_name(tool));
        } else if (undo) {
            doc_undo(d);
        } else if (clearb) {
            doc_checkpoint(d);
            doc_clear(d);
            doc_status(d, "Clear");
        }
        return;
    }
    page_pos(u, d, mx, my, &px, &py, &over);
    if (!over) {
        return;
    }
    if (px < 0 || py < 0 || px >= page_w(d) || py >= page_h(d)) {
        return;
    }
    u->use_bg = color_button;
    if (u->mag_arm && button == 1) {
        zoom_place(u, d, px, py);
        u->magnify = 1;
        u->mag_arm = 0;
        u->gesture = GS_NONE;
        u->stroking = 0;
        doc_status(d, "Magnify");
        return;
    }
    grid_snap(d, &px, &py);
    if (u->gesture == GS_BEND && d->tool == TOOL_CURVE) {
        doc_preview_restore(d);
        bezier(d, u->curve_x0, u->curve_y0, px, py, u->curve_x1, u->curve_y1,
               u->use_bg ? d->bg : d->fg);
        u->gesture = GS_NONE;
        return;
    }
    start_gesture(u, d, px, py);
}

void ui_motion(Ui *u, Doc *d, DpAssets *a, int mx, int my, int buttons)
{
    int px, py, over;
    int s = sc(u);
    int lx = mx / s;
    int ly = my / s;
    const DpFont *f;
    place_pointer(u, d, mx, my);
    page_pos(u, d, mx, my, &px, &py, &over);
    (void)over;
    if (u->menu_hold && ly < MENU_H) {
        int b = hit_bar(u, a, lx);
        if (b >= 0 && b != u->menu_open) {
            u->menu_open = b;
            u->menu_item = -1;
            u->menu_sub = -1;
        }
    }
    if (u->menu_hold && u->menu_open >= 0) {
        f = font(a, u);
        if (f != NULL && u->menu_open < nbars) {
            const MenuBar *mb = &bars[u->menu_open];
            int mx0, my0, mw, mhgt;
            menu_column(f, u->menu_open, log_w(u), &mx0, &my0, &mw, &mhgt);
            u->menu_sub = -1;
            if (lx >= mx0 && lx < mx0 + mw && ly >= my0 && ly < my0 + mhgt) {
                u->menu_item = (ly - my0) / ITEM_H;
                if (u->menu_item >= mb->n) {
                    u->menu_item = mb->n - 1;
                }
            }
            if (u->menu_item >= 0 && u->menu_item < mb->n &&
                mb->items[u->menu_item].nkids > 0) {
                int sx, sy, sw, sh;
                submenu_column(f, u->menu_open, u->menu_item, log_w(u),
                               &sx, &sy, &sw, &sh);
                if (lx >= sx && lx < sx + sw && ly >= sy && ly < sy + sh) {
                    u->menu_sub = (ly - sy) / ITEM_H;
                    if (u->menu_sub >= mb->items[u->menu_item].nkids) {
                        u->menu_sub = mb->items[u->menu_item].nkids - 1;
                    }
                }
            }
        }
        return;
    }
    if (u->prompt || u->menu_hold) {
        return;
    }
    if (u->panel_drag && (buttons & 1)) {
        int bar_y, bar_w, arr_l, arr_r, sl_x, sl_w, btn_x;
        panel_boxes(u, &bar_y, &bar_w, &arr_l, &arr_r, &sl_x, &sl_w, &btn_x);
        panel_slider(d, lx, sl_x, sl_w);
        return;
    }
    if (u->mag_arm && u->over_page) {
        zoom_place(u, d, px, py);
    }
    if (u->gesture == GS_ZOOM) {
        if (buttons && u->over_page) {
            u->lx = px;
            u->ly = py;
        }
        return;
    }
    if (buttons && u->stroking && u->over_page && u->gesture != GS_NONE &&
        u->gesture != GS_BEND) {
        grid_snap(d, &px, &py);
        paint_move(u, d, a, px, py);
        u->lx = px;
        u->ly = py;
    } else if (u->gesture == GS_BEND && u->over_page) {
        grid_snap(d, &px, &py);
        paint_move(u, d, a, px, py);
    }
}

void ui_key(Ui *u, Doc *d, DpAssets *a, int key, int ch)
{
    const DpFont *f;
    if ((key == KEY_CTRL_Z || key == KEY_CTRL_Y) &&
        (u->prompt || (u->text_on && d->tool == TOOL_TEXT))) {
        return;
    }
    if (u->prompt) {
        if (key == KEY_ESC) {
            u->prompt = 0;
            u->menu_hold = 0;
            u->text_len = 0;
            u->text[0] = 0;
            doc_status(d, "CANCEL");
            return;
        }
        if (key == KEY_ENTER) {
            accept_prompt(u, d);
            return;
        }
        if (file_act(u->prompt) && key == KEY_UP) {
            list_move(u, -1);
            return;
        }
        if (file_act(u->prompt) && key == KEY_DOWN) {
            list_move(u, 1);
            return;
        }
        if (key == KEY_BACK) {
            if (u->text_len > 0) {
                u->text_len--;
                u->text[u->text_len] = 0;
                u->list_sel = -1;
            }
            return;
        }
        if (ch >= 32 && ch < 127 && u->text_len + 1 < (int)sizeof(u->text)) {
            u->text[u->text_len++] = (char)ch;
            u->text[u->text_len] = 0;
            u->list_sel = -1;
        }
        return;
    }
    if (u->arrange && key != KEY_ESC) {
        return;
    }
    if (u->enh_open && key != KEY_ESC) {
        return;
    }
    if (key == KEY_ESC) {
        if (u->arrange) {
            memcpy(d->pal, u->arr_open, sizeof(d->pal));
            u->arrange = 0;
            u->arr_op = 0;
            doc_status(d, "Revert");
            return;
        }
        if (u->enh_open) {
            u->enh_open = 0;
            return;
        }
        if (u->text_on) {
            doc_preview_restore(d);
            u->text[0] = 0;
            u->text_len = 0;
        }
        u->menu_open = -1;
        u->menu_hold = 0;
        u->menu_item = -1;
        u->about = 0;
        u->printer = 0;
        u->text_on = 0;
        u->magnify = 0;
        u->mag_arm = 0;
        u->gesture = GS_NONE;
        u->stroking = 0;
        d->play = 0;
        return;
    }
    if (u->text_on && d->tool == TOOL_TEXT) {
        f = text_font(a, u);
        if (key == KEY_ENTER) {
            u->text_on = 0;
            return;
        }
        if (key == KEY_BACK && u->text_len > 0) {
            u->text_len--;
            u->text[u->text_len] = 0;
        } else if (ch >= 32 && ch < 127 && u->text_len + 1 < (int)sizeof(u->text)) {
            u->text[u->text_len++] = (char)ch;
            u->text[u->text_len] = 0;
        }
        doc_preview_restore(d);
        if (f != NULL) {
            dp_font_index(f, d->frames[d->cur].pix, page_w(d), page_h(d),
                          u->text_px, u->text_py - f->baseline, u->text,
                          (uint8_t)d->fg);
        }
        return;
    }
    if (key == KEY_CTRL_Z) {
        if (u->enh_undo) {
            doc_undo(d);
        }
        return;
    }
    if (key == KEY_CTRL_Y) {
        if (u->enh_undo) {
            doc_redo(d);
        }
        return;
    }
    if ((key & 0xF00) == 0xA00 && (key & 255) == 'a') {
        u->panel = !u->panel;
        doc_status(d, u->panel ? "Panel On" : "Panel Off");
        return;
    }
    if (ch == '3') {
        begin_prompt(u, d, A_GOTO, "number:");
        return;
    }
    if (key >= 0xF1 && key <= 0xF8) {
        static const int modes[8] = {
            MODE_MATTE, MODE_COLOR, MODE_REPLC, MODE_SMEAR,
            MODE_SHADE, MODE_BLEND, MODE_CYCLE, MODE_SMOOTH
        };
        d->mode = modes[key - 0xF1];
        doc_status(d, dp_mode_name(d->mode));
        return;
    }
    if (ch == 'u' || ch == 'U') {
        doc_undo(d);
        return;
    }
    if (ch == '.' || ch == '2') {
        int n = d->nframes;
        int i = d->cur + 1;
        if (n < 1) {
            return;
        }
        if (i >= n) {
            i = 0;
        }
        doc_goto(d, i);
        doc_status(d, "Next");
        return;
    }
    if (ch == ',' || ch == '1') {
        int n = d->nframes;
        int i = d->cur - 1;
        if (n < 1) {
            return;
        }
        if (i < 0) {
            i = n - 1;
        }
        doc_goto(d, i);
        doc_status(d, "Previous");
        return;
    }
    if (ch == '4' || ch == ' ') {
        if (d->play) {
            d->play = 0;
            doc_status(d, "Stop");
        } else {
            d->play = 1;
            d->play_dir = 1;
            d->play_acc = 0;
            doc_status(d, "Play");
        }
        return;
    }
    if (ch == '5') {
        d->play = 2;
        d->play_dir = 1;
        d->play_acc = 0;
        doc_status(d, "Play once");
        return;
    }
    if (ch == '6') {
        d->play = 3;
        d->play_dir = 1;
        d->play_acc = 0;
        doc_status(d, "Ping-pong");
        return;
    }
    if (ch == '!') {
        doc_goto(d, 0);
        doc_status(d, "First");
        return;
    }
    if (ch == '@') {
        doc_goto(d, d->nframes - 1);
        doc_status(d, "Last");
        return;
    }
    if (ch == '$') {
        d->play = 1;
        d->play_dir = -1;
        d->play_acc = 0;
        doc_status(d, "Play reverse");
        return;
    }
    if (ch == '%') {
        d->play = 2;
        d->play_dir = -1;
        d->play_acc = 0;
        doc_status(d, "Play once reverse");
        return;
    }
    if ((ch == '7' || ch == '8') && d->nabrush > 0) {
        if (ch == '7') {
            d->abrush_i--;
            if (d->abrush_i < 0) {
                d->abrush_i = d->nabrush - 1;
            }
        } else {
            d->abrush_i++;
            if (d->abrush_i >= d->nabrush) {
                d->abrush_i = 0;
            }
        }
        doc_status(d, "AnimBrush");
        return;
    }
    if (ch == '>') {
        mag_step(u, d, 1);
        return;
    }
    if (ch == '<') {
        mag_step(u, d, 0);
        return;
    }
    if (ch == '/') {
        d->sym_on = !d->sym_on;
        doc_status(d, d->sym_on ? "Symmetry" : "Symmetry off");
        return;
    }
    if (ch == 'j') {
        doc_swap_spare(d);
        return;
    }
    if (ch == 'b') {
        d->tool = TOOL_BRUSH;
        doc_status(d, "Brush");
        return;
    }
    if (ch == '+' || ch == '=') {
        doc_add_frames(d, 1, 0);
        return;
    }
    if (ch == '-') {
        doc_delete_frames(d, 1);
        return;
    }
    if (key == KEY_LEFT || key == KEY_RIGHT) {
        if (d->play) {
            if (key == KEY_LEFT && d->fps > 1) {
                d->fps--;
            }
            if (key == KEY_RIGHT && d->fps < 60) {
                d->fps++;
            }
            return;
        }
        if (key == KEY_LEFT && u->scroll_x > 0) {
            u->scroll_x -= 8;
        }
        if (key == KEY_RIGHT) {
            u->scroll_x += 8;
            if (u->scroll_x > page_w(d)) {
                u->scroll_x = page_w(d);
            }
        }
        return;
    }
    if (key == KEY_UP) {
        if (u->scroll_y > 0) {
            u->scroll_y -= 8;
        }
        return;
    }
    if (key == KEY_DOWN) {
        u->scroll_y += 8;
        if (u->scroll_y > page_h(d)) {
            u->scroll_y = page_h(d);
        }
        return;
    }
}

void ui_tick(Ui *u, Doc *d, uint32_t dt_ms)
{
    int a, b, n;
    if (u->hover_id != 0 && u->hover_ms < 4000) {
        u->hover_ms += (int)dt_ms;
    }
    if (d->nranges > 0 && d->ranges[0].active) {
        {
            static uint32_t cyc;
            cyc += dt_ms;
            if (cyc > 80) {
                doc_cycle_palette(d);
                cyc = 0;
            }
        }
    }
    if (!d->play || d->nframes < 2 || d->fps < 1) {
        return;
    }
    d->play_acc += dt_ms;
    while (d->play_acc >= (uint32_t)(1000 / d->fps)) {
        d->play_acc -= (uint32_t)(1000 / d->fps);
        a = d->range_a;
        b = d->range_b;
        if (a < 0) {
            a = 0;
        }
        if (b >= d->nframes) {
            b = d->nframes - 1;
        }
        if (b < a) {
            b = a;
        }
        if (d->play == 3) {
            n = d->cur + d->play_dir;
            if (n > b) {
                d->play_dir = -1;
                n = d->cur - 1;
            }
            if (n < a) {
                d->play_dir = 1;
                n = d->cur + 1;
            }
            if (n < a) {
                n = a;
            }
            if (n > b) {
                n = b;
            }
            doc_goto(d, n);
        } else {
            int dir = d->play_dir < 0 ? -1 : 1;
            n = d->cur + dir;
            if (dir < 0 && n < a) {
                if (d->play == 2) {
                    d->play = 0;
                    break;
                }
                n = b;
            } else if (dir > 0 && n > b) {
                if (d->play == 2) {
                    d->play = 0;
                    break;
                }
                n = a;
            }
            doc_goto(d, n);
        }
    }
}
