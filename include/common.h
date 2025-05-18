#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "event_queue.h"

#define MESSAGE_BUFF_SIZE 256

typedef enum {
  EXIT_OK = 1,
  EXIT_ERROR = 0,
  ERR_FILE_OPEN = 101,
  ERR_ALLOCATION = 102,
  ERR_ASSERTION = 105
} exit_codes;

typedef enum {
  LOG_LEVEL_ERROR = 0,   // Only errors
  LOG_LEVEL_WARNING = 1, // Errors + warnings
  LOG_LEVEL_INFO = 2,    // Errors + warnings + info
  LOG_LEVEL_DEBUG = 3    // Errors + warnings + info + debug
} log_level_t;

typedef void (*event_pusher_fn)(event);

void assertion(bool r, const char *fcname, int line, const char *fname);
void *safe_alloc(size_t size);

void set_log_level(log_level_t level);
void log_msg(log_level_t level, const char *level_str, const char *fmt, ...);

#define error(...) log_msg(LOG_LEVEL_ERROR, "ERROR", __VA_ARGS__)
#define warning(...) log_msg(LOG_LEVEL_WARNING, "WARNING", __VA_ARGS__)
#define info(...) log_msg(LOG_LEVEL_INFO, "INFO", __VA_ARGS__)
#define debug(...) log_msg(LOG_LEVEL_DEBUG, "DEBUG", __VA_ARGS__)

#endif
