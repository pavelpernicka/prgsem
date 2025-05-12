#ifndef PRGSEM_MAIN_H
#define PRGSEM_MAIN_H

#include <stdint.h>
#include "event_queue.h"

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

typedef enum {
    CMD_NONE,
    CMD_QUIT,
    CMD_GET_VERSION,
    CMD_SET_COMPUTE,
    CMD_COMPUTE,
    CMD_ABORT
} cmd_type;

void xwin_redraw_image();
void update_and_redraw();
void set_image_size(int x, int y);
uint8_t compute_pixel(double c_re, double c_im, double z_re, double z_im, uint8_t max_iter);
void local_compute();
void send_command(cmd_type cmd);
void process_event(event *ev);
void render_pixel(uint8_t iter, uint8_t *rgb);

#endif
