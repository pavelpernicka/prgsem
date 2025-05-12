#include "keyboard_thread.h"
#include "common.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static event_pusher_fn event_push = NULL;

void keyboard_set_event_pusher(event_pusher_fn handler) {
    event_push = handler;
}

static int is_valid_key_char(int c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

void *keyboard_thread(void *arg) {
    debug("keyboard_thread - started");

    call_termios(0);  // Set terminal to raw mode

    int c;
    while (!is_quit()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = 100000  // 100 ms
        };

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            c = getchar();
            if (c == EOF) continue;

            if (is_valid_key_char(c)) {
                event ev = {
                    .source = EV_KEYBOARD,
                    .type = EV_KEYBOARD,
                    .data.param = c
                };
                event_push(ev);

                if (c == 'q') {
                    set_quit();

                    event ev_quit = {
                        .source = EV_KEYBOARD,
                        .type = EV_QUIT
                    };
                    event_push(ev_quit);
                    break;
                }
            }
        }
    }

    call_termios(1);  // Restore terminal settings
    debug("keyboard_thread - stop");

    return NULL;
}

void call_termios(int reset) {
    static struct termios tio, tioOld;
    tcgetattr(STDIN_FILENO, &tio);

    if (reset) {
        // Restore saved terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &tioOld);
    } else {
        // Save current settings and set raw mode
        tioOld = tio;
        cfmakeraw(&tio);

        // Enable output processing and convert \n to \r\n
        tio.c_oflag |= OPOST;
        tio.c_oflag |= ONLCR;

        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    }
}

