#include "common.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

static log_level_t current_log_level = LOG_LEVEL_INFO;

void set_log_level(log_level_t level) {
    current_log_level = level;
}

void log_msg(log_level_t level, const char *level_str, const char *fmt, ...) {
    if (level > current_log_level)
        return;

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s: ", level_str);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void assertion(bool r, const char *fcname , int line, const char *fname){
    if(!r){
        fprintf(stderr, "ERROR: my_assert FAIL: %s() line %d in %s\n", fcname, line, fname);
        exit(ERR_ASSERTION);
    }
}

void *safe_alloc(size_t size){  
    void *ret = malloc(size);
    if(!ret){
        error("Allocation error, exiting");
        exit(ERR_ALLOCATION);
    }
    return ret;
}
