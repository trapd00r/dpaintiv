#include "dpaint.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Cursor *blank_cur;
static SDL_Cursor *arrow_cur;
static Cursor x11_blank_cur;
static int x11_blank_ready;

static int wayland_session(void)
{
    const char *s = getenv("XDG_SESSION_TYPE");
    return s != NULL && strcmp(s, "wayland") == 0;
}

static void x11_pointer(SDL_Window *win, int cross)
{
    SDL_SysWMinfo info;
    if (wayland_session() || win == NULL) {
        return;
    }
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_X11) {
        return;
    }
    if (info.info.x11.display == NULL || info.info.x11.window == 0) {
        return;
    }
    if (!x11_blank_ready) {
        char bits = 0;
        Pixmap pm;
        XColor c;
        Cursor cur;
        pm = XCreateBitmapFromData(info.info.x11.display, info.info.x11.window,
                                   &bits, 1, 1);
        if (pm == 0) {
            return;
        }
        memset(&c, 0, sizeof(c));
        cur = XCreatePixmapCursor(info.info.x11.display, pm, pm, &c, &c, 0, 0);
        XFreePixmap(info.info.x11.display, pm);
        if (cur == 0) {
            return;
        }
        x11_blank_cur = cur;
        x11_blank_ready = 1;
    }
    if (cross) {
        XDefineCursor(info.info.x11.display, info.info.x11.window, x11_blank_cur);
    } else {
        XUndefineCursor(info.info.x11.display, info.info.x11.window);
    }
    XFlush(info.info.x11.display);
}

static void apply_pointer(SDL_Window *win, int cross)
{
    if (blank_cur == NULL) {
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, 16, 16, 32,
                                                           SDL_PIXELFORMAT_RGBA32);
        Uint8 data[8];
        Uint8 mask[8];
        if (surf != NULL) {
            SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, 0, 0, 0, 0));
            blank_cur = SDL_CreateColorCursor(surf, 0, 0);
            SDL_FreeSurface(surf);
        }
        if (blank_cur == NULL) {
            memset(data, 0, sizeof(data));
            memset(mask, 0, sizeof(mask));
            blank_cur = SDL_CreateCursor(data, mask, 8, 8, 0, 0);
        }
    }
    if (cross) {
        if (blank_cur != NULL) {
            SDL_SetCursor(blank_cur);
        }
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        if (arrow_cur != NULL) {
            SDL_SetCursor(arrow_cur);
        }
        SDL_ShowCursor(SDL_ENABLE);
    }
    x11_pointer(win, cross);
}

