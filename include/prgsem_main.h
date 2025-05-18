#ifndef PRGSEM_MAIN_H
#define PRGSEM_MAIN_H

#include "computation.h"
#include "event_queue.h"
#include <stdint.h>

// Functionality configurators
// #define ENABLE_HANDSHAKE
// #define ENABLE_CLI

// Initial values
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

// Toggling between image sizes
#define WIDTH_A 640
#define HEIGHT_A 480
#define WIDTH_B 1280
#define HEIGHT_B 960

typedef struct {
  int fd_in;
  int fd_out;
  comp_ctx *ctx;
  uint8_t *image;
  bool computing_lock;
} app_state;

struct arguments {
  const char *pipe_in;
  const char *pipe_out;
  int w, h, n;
  double c_re, c_im;
  double range_re_min, range_re_max;
  double range_im_min, range_im_max;
  int log_level;
  bool cli_mode;
  char *output_path;
  int anim_fps;
  int anim_duration;
  double anim_zoom_factor;
};

bool module_handshake(app_state *state);
bool apply_args_to_ctx(struct arguments *args, comp_ctx *ctx);
void process_event(app_state *state, event *ev);
void send_command(app_state *state, message_type cmd);
void update_and_redraw(app_state *state);
void toggle_image_size(app_state *state);
void zoom_view(app_state *state, double factor);
void move_view(app_state *state, double dx, double dy);
void change_iterations(app_state *state, int delta);
void set_image_size(app_state *state, int w, int h);
void safe_show_helpscreen(app_state *state);
uint8_t compute_pixel(
    double c_re, double c_im, double z_re, double z_im, uint8_t max_iter
);
void local_compute(app_state *state);
void print_params(app_state *state);

#endif
