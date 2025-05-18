#include <stdbool.h>

#include "messages.h"

#ifndef __COMPUTATION_H__
#define __COMPUTATION_H__

#define CHUNK_SIZE_FACTOR 10 // Chunk size is width or height / this
#define APP_DOCSTRING "Fractal computation viewer"

typedef struct {
  double c_re; // Real part of complex constant c
  double c_im; // Imaginary part of complex constant c

  int n; // Maximum number of iterations per pixel

  double range_re_min; // Minimum value on the real axis of the complex plane
  double range_re_max; // Maximum value on the real axis
  double range_im_min; // Minimum value on the imaginary axis
  double range_im_max; // Maximum value on the imaginary axis

  int grid_w; // Width of the image in pixels
  int grid_h; // Height of the image in pixels

  int cur_x; // X position (in pixels) of the top-left corner of the current
             // chunk
  int cur_y; // Y position (in pixels) of the top-left corner of the current
             // chunk

  double d_re; // Step size in the real direction per pixel
  double d_im; // Step size in the imaginary direction per pixel

  int nbr_chunks; // Total number of chunks the full image is divided into
  int cid; // Index of the currently processed chunk (note: Faigl uses uint8 in
           // reference comp_module)
  double chunk_re; // Real coordinate of the current chunk's origin
  double chunk_im; // Imaginary coordinate of the current chunk's origin

  uint8_t chunk_n_re; // Width of one chunk in pixels on the real axis)
  uint8_t chunk_n_im; // Height of one chunk in pixels on the imaginary axis

  uint8_t *grid;

  bool computing, abort, done;
} comp_ctx;

comp_ctx *computation_create(void);
void ctx_update(comp_ctx *ctx);
void computation_destroy(comp_ctx *ctx);

bool is_computing(comp_ctx *ctx);
bool is_done(comp_ctx *ctx);
bool is_abort(comp_ctx *ctx);

void get_grid_size(comp_ctx *ctx, int *w, int *h);
void abort_comp(comp_ctx *ctx);
bool set_compute(comp_ctx *ctx, message *msg);
bool compute(comp_ctx *ctx, message *msg);
void update_image(comp_ctx *ctx, int w, int h, unsigned char *img);
void update_data(comp_ctx *ctx, const msg_compute_data *data);
void clear_grid(comp_ctx *ctx);
int get_current_cid(comp_ctx *ctx);
void reset_cid(comp_ctx *ctx);
uint8_t *get_internal_grid(comp_ctx *ctx);

#endif