static int map_key(SDL_Keycode sym, int *ch)
{
    *ch = 0;
    if (sym == SDLK_ESCAPE) {
        return KEY_ESC;
    }
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
        return KEY_ENTER;
    }
    if (sym == SDLK_BACKSPACE) {
        return KEY_BACK;
    }
    if (sym == SDLK_LEFT) {
        return KEY_LEFT;
    }
    if (sym == SDLK_RIGHT) {
        return KEY_RIGHT;
    }
    if (sym == SDLK_UP) {
        return KEY_UP;
    }
    if (sym == SDLK_DOWN) {
        return KEY_DOWN;
    }
    if (sym == SDLK_SPACE) {
        *ch = ' ';
        return ' ';
    }
    if (sym >= 32 && sym < 127) {
        *ch = (int)sym;
        return (int)sym;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : NULL;
    DpAssets assets;
    Doc doc;
    Ui ui;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    uint8_t *rgb = NULL;
    int win_w = 1280, win_h = 800;
    int running = 1;
    uint32_t prev;
    int mh, tw, bh, vw, vh;

    if (dp_assets_load(&assets, root) != 0) {
        fprintf(stderr, "Could not load a DPaint font\n");
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        dp_assets_free(&assets);
        return 1;
    }
    win = SDL_CreateWindow("Deluxe Paint IV",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (win == NULL) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        SDL_Quit();
        dp_assets_free(&assets);
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (ren == NULL) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_GetWindowSize(win, &win_w, &win_h);
    ui_init(&ui);
    ui.win_w = win_w;
    ui.win_h = win_h;
    ui.scale = win_h >= 1100 ? 3 : (win_h >= 700 ? 2 : 1);
    ui_layout(&ui, &mh, &tw, &bh, &vw, &vh);
    if (doc_init(&doc, vw, vh, 64, assets.pal) != 0) {
        fprintf(stderr, "page alloc failed\n");
        SDL_Quit();
        dp_assets_free(&assets);
        return 1;
    }
    if (!assets.pal_ok) {
        doc_status(&doc, "Default palette fallback");
    }
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                            win_w, win_h);
    rgb = (uint8_t *)malloc((size_t)win_w * (size_t)win_h * 3u);
    if (tex == NULL || rgb == NULL) {
        fprintf(stderr, "framebuffer failed\n");
        doc_free(&doc);
        dp_assets_free(&assets);
        SDL_Quit();
        return 1;
    }
    arrow_cur = SDL_GetCursor();
    apply_pointer(win, 0);
    SDL_StartTextInput();
    prev = SDL_GetTicks();
    while (running && !ui.quit) {
        SDL_Event ev;
        uint32_t now = SDL_GetTicks();
        uint32_t dt = now - prev;
        prev = now;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_WINDOWEVENT &&
                       ev.window.event == SDL_WINDOWEVENT_ENTER) {
                apply_pointer(win, ui.cursor_cross);
            } else if (ev.type == SDL_WINDOWEVENT &&
                       ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int mh2, tw2, bh2, vw2, vh2;
                win_w = ev.window.data1;
                win_h = ev.window.data2;
                ui.win_w = win_w;
                ui.win_h = win_h;
                ui.scale = win_h >= 1100 ? 3 : (win_h >= 700 ? 2 : 1);
                ui_layout(&ui, &mh2, &tw2, &bh2, &vw2, &vh2);
                if (doc.frames[0].w != vw2 || doc.frames[0].h != vh2) {
                    doc_resize(&doc, vw2, vh2, doc.ncolors);
                }
                SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24,
                                        SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
                free(rgb);
                rgb = (uint8_t *)malloc((size_t)win_w * (size_t)win_h * 3u);
            } else if (ev.type == SDL_MOUSEBUTTONDOWN ||
                       ev.type == SDL_MOUSEBUTTONUP) {
                ui.mod_alt = (SDL_GetModState() & KMOD_ALT) ? 1 : 0;
                ui_mouse(&ui, &doc, &assets, ev.button.x, ev.button.y,
                         ev.button.button, ev.type == SDL_MOUSEBUTTONDOWN);
                apply_pointer(win, ui.cursor_cross);
            } else if (ev.type == SDL_MOUSEMOTION) {
                ui_motion(&ui, &doc, &assets, ev.motion.x, ev.motion.y,
                          ev.motion.state);
                apply_pointer(win, ui.cursor_cross);
            } else if (ev.type == SDL_MOUSEWHEEL) {
                int dy = ev.wheel.y;
                if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    dy = -dy;
                }
                ui_wheel(&ui, &doc, ev.wheel.mouseX, ev.wheel.mouseY, dy);
            } else if (ev.type == SDL_TEXTINPUT) {
                const char *s = ev.text.text;
                int typing = ui.prompt || (ui.text_on && doc.tool == TOOL_TEXT);
                int frame_key = s[0] == '+' || s[0] == '-' || s[0] == '=';
                if (s[0] >= 32 && s[0] < 127 && s[1] == 0 && (typing || !frame_key)) {
                    ui_key(&ui, &doc, &assets, s[0], s[0]);
                }
            } else if (ev.type == SDL_KEYDOWN) {
                int ch = 0;
                int k = map_key(ev.key.keysym.sym, &ch);
                SDL_Keymod mod = SDL_GetModState();
                if ((mod & KMOD_SHIFT) && ch >= 'a' && ch <= 'z') {
                    ch = ch - 'a' + 'A';
                }
                if (ev.key.keysym.sym >= SDLK_F1 && ev.key.keysym.sym <= SDLK_F8) {
                    ui_key(&ui, &doc, &assets,
                           0xF1 + (int)(ev.key.keysym.sym - SDLK_F1), 0);
                } else if (k == KEY_ESC || k == KEY_ENTER || k == KEY_BACK ||
                    k == KEY_LEFT || k == KEY_RIGHT || k == KEY_UP || k == KEY_DOWN) {
                    ui_key(&ui, &doc, &assets, k, 0);
                } else if (ch != 0 && (mod & KMOD_ALT)) {
                    ui_key(&ui, &doc, &assets, 0xA00 + (ch & 255), ch);
                } else if ((mod & KMOD_CTRL) && (ch == 'z' || ch == 'Z')) {
                    ui_key(&ui, &doc, &assets, KEY_CTRL_Z, 0);
                } else if ((mod & KMOD_CTRL) && (ch == 'y' || ch == 'Y')) {
                    ui_key(&ui, &doc, &assets, KEY_CTRL_Y, 0);
                } else if ((ch == '+' || ch == '-' || ch == '=') &&
                           !(ui.prompt || (ui.text_on && doc.tool == TOOL_TEXT))) {
                    ui_key(&ui, &doc, &assets, ch, ch);
                } else if (ch != 0 && (mod & KMOD_CTRL)) {
                    ui_key(&ui, &doc, &assets, ch, ch);
                }
            }
        }
        if (rgb != NULL && tex != NULL) {
            ui_tick(&ui, &doc, dt);
            ui_render(&ui, &doc, &assets, rgb, win_w, win_h);
            SDL_UpdateTexture(tex, NULL, rgb, win_w * 3);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        }
        SDL_Delay(16);
    }
    free(rgb);
    if (tex != NULL) {
        SDL_DestroyTexture(tex);
    }
    if (ren != NULL) {
        SDL_DestroyRenderer(ren);
    }
    if (x11_blank_ready) {
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_X11 &&
            info.info.x11.display != NULL) {
            XFreeCursor(info.info.x11.display, x11_blank_cur);
        }
    }
    SDL_DestroyWindow(win);
    if (blank_cur != NULL) {
        SDL_FreeCursor(blank_cur);
    }
    SDL_ShowCursor(SDL_ENABLE);
    SDL_Quit();
    doc_free(&doc);
    dp_assets_free(&assets);
    return 0;
}
