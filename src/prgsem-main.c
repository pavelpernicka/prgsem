#include "prgsem_main.h"
#include "prg_io_nonblock.h"
#include "computation.h"
#include "window_thread.h"
#include "keyboard_thread.h"
#include "pipe_thread.h"
#include "version.h" // Will be autofilled in Makefile

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>
#include <time.h>
#include <signal.h>

static pthread_t th_keyboard, th_pipe, th_sdl;
static uint8_t *image = NULL;
static bool computing_lock = false;
static comp_ctx *ctx = NULL;

static int fd_in = -1;
static int fd_out = -1;

const char *argp_program_version = APP_VERSION;

static struct argp_option options[] = {
    {"pipe-in",  'i', "FILE", 0, "Input pipe path (default: /tmp/computational_module.out)"},
    {"pipe-out", 'o', "FILE", 0, "Output pipe path (default: /tmp/computational_module.in)"},
    {"width",    'w', "PX",   0, "Image width (default: 640)"},
    {"height",   'h', "PX",   0, "Image height (default: 480)"},
    {"c-re",     'r', "VAL",  0, "Real part of c (default: -0.4)"},
    {"c-im",     'm', "VAL",  0, "Imaginary part of c (default: 0.6)"},
    {"n",        'n', "INT",  0, "Max iterations (default: 60)"},
    {"range-re", 1001, "MIN MAX", 0, "Real range (default: -1.6 1.6)"},
    {"range-im", 1002, "MIN MAX", 0, "Imaginary range (default: -1.1 1.1)"},
    {"log-level", 'v', "LEVEL", 0, "Set log verbosity (0=error, 1=warn, 2=info, 3=debug)"},
    {0}
};

struct arguments {
    const char *pipe_in;
    const char *pipe_out;
    int w, h, n;
    double c_re, c_im;
    double range_re_min, range_re_max;
    double range_im_min, range_im_max;
    int log_level;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
        case 'i': args->pipe_in = arg; break;
        case 'o': args->pipe_out = arg; break;
        case 'w': args->w = atoi(arg); break;
        case 'h': args->h = atoi(arg); break;
        case 'r': args->c_re = atof(arg); break;
        case 'm': args->c_im = atof(arg); break;
        case 'n': args->n = atoi(arg); break;
        case 'v':
    		args->log_level = atoi(arg);
    		if (args->log_level < 0 || args->log_level > 3) {
        		argp_usage(state);
    		}
    break;
        case 1001:
            if (state->arg_num + 1 >= state->argc) return ARGP_ERR_UNKNOWN;
            args->range_re_min = atof(arg);
            args->range_re_max = atof(state->argv[++state->next]);
            break;
        case 1002:
            if (state->arg_num + 1 >= state->argc) return ARGP_ERR_UNKNOWN;
            args->range_im_min = atof(arg);
            args->range_im_max = atof(state->argv[++state->next]);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, NULL, APP_DOCSTRING};

