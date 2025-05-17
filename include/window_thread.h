#ifndef __WINDOW_THREAD_H__
#define __WINDOW_THREAD_H__

#include <SDL.h>
#include "event_queue.h"
#include "common.h"

#define WINDOW_LOOP_SLEEP_INTERVAL 10
#define HELPSCREEN_W_MIN 200
#define HELPSCREEN_H_MIN 100
#define OVERLAY_MSG_MAXLEN 128

#define FONT_PATH "assets/Purisa.ttf"
#define FONT_SIZE 20

typedef enum {
	ESC = 27,
	TAB = 9,
	F1 = 1073741882,
	F2,
	F3,
	F4
} window_key_codes;

int xwin_init(int w, int h);
int xwin_resize(int w, int h);
void xwin_close(void);
void xwin_redraw(int w, int h, unsigned char *img);
void render_pixel(uint8_t iter, uint8_t *rgb);
void xwin_poll_events(void);
void xwin_set_event_pusher(event_pusher_fn handler);
void *window_thread(void *arg);

bool show_helpscreen(int w, int h);
void xwin_draw_overlay_message(SDL_Surface *surf);
void xwin_set_overlay_message(const char *fmt, ...);

#endif

