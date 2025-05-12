#ifndef __PIPE_THREAD_H__
#define __PIPE_THREAD_H__

#include "common.h"

void pipe_set_event_pusher(event_pusher_fn handler);
void pipe_set_input_pipe_fd(int fd);
void *pipe_thread(void *arg);

#endif
