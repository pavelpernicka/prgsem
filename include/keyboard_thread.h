#ifndef __KEYBOARD_THREAD_H__
#define __KEYBOARD_THREAD_H__

#include "common.h"

void call_termios(int reset);
void keyboard_set_event_pusher(event_pusher_fn handler);
void *keyboard_thread(void *arg);

#endif
