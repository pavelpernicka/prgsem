#include "window_thread.h"
#include "event_queue.h"
#include "graphical_assets.h"

#include <assert.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

static SDL_Window *win = NULL;
static TTF_Font *font = NULL;
static event_pusher_fn event_push = NULL;
static pthread_mutex_t xwin_mutex = PTHREAD_MUTEX_INITIALIZER;
static char overlay_message[OVERLAY_MSG_MAXLEN] = "";

int xwin_init(int w, int h) {
    int r = SDL_Init(SDL_INIT_VIDEO);
    if (r != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_ERROR;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return EXIT_ERROR;
    }

    font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        // proceed without fonts
    }

    assert(win == NULL);
    win = SDL_CreateWindow("PRG Semester Project", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
    assert(win != NULL);

    SDL_SetWindowTitle(win, "PRG SEM");
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(icon_32x32_bits, 32, 32, 24, 32 * 3, 0xff, 0xff00, 0xff0000, 0x0000);
    SDL_SetWindowIcon(win, surface);
    SDL_FreeSurface(surface);
    return EXIT_OK;
}

int xwin_resize(int w, int h) {
    pthread_mutex_lock(&xwin_mutex);
    assert(win != NULL);
    SDL_SetWindowSize(win, w, h);
    pthread_mutex_unlock(&xwin_mutex);
    return EXIT_OK;
}

void xwin_close(void) {
    pthread_mutex_lock(&xwin_mutex);
    if (font) {
        TTF_CloseFont(font);
        font = NULL;
    }
    TTF_Quit();
    if (win) SDL_DestroyWindow(win);
    win = NULL;
    SDL_Quit();
    pthread_mutex_unlock(&xwin_mutex);
}

void xwin_redraw(int w, int h, unsigned char *img) {
    assert(img);
    pthread_mutex_lock(&xwin_mutex);
    if (!win) {
        pthread_mutex_unlock(&xwin_mutex);
        return;
    }

    SDL_Surface *scr = SDL_GetWindowSurface(win);
    if (!scr) {
        pthread_mutex_unlock(&xwin_mutex);
        return;
    }

    for (int y = 0; y < scr->h; ++y) {
        for (int x = 0; x < scr->w; ++x) {
            const int idx = (y * scr->w + x) * scr->format->BytesPerPixel;
            Uint8 *px = (Uint8 *)scr->pixels + idx;
            *(px + scr->format->Rshift / 8) = *(img++);
            *(px + scr->format->Gshift / 8) = *(img++);
            *(px + scr->format->Bshift / 8) = *(img++);
        }
    }

    xwin_draw_overlay_message(scr);

    SDL_UpdateWindowSurface(win);
    pthread_mutex_unlock(&xwin_mutex);
}

void xwin_set_event_pusher(event_pusher_fn handler) {
    event_push = handler;
}

int is_valid_sdl_key(SDL_Keycode key) {
    return (key >= 'a' && key <= 'z') ||
           (key >= 'A' && key <= 'Z') ||
           (key >= '0' && key <= '9') ||
           key == TAB ||
           (key >= F1 && key <= F4);
}

void render_pixel(uint8_t iter, uint8_t *rgb) {
    if (iter == 60) {
        rgb[0] = rgb[1] = rgb[2] = 0;
    } else {
        double t = (double)iter / 60.0;
        rgb[0] = (uint8_t)(9 * (1 - t) * t * t * t * 255);
        rgb[1] = (uint8_t)(15 * (1 - t) * (1 - t) * t * t * 255);
        rgb[2] = (uint8_t)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);
    }
}

bool show_helpscreen(int w, int h) {
    if (w < HELPSCREEN_W_MIN || h < HELPSCREEN_H_MIN || !font) {
        return EXIT_ERROR;
    }

    pthread_mutex_lock(&xwin_mutex);
    if (!win) {
        pthread_mutex_unlock(&xwin_mutex);
        return EXIT_ERROR;
    }

    SDL_Surface *surf = SDL_GetWindowSurface(win);
    if (!surf) {
        pthread_mutex_unlock(&xwin_mutex);
        return EXIT_ERROR;
    }

    SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, 255, 255, 255));

    const char *lines[] = {
        "HELP SCREEN",
        "",
        "1 - compute using module",
        "q - quit",
        "a - abort",
        "r - reset chunk id",
        "g - get version",
        "s - switching between resolutions",
        "l - clear buffer",
        "p - redraw window",
        "c - compute locally"
    };

    SDL_Color textColor = {0, 0, 0};
    int y = 30;
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); ++i) {
        SDL_Surface *text = TTF_RenderUTF8_Blended(font, lines[i], textColor);
        if (!text) continue;

        SDL_Rect dest = {
            .x = (w - text->w) / 2,
            .y = y,
            .w = text->w,
            .h = text->h
        };

        SDL_BlitSurface(text, NULL, surf, &dest);
        SDL_FreeSurface(text);
        y += FONT_SIZE + 6;
    }

    SDL_UpdateWindowSurface(win);
    pthread_mutex_unlock(&xwin_mutex);
    return EXIT_OK;
}

void xwin_draw_overlay_message(SDL_Surface *surf) {
    if (!overlay_message[0] || !font) return;

    SDL_Color textColor = {255, 255, 255};
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, overlay_message, textColor);
    if (!text) return;

    SDL_Rect dest = {
        .x = surf->w - text->w - 10,
        .y = 10,
        .w = text->w,
        .h = text->h
    };

    SDL_BlitSurface(text, NULL, surf, &dest);
    SDL_FreeSurface(text);
}

void xwin_set_overlay_message(const char *fmt, ...) {
    pthread_mutex_lock(&xwin_mutex);

    if (fmt && *fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(overlay_message, OVERLAY_MSG_MAXLEN, fmt, args);
        va_end(args);
    } else {
        overlay_message[0] = '\0';
    }

    pthread_mutex_unlock(&xwin_mutex);
}

void *window_thread(void *arg) {
    SDL_Event event_sdl;
    debug("window_thread - start");
    while (!is_quit()) {
        while (SDL_PollEvent(&event_sdl)) {
            switch (event_sdl.type) {
                case SDL_QUIT: {
                    event ev = { .source = EV_SDL, .type = EV_QUIT, .data.param = 'q' };
                    event_push(ev);
                    break;
                }
                case SDL_KEYDOWN: {
                    SDL_Keycode key = event_sdl.key.keysym.sym;
                    if (key == ESC) {
                        set_quit();
                        event ev = { .source = EV_SDL, .type = EV_QUIT, .data.param = 'q' };
                        event_push(ev);
                        break;
                    }
                    if (is_valid_sdl_key(key)) {
                        event ev = {
                            .source = EV_SDL,
                            .type = EV_KEYBOARD,
                            .data.param = (int)key
                        };
                        event_push(ev);
                    }
                    break;
                }
            }
        }
        usleep(WINDOW_LOOP_SLEEP_INTERVAL);
    }
    debug("window_thread - stop");
    return NULL;
}