void apply_args_to_ctx(struct arguments *args, comp_ctx *ctx) {
    if (args->w <= 0 || args->h <= 0 || args->n <= 0) {
        error("Invalid size or iteration count");
        exit(EXIT_FAILURE);
    }
    if (args->range_re_min >= args->range_re_max || args->range_im_min >= args->range_im_max) {
        error("Invalid coordinate range: min must be less than max");
        exit(EXIT_FAILURE);
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
}

int main(int argc, char *argv[]) {
	signal(SIGPIPE, SIG_IGN); // ignore pipe errors, to gracefully handle them in code
    struct arguments args = {
        .pipe_in = "/tmp/computational_module.out",
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
        .log_level = LOG_LEVEL_INFO
    };

    argp_parse(&argp, argc, argv, 0, 0, &args);
    set_log_level(args.log_level);

	fd_in = io_open_read(args.pipe_in);
	debug("Waiting for module to open pipe in reading mode...");
    fd_out = io_open_write(args.pipe_out);
    if (fd_out == -1 || fd_in == -1) {
        error("Cannot open pipes");
        return ERR_FILE_OPEN;
    }
    debug("Pipe opened");

    queue_init();
    ctx = computation_create();
    if (!ctx) {
        error("Failed to initialize computation context");
        return EXIT_FAILURE;
    }

    xwin_set_event_pusher(queue_push);
    keyboard_set_event_pusher(queue_push);
    pipe_set_event_pusher(queue_push);
    pipe_set_input_pipe_fd(fd_in);

	pthread_create(&th_pipe, NULL, pipe_thread, NULL);
	bool handshake_ok = module_handshake();
	if(handshake_ok){
    	pthread_create(&th_keyboard, NULL, keyboard_thread, NULL);
    	pthread_create(&th_sdl, NULL, window_thread, NULL);
    
    	apply_args_to_ctx(&args, ctx);
		xwin_init(ctx->grid_w, ctx->grid_h);
		set_image_size(ctx->grid_w, ctx->grid_h);
    	send_command(CMD_SET_COMPUTE);
    
    	while (!is_quit()) {
        	event ev = queue_pop();
        	process_event(&ev);
    	}

    	send_command(CMD_ABORT);
    	pthread_join(th_keyboard, NULL);
    	pthread_join(th_sdl, NULL);
    	xwin_close();
    }else{
    	error("Handshake with compute module failed, exiting...");
    	set_quit();
    }
    pthread_join(th_pipe, NULL);

    free(image);
    computation_destroy(ctx);
    io_close(fd_in);
    io_close(fd_out);
	info("Gracefully exited");
    return EXIT_OK;
}


bool module_handshake(void) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed = 0;

    while (!is_quit()) {
        send_command(CMD_GET_VERSION);
        debug("Handshake: sent command");

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;

        if (elapsed >= 10.0) {
            warning("Handshake timed out after 10 seconds");
            return false;
        }
        
        if (!queue_wait_for_data(0.5)) {
            continue;
        }

            event ev = queue_pop();
            if (ev.source == EV_MODULE && ev.data.msg->type == MSG_VERSION) {
                message *msg = ev.data.msg;
                msg_version local = VERSION;
                msg_version module = msg->data.version;

                if (module.major != local.major || module.minor != local.minor || module.patch != local.patch) {
                    warning("Module has different version: %d.%d.%d, errors may occur",
                            module.major, module.minor, module.patch);
                } else {
                    info("Handshake successful, module version matching: %d.%d.%d",
                         module.major, module.minor, module.patch);
                }
                free(msg);
                return true;
            }else if(ev.source == EV_MODULE && ev.data.msg->type == MSG_STARTUP){
            	message *msg = ev.data.msg;
            	msg_startup startup = msg->data.startup;
            	info("Startup message catched: %s", startup.message);
            	free(msg);
            	return true;
            }else {
                debug("Other data caught during handshake");
            }
    }

    return false;
}

void process_event(event *ev) {
    if (ev->source == EV_KEYBOARD || ev->source == EV_SDL) {
        switch (ev->data.param) {
            case 'q': 
                set_quit();
                break;
            case 'g': 
                info("Get version requested");
                send_command(CMD_GET_VERSION);
                break;
            case 's':
                if (computing_lock) {
                    warning("New computation parameters requested but it is discarded due to ongoing computation");
                } else {
                    info("Set new computation parameters");
					set_image_size(1280, 960);
					send_command(CMD_SET_COMPUTE);
                }
                break;
            case '1':
                if (computing_lock) {
                    warning("Computation already in progress");
                } else {
                    computing_lock = true;
                    info("Starting full image computation");
                    send_command(CMD_COMPUTE);
                }
                break;
            case 'a':
                if (!computing_lock) {
                    warning("Abort requested but it is not computing");
                } else {
                    info("Abort requested");
                    send_command(CMD_ABORT);
                    abort_comp(ctx);
                    computing_lock = false;
                }
                break;
            case 'r':
                if (computing_lock) {
                    warning("Chunk reset request discarded, it is currently computing");
                } else {
                    reset_cid(ctx);
                    info("Chunk reset request");
                }
                break;
            case 'l': {
                int w, h;
                get_grid_size(ctx, &w, &h);
                memset(image, 0, w * h * 3);
                clear_grid(ctx);
                info("Display buffer cleared");
                update_and_redraw();
                break;
            }
            case 'p':
                update_and_redraw();
                info("Image refreshed");
                break;
            case 'c':
                local_compute();
                update_and_redraw();
                break;
            default:
                warning("Unknown keyboard event received: %c", ev->data.param);
        }
    } else if (ev->source == EV_MODULE) {
        message *msg = ev->data.msg;
        switch (msg->type) {
            case MSG_OK:
                debug("Acked!");
                break;
            case MSG_VERSION:
                info("Module version: %d.%d.%d",
                        msg->data.version.major,
                        msg->data.version.minor,
                        msg->data.version.patch);
                break;
            case MSG_COMPUTE_DATA: {
                if (computing_lock) {
                    debug("Received new computed data from module");
                    update_data(ctx, &msg->data.compute_data);
                } else {
                    debug("Received computed data from module, but not computing");
                }
                break;
            }
            case MSG_DONE:
                debug("Module reports done computing chunk");
                update_and_redraw();
                if (is_done(ctx)) {
                    info("Computation ended");
                    computing_lock = false;
                } else {
                    debug("Not done yet, computing next chunk");
                    send_command(CMD_COMPUTE);
                }
                break;
            case MSG_ABORT:
                warning("Abort from Module, stopping computing");
                computing_lock = false;
                break;
            default:
                warning("Unknown message type has been received 0x%x", msg->type);
                break;
        }
        free(msg);
    }
}

