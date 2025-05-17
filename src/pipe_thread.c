#include "pipe_thread.h"
#include "messages.h"
#include "common.h"
#include "prg_io_nonblock.h"

#include <stdio.h>

static event_pusher_fn event_pusher = NULL;
static int input_pipe_fd = -1;

void pipe_set_event_pusher(event_pusher_fn handler) {
    event_pusher = handler;
}

void pipe_set_input_pipe_fd(int fd) {
    input_pipe_fd = fd;
}

void *pipe_thread(void *arg) {
    debug("pipe_thread - start");

    uint8_t msg_buf[MESSAGE_BUFF_SIZE];
    int i = 0, len = 0;
    unsigned char c;

    // Discard garbage before the first message
    //while (io_getc_timeout(input_pipe_fd, 100, &c) > 0) {}

    while (!is_quit()) {
        int r = io_getc_timeout(input_pipe_fd, 100, &c);

        if (r > 0) {
            if (i == 0) {
                len = 0;
                if (get_message_size(c, &len)) {
                    msg_buf[i++] = c;
                } else {
                    debug("unknown message type 0x%x", c);
                }
            } else {
                msg_buf[i++] = c;
            }

            if (len > 0 && i == len) {
                message *msg = safe_alloc(sizeof(message));
                if (parse_message_buf(msg_buf, len, msg)) {
                    event ev = {
                        .type = EV_PIPE,
                        .data.msg = msg
                    };
                    //debug("pipe_thread received message");
                    event_pusher(ev);
                } else {
                    error("cannot parse message type %d", msg_buf[0]);
                    free(msg);
                }
                i = len = 0;
            }

        } else if (r < 0) {
            error("cannot read from pipe, trying to exit");
            set_quit();

            event ev = {
                .source = EV_KEYBOARD,
                .type = EV_QUIT,
                .data.param = 'q'
            };
            event_pusher(ev);
        }

        // usleep(10);
    }

    debug("pipe_thread - stop");
    return NULL;
}

