#ifndef CLI_H
#define CLI_H

#include "prgsem_main.h"

bool save_image_auto(const char *path, uint8_t *image, int w, int h);
void render_image(
    uint8_t *image, int w, int h, double c_re, double c_i, double re_min,
    double re_max, double im_min, double im_max, uint8_t max_iter
);
int cli_main(app_state *state, struct arguments *args);

#endif
