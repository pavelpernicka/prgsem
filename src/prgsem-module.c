#include "prgsem_module.h"
#include "prg_io_nonblock.h"
#include "pipe_thread.h"
#include "messages.h"
#include "event_queue.h"
#include "keyboard_thread.h"
#include "version.h"

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>

static pthread_t th_pipe;
static pthread_t th_keyboard;

const char *argp_program_version = MOD_VERSION;

static struct argp_option options[] = {
    {"pipe-in",  'i', "FILE", 0, "Input pipe path (default: /tmp/computational_module.in)"},
    {"pipe-out", 'o', "FILE", 0, "Output pipe path (default: /tmp/computational_module.out)"},
    {"log-level", 'v', "LEVEL", 0, "Set log verbosity (0=error, 1=warn, 2=info, 3=debug)"},
    {0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
        case 'i': args->pipe_in = arg; break;
        case 'o': args->pipe_out = arg; break;
        case 'v':
            args->log_level = atoi(arg);
            if (args->log_level < 0 || args->log_level > 3) {
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return EXIT_ERROR;
}

static struct argp argp = {options, parse_opt, NULL, MOD_DOCSTRING};

bool check_params(const message *msg) {
    if (!msg) return false;
    double c_re = msg->data.set_compute.c_re;
    double c_im = msg->data.set_compute.c_im;
    double d_re = msg->data.set_compute.d_re;
    double d_im = msg->data.set_compute.d_im;
    uint8_t max_iter = msg->data.set_compute.n;

    return max_iter > 0 && max_iter <= 255 &&
           d_re > 0.0 && fabs(d_re) <= 10.0 &&
           d_im != 0.0 && fabs(d_im) <= 10.0 &&
           !isnan(c_re) && !isnan(c_im);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    struct arguments args = {
        .pipe_in = "/tmp/computational_module.in",
        .pipe_out = "/tmp/computational_module.out",
        .log_level = LOG_LEVEL_INFO
    };

    module_state state = {
        .c_re = -0.4,
        .c_im = 0.6,
        .d_re = 0.01,
        .d_im = 0.01,
        .max_iter = 60
    };

    argp_parse(&argp, argc, argv, 0, 0, &args);
    set_log_level(args.log_level);

    info("Waiting for graphical application...");
    state.fd_in = io_open_read(args.pipe_in);
    state.fd_out = io_open_write(args.pipe_out);
    if (state.fd_out == -1 || state.fd_in == -1) {
        error("Cannot open pipes");
        return ERR_FILE_OPEN;
    }
    debug("Pipe opened");

    queue_init();
    pipe_set_event_pusher(queue_push);
    pipe_set_input_pipe_fd(state.fd_in);
    keyboard_set_event_pusher(queue_push);

    pthread_create(&th_pipe, NULL, pipe_thread, NULL);
    pthread_create(&th_keyboard, NULL, keyboard_thread, NULL);

    send_command(&state, MSG_STARTUP);

    while (!is_quit()) {
        event ev = queue_pop();
        process_event(&state, &ev);
    }

    send_command(&state, MSG_ABORT);
    pthread_join(th_pipe, NULL);
    pthread_join(th_keyboard, NULL);

    io_close(state.fd_in);
    io_close(state.fd_out);
    info("Gracefully exited");
    return EXIT_OK;
}

void send_command(module_state *state, message_type cmd) {
    message msg;
    memset(&msg, 0, sizeof(msg));

    bool valid = false;
    switch (cmd) {
        case MSG_VERSION: {
            msg_version local = VERSION;
            msg.type = MSG_VERSION;
            msg.data.version = local;
            info("Sending MSG_VERSION containing %d.%d.%d", local.major, local.minor, local.patch);
            valid = true;
            break;
        }
        case MSG_STARTUP:
            msg.type = MSG_STARTUP;
            memcpy(msg.data.startup.message, MOD_STARTUP_MSG, STARTUP_MSG_LEN);
            info("Sending MSG_STARTUP");
            valid = true;
            break;
        case MSG_ABORT:
            info("Sending MSG_ABORT");
            msg.type = MSG_ABORT;
            valid = true;
            break;
        case MSG_DONE:
            info("Sending MSG_DONE");
            msg.type = MSG_DONE;
            valid = true;
            break;
        case MSG_OK:
            info("Sending MSG_OK");
            msg.type = MSG_OK;
            valid = true;
            break;
        case MSG_ERROR:
            info("Sending MSG_ERROR");
            msg.type = MSG_ERROR;
            valid = true;
            break;
        default:
            error("Unsupported command type: %d", cmd);
            return;
    }

    if (!valid) {
        error("Invalid message given to send");
        return;
    }

    send_message(state, &msg);
}

void send_message(module_state *state, const message *msg) {
    uint8_t buf[MESSAGE_BUFF_SIZE];
    int len = 0;
    if (fill_message_buf(msg, buf, sizeof(buf), &len)) {
        if (write(state->fd_out, buf, len) != len) {
            error("Failed to write full message to output pipe");
            if (errno == EPIPE) {
        		error("Pipe closed: cannot send to computation module (EPIPE)");
        		set_quit();
        	}
        }
    } else {
        error("Could not serialize message");
    }
}

void process_event(module_state *state, event *ev) {
    if (ev->type == EV_QUIT) {
        set_quit();
    } else if (ev->source == EV_PIPE) {
        message *msg = ev->data.msg;
        switch (msg->type) {
            case MSG_OK:
                info("MSG_OK received");
                break;
            case MSG_GET_VERSION:
                info("MSG_GET_VERSION received");
                send_command(state, MSG_VERSION);
                break;
            case MSG_ABORT:
                info("MSG_ABORT received");
                send_command(state, MSG_OK);
                break;
            case MSG_SET_COMPUTE:
                info("MSG_SET_COMPUTE received");
                if (!check_params(msg)) {
                    error("Invalid compute parameters: c = %.3f + %.3fi, d = %.5f, %.5f, n = %d",
                          msg->data.set_compute.c_re,
                          msg->data.set_compute.c_im,
                          msg->data.set_compute.d_re,
                          msg->data.set_compute.d_im,
                          msg->data.set_compute.n);
                    send_command(state, MSG_ERROR);
                    break;
                }
                state->c_re = msg->data.set_compute.c_re;
                state->c_im = msg->data.set_compute.c_im;
                state->d_re = msg->data.set_compute.d_re;
                state->d_im = msg->data.set_compute.d_im;
                state->max_iter = msg->data.set_compute.n;
                debug("Params set: c = %.3f + %.3fi, d = %.5f, %.5f, n = %d", state->c_re, state->c_im, state->d_re, state->d_im, state->max_iter);
                send_command(state, MSG_OK);
                break;
            case MSG_COMPUTE:
                info("MSG_COMPUTE received");
                if (msg->data.compute.n_re <= 0 || msg->data.compute.n_im <= 0) {
                    warning("Invalid compute region size: n_re = %d, n_im = %d", msg->data.compute.n_re, msg->data.compute.n_im);
                    send_command(state, MSG_ERROR);
                    break;
                }
                compute_chunk_and_send(state,
                    msg->data.compute.cid,
                    msg->data.compute.re,
                    msg->data.compute.im,
                    msg->data.compute.n_re,
                    msg->data.compute.n_im);
                send_command(state, MSG_DONE);
                break;
            default:
                warning("Unknown message type received: 0x%x", msg->type);
                send_command(state, MSG_ERROR);
                break;
        }
        free(msg);
    } else if (ev->source == EV_KEYBOARD) {
        char key = ev->data.param;
        if (key == 'q') {
            info("Keyboard: 'q' pressed, quitting");
            set_quit();
        } else if (key == 'a') {
            info("Keyboard: 'a' pressed, sending abort");
            send_command(state, MSG_ABORT);
        } else {
            debug("Keyboard: unhandled key '%c'", key);
        }
    }
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

void compute_chunk_and_send(module_state *state, uint8_t cid, double re0, double im0, uint8_t n_re, uint8_t n_im) {
    for (uint8_t y = 0; y < n_im; ++y) {
        for (uint8_t x = 0; x < n_re; ++x) {
            double z_re = re0 + x * state->d_re;
            double z_im = im0 + y * state->d_im;
            uint8_t iter = compute_pixel(state->c_re, state->c_im, z_re, z_im, state->max_iter);

            message data_msg = {
                .type = MSG_COMPUTE_DATA,
                .data.compute_data = {
                    .cid = cid, .i_re = x, .i_im = y, .iter = iter
                }
            };
            send_message(state, &data_msg);
        }
    }
}

