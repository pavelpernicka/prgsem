#include "cli.h"
#include "common.h"
#include "prgsem_main.h"
#include "image_writer.h"
#include "ffmpeg_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

static volatile int interrupted = 0;

static void handle_sigint(int sig) {
    (void)sig;
    interrupted = 1;
    printf("\nInterrupted. Cleaning up...\n");
}

void render_image(uint8_t *image, int w, int h,
                         double c_re, double c_im,
                         double re_min, double re_max,
                         double im_min, double im_max,
                         uint8_t max_iter) {
    debug("Rendering image with c = %.4f + %.4fi, re:[%.4f,%.4f] im:[%.4f,%.4f] n=%d",
          c_re, c_im, re_min, re_max, im_min, im_max, max_iter);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double z_re = re_min + x * (re_max - re_min) / w;
            double z_im = im_max - y * (im_max - im_min) / h;
            uint8_t iter = compute_pixel(c_re, c_im, z_re, z_im, max_iter);
            double t = (double)iter / (max_iter + 1.0);
            int index = (y * w + x) * 3;
            image[index + 0] = 9 * (1 - t) * t * t * t * 255;
            image[index + 1] = 15 * (1 - t) * (1 - t) * t * t * 255;
            image[index + 2] = 8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255;
        }
    }
}

static void show_progress(int current, int total) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int terminal_width = w.ws_col > 0 ? w.ws_col : 80;
    int bar_width = terminal_width - 10;
    if (bar_width < 10) bar_width = 10;

    double ratio = (double)current / total;
    int filled = (int)(bar_width * ratio);

    printf("\r[");
    for (int i = 0; i < bar_width; ++i) {
        printf(i < filled ? "=" : " ");
    }
    printf("] %3d%%", (int)(ratio * 100));
    fflush(stdout);
}

bool save_image_auto(const char *path, uint8_t *image, int w, int h) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;

    if (strcmp(ext, ".png") == 0) {
        return save_image_png(path, image, w, h);
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return save_image_jpg(path, image, w, h);
    } else {
    	error("Unsupported image format (use .png or .jpg)");
    }
    return false;
}

int cli_main(app_state *state, struct arguments *args) {
    signal(SIGINT, handle_sigint);

    int w = args->w;
    int h = args->h;
    uint8_t *image = malloc(w * h * 3);
    if (!image) {
        error("Memory allocation failed");
        return EXIT_FAILURE;
    }

    double re_min = args->range_re_min;
    double re_max = args->range_re_max;
    double im_min = args->range_im_min;
    double im_max = args->range_im_max;

    debug("CLI tool started");

    if (args->output_path && args->anim_duration == 0) {
        debug("Rendering static image to %s", args->output_path);
        render_image(image, w, h, args->c_re, args->c_im, re_min, re_max, im_min, im_max, args->n);
        if (!save_image_auto(args->output_path, image, w, h)) {
            error("Failed to save output image");
            free(image);
            return EXIT_FAILURE;
        }
        info("Saved image to %s", args->output_path);
        free(image);
        return EXIT_SUCCESS;
    } else if (args->anim_duration > 0 && args->output_path) {
        debug("Rendering animation to %s", args->output_path);
        const char *ext = strrchr(args->output_path, '.');
        if (!ext || (strcmp(ext, ".mp4") != 0)) {
            error("Unsupported animation format (only .mp4 supported)");
            free(image);
            return EXIT_FAILURE;
        }

        ffmpeg_writer *video = ffmpeg_writer_create(args->output_path, w, h, args->anim_fps);
        if (!video) {
            error("Failed to initialize video writer");
            free(image);
            return EXIT_FAILURE;
        }

        int total_frames = args->anim_duration * args->anim_fps;
        double zoom_per_frame = pow(args->anim_zoom_factor, 1.0 / total_frames);
        debug("Total frames: %d, FPS: %d, Zoom/frame: %.6f", total_frames, args->anim_fps, zoom_per_frame);

        for (int i = 0; i < total_frames && !interrupted; ++i) {
            render_image(image, w, h, args->c_re, args->c_im, re_min, re_max, im_min, im_max, args->n);
            ffmpeg_writer_add_frame(video, image);

            double re_center = 0.5 * (re_min + re_max);
            double im_center = 0.5 * (im_min + im_max);
            double re_half_span = (re_max - re_min) * 0.5 * zoom_per_frame;
            double im_half_span = (im_max - im_min) * 0.5 * zoom_per_frame;
            re_min = re_center - re_half_span;
            re_max = re_center + re_half_span;
            im_min = im_center - im_half_span;
            im_max = im_center + im_half_span;

            show_progress(i + 1, total_frames);
        }

        printf("\n");
        ffmpeg_writer_close(video);
        info("Animation saved to %s", args->output_path);
    } else {
    	error("Not enough arguments given for image or video generation");
    }

    free(image);
    debug("CLI tool exiting");
    return interrupted ? EXIT_FAILURE : EXIT_SUCCESS;
}

