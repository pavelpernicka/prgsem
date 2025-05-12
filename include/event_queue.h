#ifndef __EVENT_QUEUE_H__
#define __EVENT_QUEUE_H__

#include "messages.h"
#include <stdbool.h>
#include <pthread.h>

typedef enum {
   EV_MODULE,
   EV_KEYBOARD,
   EV_NUM,
   EV_SDL
} event_source;

typedef enum {
   EV_COMPUTE,
   EV_RESET_CHUNK,
   EV_ABORT,
   EV_GET_VERSION,
   EV_THREAD_EXIT,
   EV_QUIT,
   EV_SERIAL,
   EV_SET_COMPUTE,
   EV_COMPUTE_CPU,
   EV_CLEAR_BUFFER,
   EV_REFRESH,
   EV_PIPE_IN_MESSAGE,
   EV_TYPE_NUM
} event_type;

typedef struct {
   int param;
} event_keyboard;

typedef struct {
   message *msg;
} event_serial;

typedef struct {
   event_source source;
   event_type type;
   union {
      int param;
      message *msg;
   } data;
} event;

void queue_init(void);
void queue_cleanup(void);

bool queue_hasdata(void);
bool queue_wait_for_data(double timeout_s);
event queue_pop(void);

void queue_push(event ev);

bool is_quit();

void set_quit();
#endif