void send_command(cmd_type cmd) {
    message msg;
    memset(&msg, 0, sizeof(msg));

    bool valid = false;
    switch (cmd) {
        case CMD_GET_VERSION:
            msg.type = MSG_GET_VERSION;
            valid = true;
            break;
        case CMD_ABORT:
            msg.type = MSG_ABORT;
            valid = true;
            break;
        case CMD_SET_COMPUTE:
            valid = set_compute(ctx, &msg);
            break;
        case CMD_COMPUTE:
            valid = compute(ctx, &msg);
            break;
        default:
            return;
    }

    if (!valid){
    	error("Invalid message given to send");
        return;
	}
    uint8_t buf[256];
    int len = 0;
    if (fill_message_buf(&msg, buf, sizeof(buf), &len)) {
        if (write(fd_out, buf, len) != len) {
    if (errno == EPIPE) {
        error("Pipe closed: cannot send to computation module (EPIPE)");
        set_quit();
    } else {
        error("send_message() does not send all bytes of the message!");
    }
}
    }else{
    	error("Cannot fill message buffer");
    }
}

void update_and_redraw() {
    int w, h;
    get_grid_size(ctx, &w, &h);
    update_image(ctx, w, h, image);
    xwin_redraw(w, h, image);
}

void set_image_size(int w, int h) {
    ctx->grid_w = w;
    ctx->grid_h = h;
    ctx->chunk_n_re = w/CHUNK_SIZE_FACTOR;
    ctx->chunk_n_im = h/CHUNK_SIZE_FACTOR;
    ctx_update(ctx);

    image = realloc(image, w * h * 3);
    if (!image) {
        error("Failed to reallocate image buffer");
        set_quit();
        return;
    }
    xwin_resize(w, h);
    memset(image, 0, w * h * 3);
    clear_grid(ctx);
    update_and_redraw();
}

uint8_t compute_pixel(double c_re, double c_im, double z_re, double z_im, uint8_t max_iter) {
    uint8_t k = 0;
    while (k < max_iter && z_re * z_re + z_im * z_im < 4.0) {
        double tmp = z_re * z_re - z_im * z_im + c_re;
        z_im = 2 * z_re * z_im + c_im;
        z_re = tmp;
        k++;
    }
    return k;
}

void local_compute() {
    info("Local computation on PC started");

    int w, h;
    get_grid_size(ctx, &w, &h);
    uint8_t *grid = get_internal_grid(ctx);

    double c_re = -0.4, c_im = 0.6;
    double start_re = -1.6, start_im = 1.1;
    double d_re = 3.2 / w;
    double d_im = -2.2 / h;
    uint8_t max_iter = 60;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double z_re = start_re + x * d_re;
            double z_im = start_im + y * d_im;
            uint8_t iter = compute_pixel(c_re, c_im, z_re, z_im, max_iter);
            grid[y * w + x] = iter;
        }
    }
    info("Local computation done");
}
