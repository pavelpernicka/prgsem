#include "computation.h"
#include "keyboard_thread.h"
#include "pipe_thread.h"
#include "prg_io_nonblock.h"
#include "prgsem_main.h"
#include "window_thread.h"

#ifdef ENABLE_CLI
#include "cli.h"
#endif

#include "version.h"

#include <argp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *argp_program_version = APP_VERSION;

static struct argp_option options[] = {
    {"pipe-in", 'i', "FILE", 0,
     "Input pipe path (default: /tmp/computational_module.out)"}, //path
    {"pipe-out", 'o', "FILE", 0,
     "Output pipe path (default: /tmp/computational_module.in)"}, //path
    {"width", 'w', "PX", 0, "Image width (default: 640)"}, // > 50, < 10000 must be divisable by CHUNK_SIZE_FACTOR
    {"height", 'h', "PX", 0, "Image height (default: 480)"}, // > 50, < 10000 must be divisable by CHUNK_SIZE_FACTOR
    {"c-re", 'r', "VAL", 0, "Real part of c (default: -0.4)"}, //up to size of int
    {"c-im", 'm', "VAL", 0, "Imaginary part of c (default: 0.6)"}, //up to size of int
    {"n", 'n', "INT", 0, "Max iterations (default: 60)"}, // > 0, < 200
    {"range-re", 1001, "MIN MAX", 0, "Real range (default: -1.6 1.6)"}, //up to size of int
    {"range-im", 1002, "MIN MAX", 0, "Imaginary range (default: -1.1 1.1)"}, //up to size of int
    {"log-level", 'v', "LEVEL", 0, // 0-3
     "Set log verbosity (0=error, 1=warn, 2=info, 3=debug)"},
#ifdef ENABLE_CLI
    {"cli", 1003, 0, 0,
     "Enable non-graphical CLI mode allowing animation creation"},
    {"output", 1004, "FILE", 0, "Output image file (.png or .jpg for images, .mp4 for videos)"},
    {"anim-fps", 1005, "N", 0, "Number of frames per second in video"}, // only if cli, >0, <120
    {"anim-duration", 1007, "N", 0, "Duration of animation in seconds"}, //only if cli, >0, <36000
    {"anim-zoom", 1006, "F", 0, "Zoom factor from start to the end of video sequence (e.g. 0.5)"}, //only if cli, > 0, <= 255
#endif
    {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *args = state->input;
    switch (key) {
    case 'i':
        args->pipe_in = arg;
        break;
    case 'o':
        args->pipe_out = arg;
        break;
    case 'w':
        args->w = atoi(arg);
        if (args->w < 50 || args->w > 10000 || args->w % CHUNK_SIZE_FACTOR != 0) {
            argp_error(state, "Invalid width (must be 50–10000 and divisible by %d)", CHUNK_SIZE_FACTOR);
        }
        break;
    case 'h':
        args->h = atoi(arg);
        if (args->h < 50 || args->h > 10000 || args->h % CHUNK_SIZE_FACTOR != 0) {
            argp_error(state, "Invalid height (must be 50–10000 and divisible by %d)", CHUNK_SIZE_FACTOR);
        }
        break;
    case 'r':
        args->c_re = atof(arg);
        break;
    case 'm':
        args->c_im = atof(arg);
        break;
    case 'n':
        args->n = atoi(arg);
        if (args->n <= 0 || args->n > 200) {
            argp_error(state, "Invalid iteration count (must be 1–200)");
        }
        break;
    case 'v':
        args->log_level = atoi(arg);
        if (args->log_level < 0 || args->log_level > 3) {
            argp_error(state, "Invalid log level (0–3 expected)");
        }
        break;
    case 1001:  // --range-re
        if (state->next + 1 >= state->argc) {
            argp_error(state, "--range-re requires two values (MIN MAX)");
        }
        args->range_re_min = atof(arg);
        args->range_re_max = atof(state->argv[state->next++]);
        break;
    case 1002:  // --range-im
        if (state->next + 1 >= state->argc) {
            argp_error(state, "--range-im requires two values (MIN MAX)");
        }
        args->range_im_min = atof(arg);
        args->range_im_max = atof(state->argv[state->next++]);
        break;
#ifdef ENABLE_CLI
    case 1003:
        args->cli_mode = true;
        break;
    case 1004:
        args->output_path = arg;
        break;
    case 1005:
        args->anim_fps = atoi(arg);
        if (args->anim_fps <= 0 || args->anim_fps > 120) {
            argp_error(state, "Invalid FPS (must be 1–120)");
        }
        break;
    case 1006:
        args->anim_zoom_factor = atof(arg);
        if (args->anim_zoom_factor <= 0.0 || args->anim_zoom_factor > 255.0) {
            argp_error(state, "Invalid zoom factor (must be > 0 and <= 255)");
        }
        break;
    case 1007:
        args->anim_duration = atoi(arg);
        if (args->anim_duration <= 0 || args->anim_duration > 36000) {
            argp_error(state, "Invalid animation duration (must be 1–36000 seconds)");
        }
        break;
#endif
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, NULL, APP_DOCSTRING};

bool apply_args_to_ctx(struct arguments *args, comp_ctx *ctx)
{
    if (args->w <= 0 || args->h <= 0 || args->n <= 0) {
        error("Invalid size or iteration count: w=%d, h=%d, n=%d", args->w,
              args->h, args->n);
        return EXIT_ERROR;
    }
    if (args->range_re_min >= args->range_re_max ||
        args->range_im_min >= args->range_im_max) {
        error("Invalid coordinate range: re_min=%.3f, re_max=%.3f, "
              "im_min=%.3f, im_max=%.3f",
              args->range_re_min, args->range_re_max, args->range_im_min,
              args->range_im_max);
        return EXIT_ERROR;
    }

    ctx->c_re = args->c_re;
    ctx->c_im = args->c_im;
    ctx->n = args->n;
    ctx->grid_h = args->h;
    ctx->grid_w = args->w;
    ctx->range_re_min = args->range_re_min;
    ctx->range_re_max = args->range_re_max;
    ctx->range_im_min = args->range_im_min;
    ctx->range_im_max = args->range_im_max;

    return EXIT_OK;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); // ignore pipe errors, handle them in code

    // default argument values
    struct arguments args = {.pipe_in = "/tmp/computational_module.out",
                             .pipe_out = "/tmp/computational_module.in",
                             .w = 640,
                             .h = 480,
                             .c_re = -0.4,
                             .c_im = 0.6,
                             .n = 60,
                             .range_re_min = -1.6,
                             .range_re_max = 1.6,
                             .range_im_min = -1.1,
                             .range_im_max = 1.1,
                             .log_level = LOG_LEVEL_INFO};

    app_state state = {.image = NULL,
                       .computing_lock = false,
                       .ctx = NULL,
                       .fd_in = -1,
                       .fd_out = -1};

    static pthread_t th_keyboard = 0, th_pipe = 0, th_sdl = 0;
    bool xwin_initialized = false;

    argp_parse(&argp, argc, argv, 0, 0, &args);
    set_log_level(args.log_level);

#ifdef ENABLE_CLI
	// CLI-only mode
    if (args.cli_mode) {
        int ret = cli_main(&state, &args);
        // handles cleaning independently
        return ret;
    }
#endif

    state.fd_in = io_open_read(args.pipe_in);
    info("Waiting for module to open pipe in reading mode...");
    state.fd_out = io_open_write(args.pipe_out);
    if (state.fd_out == -1 || state.fd_in == -1) {
        error("Cannot open pipes");
        goto cleanup;
    }

    debug("Pipe opened");
    queue_init();

    state.ctx = computation_create();
    if (!state.ctx) {
        error("Failed to initialize computation context");
        goto cleanup;
    }

    xwin_set_event_pusher(queue_push);
    keyboard_set_event_pusher(queue_push);
    pipe_set_event_pusher(queue_push);
    pipe_set_input_pipe_fd(state.fd_in);

    if (!apply_args_to_ctx(&args, state.ctx)) {
        error("Invalid arguments");
        set_quit();
        goto cleanup;
    }

    int x = state.ctx->grid_w;
    int y = state.ctx->grid_h;

    if (xwin_init(x, y) != EXIT_OK) {
        error("Failed to initialize SDL window");
        set_quit();
        goto cleanup;
    }
    xwin_initialized = true;

    set_image_size(&state, x, y);

    if (pthread_create(&th_pipe, NULL, pipe_thread, NULL) != 0) {
        error("Failed to start pipe thread");
        goto cleanup;
    }

    if (!module_handshake(&state)) {
        error("Handshake with compute module failed");
        set_quit();
        goto cleanup;
    }

    safe_show_helpscreen(&state);

    if (pthread_create(&th_keyboard, NULL, keyboard_thread, NULL) != 0 ||
        pthread_create(&th_sdl, NULL, window_thread, NULL) != 0) {
        error("Failed to start threads");
        set_quit();
        goto cleanup;
    }

    send_command(&state, MSG_SET_COMPUTE);

    while (!is_quit()) {
        event ev = queue_pop();
        process_event(&state, &ev);
    }

    send_command(&state, MSG_ABORT);

cleanup:
    if (th_keyboard)
        pthread_join(th_keyboard, NULL);
    if (th_sdl)
        pthread_join(th_sdl, NULL);
    if (th_pipe)
        pthread_join(th_pipe, NULL);

    if (xwin_initialized)
        xwin_close();
    free(state.image);
    computation_destroy(state.ctx);
    if (state.fd_in != -1)
        io_close(state.fd_in);
    if (state.fd_out != -1)
        io_close(state.fd_out);

    info("Gracefully exiting program");
    return is_quit() ? EXIT_FAILURE : EXIT_OK;
}

#ifdef ENABLE_HANDSHAKE
bool module_handshake(app_state *state)
{
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed = 0;

    while (!is_quit()) {
        send_command(state, MSG_GET_VERSION);
        debug("Handshake: sent command");

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed =
            (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;

        if (elapsed >= 10.0) {
            warning("Handshake timed out after 10 seconds");
            return EXIT_ERROR;
        }

        if (!queue_wait_for_data(0.5)) {
            continue;
        }

        event ev = queue_pop();
        if (ev.source == EV_PIPE && ev.data.msg->type == MSG_VERSION) {
            message *msg = ev.data.msg;
            msg_version local = VERSION;
            msg_version module = msg->data.version;

            if (module.major != local.major || module.minor != local.minor ||
                module.patch != local.patch) {
                warning(
                    "Module has different version: %d.%d.%d, errors may occur",
                    module.major, module.minor, module.patch);
            } else {
                info("Handshake successful, module version matching: %d.%d.%d",
                     module.major, module.minor, module.patch);
            }
            free(msg);
            return EXIT_OK;
        } else if (ev.source == EV_PIPE && ev.data.msg->type == MSG_STARTUP) {
            message *msg = ev.data.msg;
            msg_startup startup = msg->data.startup;
            info("Startup message caught: %s", startup.message);
            free(msg);
            return EXIT_OK;
        } else {
            debug("Other data caught during handshake");
        }
    }

    return EXIT_ERROR;
}
#else
bool module_handshake(app_state *state) { return EXIT_OK; }
#endif // ENABLE_HANDSHAKE

void toggle_image_size(app_state *state)
{
    int w = state->ctx->grid_w;
    int h = state->ctx->grid_h;

    if (w == WIDTH_A && h == HEIGHT_A) {
        set_image_size(state, WIDTH_B, HEIGHT_B);
    } else {
        set_image_size(state, WIDTH_A, HEIGHT_A);
    }
    safe_show_helpscreen(state);
}

void process_event(app_state *state, event *ev)
{
    if (ev->type == EV_QUIT) {
        set_quit();
    } else if (ev->source == EV_KEYBOARD || ev->source == EV_SDL) {
        switch (ev->data.param) {
        case 'q':
            set_quit();
            break;
        case 'g':
            info("Get version requested");
            send_command(state, MSG_GET_VERSION);
            break;
        case 's':
            if (state->computing_lock) {
                warning("New computation parameters requested but it is "
                        "discarded due to ongoing computation");
                xwin_set_overlay_message("Cannot set parameters - computing!");
                update_and_redraw(state);
            } else {
                info("Set new computation parameters");
                xwin_set_overlay_message("Set new params");
                toggle_image_size(state);
                send_command(state, MSG_SET_COMPUTE);
            }
            break;
        case '1':
            if (state->computing_lock) {
                warning("Computation already in progress");
                xwin_set_overlay_message("Still computing!");
            } else {
                state->computing_lock = true;
                info("Starting full image computation");
                xwin_set_overlay_message("Computation started");
                send_command(state, MSG_COMPUTE);
            }
            break;
        case 'a':
            if (!state->computing_lock) {
                warning("Abort requested but it is not computing");
            } else {
                info("Abort requested");
                xwin_set_overlay_message("Aborted");
                update_and_redraw(state);
                send_command(state, MSG_ABORT);
                abort_comp(state->ctx);
                state->computing_lock = false;
            }
            break;
        case 'r':
            if (state->computing_lock) {
                warning(
                    "Chunk reset request discarded, it is currently computing");
                xwin_set_overlay_message(
                    "Still computing, chnk reset discarded.");
            } else {
                reset_cid(state->ctx);
                info("Chunk reset request");
                xwin_set_overlay_message("CID reseted.");
                update_and_redraw(state);
            }
            break;
        case 'l': {
            int w, h;
            get_grid_size(state->ctx, &w, &h);
            memset(state->image, 0, w * h * 3);
            clear_grid(state->ctx);
            xwin_set_overlay_message("Cleared");
            info("Display buffer cleared");
            update_and_redraw(state);
            break;
        }
        case 'p':
            xwin_set_overlay_message("Redrawed");
            update_and_redraw(state);
            info("Image refreshed");
            break;
        case 'c':
            local_compute(state);
            update_and_redraw(state);
            break;
        case 'h': {
            if (state->computing_lock) {
                return;
            }
            safe_show_helpscreen(state);
            break;
        }
        case 'i':
            // zoom in
            zoom_view(state, 0.5);
            break;
        case 'o':
            // zoom out
            zoom_view(state, 2);
            break;
        case FRONT:
            move_view(state, 0.0, 0.2);
            break;
        case BACK:
            move_view(state, 0.0, -0.2);
            break;
        case LEFT:
            move_view(state, -0.2, 0.0);
            break;
        case RIGHT:
            move_view(state, 0.2, 0.0);
            break;
        case 'j':
            change_iterations(state, +10);
            break;
        case 'k':
            change_iterations(state, -10);
            break;
        case 'b':
        	print_params(state);
        	break;
        default:
            warning("Unknown keyboard event received: %c", ev->data.param);
        }
    } else if (ev->source == EV_PIPE) {
        message *msg = ev->data.msg;
        switch (msg->type) {
        case MSG_OK:
            debug("Acked!");
            break;
        case MSG_VERSION:
            info("Module version: %d.%d.%d", msg->data.version.major,
                 msg->data.version.minor, msg->data.version.patch);
            xwin_set_overlay_message(
                "Got version: %d.%d.%d", msg->data.version.major,
                msg->data.version.minor, msg->data.version.patch);
            update_and_redraw(state);
            break;
        case MSG_STARTUP: {
            msg_startup startup = msg->data.startup;
            info("Startup message received: %s", startup.message);
            send_command(state, MSG_SET_COMPUTE);
            break;
        }
        case MSG_COMPUTE_DATA: {
            if (state->computing_lock) {
                debug("Received new computed data from module");
                update_data(state->ctx, &msg->data.compute_data);
            } else {
                debug("Received computed data from module, but not computing");
            }
            break;
        }
        case MSG_DONE:
            if (!state->computing_lock) {
                warning("MSG_DONE received, but not computing");
                send_command(state, MSG_ABORT);
                break;
            }
            debug("Module reports done computing chunk");
            update_and_redraw(state);
            if (is_done(state->ctx)) {
                info("Computation ended");
                xwin_set_overlay_message("Computation ended.");
                update_and_redraw(state);
                state->computing_lock = false;
            } else {
                debug("Not done yet, computing next chunk");
                send_command(state, MSG_COMPUTE);
            }
            break;
        case MSG_ABORT:
            warning("Abort from Module, stopping computing");
            xwin_set_overlay_message("Abort from module, stopping.");
            update_and_redraw(state);
            state->computing_lock = false;
            break;
        case MSG_ERROR:
            warning("Module reports error");
            xwin_set_overlay_message("Error from module.");
            update_and_redraw(state);
            break;
        default:
            warning("Unknown message type has been received 0x%x", msg->type);
            break;
        }
        free(msg);
    }
}

void send_command(app_state *state, message_type cmd)
{
    message msg;
    memset(&msg, 0, sizeof(msg));

    bool valid = false;
    switch (cmd) {
    case MSG_GET_VERSION:
        msg.type = MSG_GET_VERSION;
        valid = true;
        break;
    case MSG_ABORT:
        msg.type = MSG_ABORT;
        valid = true;
        break;
    case MSG_SET_COMPUTE:
        valid = set_compute(state->ctx, &msg);
        break;
    case MSG_COMPUTE:
        valid = compute(state->ctx, &msg);
        break;
    default:
        return;
    }

    if (!valid) {
        error("Invalid message given to send");
        return;
    }

    uint8_t buf[MESSAGE_BUFF_SIZE];
    int len = 0;
    if (fill_message_buf(&msg, buf, sizeof(buf), &len)) {
        if (write(state->fd_out, buf, len) != len) {
            if (errno == EPIPE) {
                error("Pipe closed: cannot send to computation module (EPIPE)");
                set_quit();
            } else {
                error("send_message() does not send all bytes of the message!");
            }
        }
    } else {
        error("Cannot fill message buffer");
    }
}

void print_params(app_state *state) {
    comp_ctx *ctx = state->ctx;

    printf("\n[Parameters - Overview]\n");
    printf("Pipe in path:       %s\n", state->fd_in != -1 ? "(opened)" : "(not opened)");
    printf("Pipe out path:      %s\n", state->fd_out != -1 ? "(opened)" : "(not opened)");
    printf("Image size:         %d x %d\n", ctx->grid_w, ctx->grid_h);
    printf("Constant c:         %.6f + %.6fi\n", ctx->c_re, ctx->c_im);
    printf("Max iterations:     %d\n", ctx->n);
    printf("Real range:         %.6f to %.6f\n", ctx->range_re_min, ctx->range_re_max);
    printf("Imaginary range:    %.6f to %.6f\n", ctx->range_im_min, ctx->range_im_max);

    printf("\n[Parameters - CLI arguments]\n");
    printf("--width %d --height %d ", ctx->grid_w, ctx->grid_h);
    printf("--c-re %.6f --c-im %.6f ", ctx->c_re, ctx->c_im);
    printf("--n %d ", ctx->n);
    printf("--range-re %.6f %.6f ", ctx->range_re_min, ctx->range_re_max);
    printf("--range-im %.6f %.6f\n\n", ctx->range_im_min, ctx->range_im_max);
}

void zoom_view(app_state *state, double factor)
{
    if (state->computing_lock) {
        warning("Zoom discarded - computation in progress");
        xwin_set_overlay_message("Not zooming - computing!");
        update_and_redraw(state);
        return;
    }

    double re_center =
        0.5 * (state->ctx->range_re_min + state->ctx->range_re_max);
    double im_center =
        0.5 * (state->ctx->range_im_min + state->ctx->range_im_max);
    double re_span =
        (state->ctx->range_re_max - state->ctx->range_re_min) * factor * 0.5;
    double im_span =
        (state->ctx->range_im_max - state->ctx->range_im_min) * factor * 0.5;

    state->ctx->range_re_min = re_center - re_span;
    state->ctx->range_re_max = re_center + re_span;
    state->ctx->range_im_min = im_center - im_span;
    state->ctx->range_im_max = im_center + im_span;
    clear_grid(state->ctx);
    ctx_update(state->ctx);
    send_command(state, MSG_SET_COMPUTE);
    local_compute(state);
    xwin_set_overlay_message(factor < 1.0 ? "Set zoom in" : "Set zoom out");
    update_and_redraw(state);
}

void move_view(app_state *state, double dx, double dy)
{
    if (state->computing_lock) {
        warning("Not moving - computing");
        xwin_set_overlay_message("Not moving - computing");
        update_and_redraw(state);
        return;
    }

    double re_span = state->ctx->range_re_max - state->ctx->range_re_min;
    double im_span = state->ctx->range_im_max - state->ctx->range_im_min;

    state->ctx->range_re_min += dx * re_span;
    state->ctx->range_re_max += dx * re_span;
    state->ctx->range_im_min += dy * im_span;
    state->ctx->range_im_max += dy * im_span;

    clear_grid(state->ctx);
    ctx_update(state->ctx);
    send_command(state, MSG_SET_COMPUTE);
    local_compute(state);
    xwin_set_overlay_message("Moved view");
    update_and_redraw(state);
}

void change_iterations(app_state *state, int delta)
{
    if (state->computing_lock) {
        warning("Not changing iterations - computing");
        xwin_set_overlay_message("Not changing n - computing");
        update_and_redraw(state);
        return;
    }

    int new_n = state->ctx->n + delta;
    if (new_n < 1 || new_n > 255) {
        warning("Iteration count out of bounds: %d", new_n);
        xwin_set_overlay_message("N out of bounds");
        update_and_redraw(state);
        return;
    }

    state->ctx->n = new_n;
    clear_grid(state->ctx);
    local_compute(state);
    xwin_set_overlay_message("New n=%d", new_n);
    update_and_redraw(state);
    ctx_update(state->ctx);
    send_command(state, MSG_SET_COMPUTE);
}

void update_and_redraw(app_state *state)
{
    int w, h;
    get_grid_size(state->ctx, &w, &h);
    update_image(state->ctx, w, h, state->image);
    xwin_redraw(w, h, state->image);
}

void set_image_size(app_state *state, int w, int h)
{
    state->ctx->grid_w = w;
    state->ctx->grid_h = h;
    state->ctx->chunk_n_re = w / CHUNK_SIZE_FACTOR;
    state->ctx->chunk_n_im = h / CHUNK_SIZE_FACTOR;
    ctx_update(state->ctx);

    uint8_t *new_image = realloc(state->image, w * h * 3);
    if (!new_image) {
        error("Failed to reallocate image buffer");
        set_quit();
        return;
    }
    state->image = new_image;

    xwin_resize(w, h);
    memset(state->image, 0, w * h * 3);
    clear_grid(state->ctx);
    update_and_redraw(state);
}

void safe_show_helpscreen(app_state *state)
{
    int w, h;
    get_grid_size(state->ctx, &w, &h);
    if (show_helpscreen(w, h)) {
        info("Help screen showed");
    } else {
        error("Error showing help scren, window too small");
    }
}

uint8_t compute_pixel(double c_re, double c_im, double z_re, double z_im,
                      uint8_t max_iter)
{
    uint8_t k = 0;
    while (k < max_iter && z_re * z_re + z_im * z_im < 4.0) {
        double tmp = z_re * z_re - z_im * z_im + c_re;
        z_im = 2 * z_re * z_im + c_im;
        z_re = tmp;
        k++;
    }
    return k;
}

void local_compute(app_state *state)
{
    info("Local computation on PC started");
    xwin_set_overlay_message("Locally computed");
    int w, h;
    get_grid_size(state->ctx, &w, &h);
    uint8_t *grid = get_internal_grid(state->ctx);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double z_re =
                state->ctx->range_re_min +
                x * ((state->ctx->range_re_max - state->ctx->range_re_min) / w);
            double z_im =
                state->ctx->range_im_max +
                y * ((state->ctx->range_im_min - state->ctx->range_im_max) / h);
            grid[y * w + x] = compute_pixel(state->ctx->c_re, state->ctx->c_im,
                                            z_re, z_im, state->ctx->n);
        }
    }

    info("Local computation done");
}

