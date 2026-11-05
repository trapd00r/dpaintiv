#ifndef DPAINT_H
#define DPAINT_H

#include <stddef.h>
#include <stdint.h>

#define DP_MAX_COLORS 64
#define DP_MAX_FRAMES 48
#define DP_UNDO_MAX 32
#define DP_MAX_RANGES 8
#define DP_DPPS_LEN 110
#define DP_MAX_FONTS 8
#define DP_NICONS 29

enum {
    TOOL_DOT = 0,
    TOOL_DRAW,
    TOOL_LINE,
    TOOL_CURVE,
    TOOL_FILL,
    TOOL_RECT,
    TOOL_FRECT,
    TOOL_CIRCLE,
    TOOL_ELLIPSE,
    TOOL_AIR,
    TOOL_TEXT,
    TOOL_BRUSH,
    TOOL_COUNT
};

enum {
    MODE_MATTE = 0,
    MODE_COLOR,
    MODE_REPLC,
    MODE_SMEAR,
    MODE_SHADE,
    MODE_BLEND,
    MODE_CYCLE,
    MODE_SMOOTH,
    MODE_HBRITE,
    MODE_MIX,
    MODE_COUNT
};

enum {
    FMT_LO = 0,
    FMT_MED,
    FMT_LACE,
    FMT_HI
};

enum {
    PROC_TINT = 0,
    PROC_HUE,
    PROC_VALUE
};

typedef struct {
    uint8_t r, g, b;
} DpColor;

typedef struct {
    int w, h;
    uint8_t *pix;
} DpFrame;

typedef struct {
    int w, h;
    int hx, hy;
    uint8_t *pix;
    uint8_t trans;
    DpColor pal[DP_MAX_COLORS];
    int ncolors;
    int has_pal;
} DpBrush;

typedef struct {
    int rate;
    int active;
    int low, high;
} DpRange;

typedef struct {
    uint8_t *data;
    int len;
    int ysize;
    int baseline;
    int xsize;
    int lo, hi;
    int modulo;
    int data_off;
    int loc_off;
    int space_off;
    int kern_off;
    int kern_ok;
    char face[32];
} DpFont;

typedef struct {
    DpFont fonts[DP_MAX_FONTS];
    int nfonts;
    int font_i;
    uint8_t icon[DP_NICONS][11][16];
    int nicons;
    DpColor pal[DP_MAX_COLORS];
    int pal_ok;
    char root[512];
} DpAssets;

typedef struct {
    DpFrame frames[DP_MAX_FRAMES];
    int nframes;
    int cur;
    int range_a, range_b;
    int fps;
    int play; /* 0 stop, 1 loop, 2 once, 3 ping-pong */
    int play_dir;
    uint32_t play_acc;
    DpFrame spare;
    DpBrush brush;
    DpBrush brush_orig;
    int brush_orig_ok;
    DpBrush brush_spare;
    int brush_spare_ok;
    DpBrush abrush[16];
    int nabrush;
    int abrush_i;
    int fg, bg;
    int tool;
    int brush_d;
    int grid_on;
    int sym_on;
    int sym_pass;
    int mode;
    int ncolors;
    int camg;
    DpColor pal[DP_MAX_COLORS];
    DpColor pal_saved[DP_MAX_COLORS];
    DpRange ranges[DP_MAX_RANGES];
    int nranges;
    uint8_t *stencil;
    int stencil_on;
    int stencil_show;
    uint8_t *bg_fixed;
    int bg_fixed_on;
    int antialias;
    int translucency_on;
    int translucency_pct;
    int process_on;
    int process_mode;
    struct DpUndo {
        int frame;
        int w, h;
        uint8_t *pix;
    } ustack[DP_UNDO_MAX], rstack[DP_UNDO_MAX];
    int un, rn;
    int undo_hist;
    int handle_center;
    int pick_x, pick_y, pick_w, pick_h;
    int pick_ok;
    int method_compressed;
    uint8_t dpps[DP_DPPS_LEN];
    char status[180];
} Doc;

