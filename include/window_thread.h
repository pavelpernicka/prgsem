#ifndef __WINDOW_THREAD_H__
#define __WINDOW_THREAD_H__

#include "event_queue.h"
#include "common.h"

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
void xwin_poll_events(void);
void xwin_set_event_pusher(event_pusher_fn handler);
void *window_thread(void *arg);

#endif