typedef struct {
    int scale;
    int win_w, win_h;
    int scroll_x, scroll_y;
    int menu_open;
    int menu_item;
    int menu_sub;
    int prompt;
    char text[256];
    int text_len;
    int about;
    int printer;
    int show_spare;
    int stroking;
    int gesture;
    int ax, ay, lx, ly;
    int curve_x0, curve_y0, curve_x1, curve_y1;
    int use_bg;
    int quit;
    int prefs;
    int text_px, text_py;
    int text_on;
    int font_i;
    int mpx, mpy;
    int over_page;
    int menu_hold;
    int pointer_x, pointer_y;
    int pointer_seen;
    int os_cursor_hidden;
    int magnify;
    int mag_arm;
    int mag_x, mag_y;
    int mag_x0, mag_y0, mag_x1, mag_y1;
    int cursor_cross;
    char dir[512];
    char lastdir[512];
    int list_scroll;
    int list_sel;
    int panel;
    int panel_drag;
    int lt_on;
    int lt_dim;
    int lt_prev;
    int lt_next;
    int lt_two;
    int lt_spare;
    int hover_id;
    int hover_ms;
    int enh_hover;
    int enh_undo;
    int enh_title;
    int enh_wheel;
    int enh_pick;
    int enh_open;
    int enh_edit_hover;
    int enh_edit_undo;
    int enh_edit_title;
    int enh_edit_wheel;
    int enh_edit_pick;
    int mod_alt;
    int ab_cels;
    int wheel_ms;
    int wheel_x;
    int wheel_y;
    int wheel_bg;
    int arrange;
    int mixer;
    int mix_hsv;
    int mix_drag;
    int arr_top;
    int arr_sel;
    int arr_op;
    int arr_from;
    int arr_undo_ok;
    DpColor arr_open[DP_MAX_COLORS];
    DpColor arr_undo[DP_MAX_COLORS];
    int range_open;
    int range_idx;
    int range_pick_hi;
    DpRange range_saved[DP_MAX_RANGES];
    int nranges_saved;
    int bend_horiz;
    int place_handle;
} Ui;

enum {
    PREF_COORDS = 1,
    PREF_FASTFB = 2,
    PREF_MULTICYCLE = 4,
    PREF_SQUARE = 8,
    PREF_WORKBENCH = 16,
    PREF_EXCL = 32,
    PREF_AUTOTRANSP = 64,
    PREF_NOICONS = 128,
    PREF_AUTOGRID = 256,
    PREF_ORIGINUL = 512,
    PREF_FASTADJUST = 1024
};

enum {
    GS_NONE = 0,
    GS_FREE,
    GS_LINE,
    GS_RECT,
    GS_CIRC,
    GS_ELLIP,
    GS_CURVE,
    GS_BEND,
    GS_PICK,
    GS_ZOOM
};

enum {
    KEY_LEFT = 256,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_BACK = 8,
    KEY_ENTER = 13,
    KEY_ESC = 27,
    KEY_CTRL_Z = 0xC1A,
    KEY_CTRL_Y = 0xC19
};

const char *dp_tool_name(int tool);
const char *dp_mode_name(int mode);
const char *const *dp_about_lines(int *n);

int dp_font_load(DpFont *f, const char *path);
int dp_font_adopt(DpFont *f, const uint8_t *bytes, int len, const char *face);
void dp_font_free(DpFont *f);
void dp_builtin_fill(DpAssets *a);
int dp_font_width(const DpFont *f, const char *s);
int dp_font_char_space(const DpFont *f, int ch);
int dp_font_glyphs(const DpFont *f, const char *s, int x,
                   int *xs, int *ws, int maxn);
void dp_font_index(const DpFont *f, uint8_t *dst, int w, int h,
                   int x, int y, const char *s, uint8_t color);
void dp_font_rgb(const DpFont *f, uint8_t *rgb, int w, int h,
                 int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b,
                 int scale);

int dp_assets_load(DpAssets *a, const char *root);
void dp_assets_free(DpAssets *a);
const DpFont *dp_font(const DpAssets *a);

int doc_init(Doc *d, int w, int h, int ncolors, const DpColor *pal);
void doc_free(Doc *d);
void doc_status(Doc *d, const char *s);
DpFrame *doc_frame(Doc *d);
int doc_resize(Doc *d, int w, int h, int ncolors);
void doc_checkpoint(Doc *d);
void doc_preview_restore(Doc *d);
void doc_undo(Doc *d);
void doc_redo(Doc *d);
void doc_set_undo_hist(Doc *d, int on);
void doc_pal_spread(Doc *d, int a, int b);
void doc_clear(Doc *d);
void doc_plot(Doc *d, int x, int y, int color);
void doc_dab(Doc *d, int x, int y, int color);
int doc_brush_covers(int size, int dx, int dy);
void doc_line(Doc *d, int x0, int y0, int x1, int y1, int color);
void doc_rect(Doc *d, int x0, int y0, int x1, int y1, int color, int fill);
void doc_ellipse(Doc *d, int cx, int cy, int rx, int ry, int color, int fill);
void doc_fill(Doc *d, int x, int y, int color);
void doc_air(Doc *d, int x, int y, int color);
void doc_smear(Doc *d, int x0, int y0, int x1, int y1);
void doc_stamp_brush(Doc *d, const DpBrush *b, int x, int y, int use_bg);
void doc_pickup(Doc *d, int x0, int y0, int x1, int y1);
int doc_add_frames(Doc *d, int n, int blank);
int doc_delete_frames(Doc *d, int n);
void doc_goto(Doc *d, int index);
void doc_swap_spare(Doc *d);
void doc_copy_spare(Doc *d);
void doc_merge(Doc *d, int front);
void doc_flip_page(Doc *d, int horiz);
void doc_set_format(Doc *d, int fmt);
void doc_cycle_palette(Doc *d);
int doc_adopt_frame(Doc *d, DpFrame *fr, const DpColor *pal, int ncolors,
                    int camg, const DpRange *rg, int nr);
int doc_adopt_anim(Doc *d, DpFrame *frames, int nframes, const DpColor *pal,
                   int ncolors, int camg, const DpRange *rg, int nr, int fps);
int doc_nearest(const Doc *d, int r, int g, int b);

void brush_free(DpBrush *b);
int brush_clone(DpBrush *dst, const DpBrush *src);
void brush_flip(DpBrush *b, int horiz);
void brush_rot90(DpBrush *b);
void brush_scale(DpBrush *b, int nw, int nh);
void brush_outline(DpBrush *b, int color);
void brush_trim(DpBrush *b);
void brush_shear(DpBrush *b, int sx, int sy);
void brush_rotate_angle(DpBrush *b, int deg);
void brush_bend(DpBrush *b, int horiz, int amount);
int brush_metamorph(Doc *d, int steps);

void doc_antialias_frame(Doc *d, int level);
void doc_fix_background(Doc *d);
void doc_free_background(Doc *d);
void doc_lock_fg(Doc *d);
void doc_remap_page(Doc *d);
void doc_recompute_palette(Doc *d);

int iff_write_png(const char *path, const uint8_t *pix, int w, int h,
                  const DpColor *pal, int ncolors);
int iff_save_ilbm(const char *path, const DpFrame *f,
                  const DpColor *pal, int ncolors, int camg,
                  const DpRange *ranges, int nranges,
                  int grab, int gx, int gy, int trans, int compression,
                  int slot);
int iff_load_ilbm(const char *path, DpFrame *f, DpColor *pal, int *ncolors,
                  int *camg, DpRange *ranges, int *nranges,
                  int *grab, int *gx, int *gy, int *trans, char *err, int errlen);

int iff_save_anim(const char *path, const DpFrame *frames, int nframes,
                  const DpColor *pal, int ncolors, int camg,
                  const DpRange *ranges, int nranges,
                  int fps, int compressed, int grab, int gx, int gy, int slot);
int iff_load_anim(const char *path, DpFrame *frames, int maxframes, int *nframes,
                  DpColor *pal, int *ncolors, int *camg,
                  DpRange *ranges, int *nranges, int *fps,
                  char *err, int errlen);

int iff_save_prsp(const char *path, const uint8_t dpps[DP_DPPS_LEN]);
int iff_load_prsp(const char *path, uint8_t dpps[DP_DPPS_LEN], char *err, int errlen);
int iff_save_stencil(const char *path, const uint8_t *mask, int w, int h);
int iff_load_stencil(const char *path, uint8_t **mask, int *w, int *h,
                     char *err, int errlen);

void ui_init(Ui *u);
void ui_cfg_path(char *dst, int n);
void ui_cfg_load(Ui *u, const char *path);
void ui_cfg_save(const Ui *u, const char *path);
void ui_mouse(Ui *u, Doc *d, DpAssets *a, int mx, int my, int button, int down);
void ui_wheel(Ui *u, Doc *d, int mx, int my, int dy, int shift);
void ui_motion(Ui *u, Doc *d, DpAssets *a, int mx, int my, int buttons);
void ui_key(Ui *u, Doc *d, DpAssets *a, int key, int ch);
void ui_tick(Ui *u, Doc *d, uint32_t dt_ms);
void ui_render(const Ui *u, const Doc *d, const DpAssets *a,
               uint8_t *rgb, int w, int h);
void ui_layout(const Ui *u, int *menu_h, int *tool_w, int *bot_h,
               int *view_w, int *view_h);
int ui_palette_box(const Ui *u, int ncolors, int *x, int *y, int *w, int *h);
int ui_panel_point(const Ui *u, int which, int *x, int *y);
int ui_prompt_pick(Ui *u, const char *name);
int ui_prompt_button(const Ui *u, int which, int *x, int *y);
int ui_menu_count(void);
const char *ui_menu_title(int index);
int ui_menu_title_x(const DpFont *f, int index);
int ui_menu_box(const Ui *u, const DpFont *f, int *x, int *y, int *w, int *h);
int ui_menu_subbox(const Ui *u, const DpFont *f, int *x, int *y, int *w, int *h);

int dp_write_ppm(const char *path, const uint8_t *rgb, int w, int h);

#endif
