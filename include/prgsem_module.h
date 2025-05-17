#ifndef PRGSEM_MAIN_H
#define PRGSEM_MAIN_H

#include "messages.h"
#include "event_queue.h"

#define MOD_DOCSTRING "Fractal computation module"
#define MOD_STARTUP_MSG "pernipa1"

typedef struct {
    int fd_in;
    int fd_out;
    double c_re, c_im;
    double d_re, d_im;
    uint8_t max_iter;
} module_state;

struct arguments {
    const char *pipe_in;
    const char *pipe_out;
    int log_level;
};

void process_event(module_state *state, event *ev);
void send_command(module_state *state, message_type cmd);
void send_message(module_state *state, const message *msg);
void compute_chunk_and_send(module_state *state, uint8_t cid, double re0, double im0, uint8_t n_re, uint8_t n_im);
void compute_chunk_burst_and_send(module_state *state, uint8_t cid, double re0, double im0, uint8_t n_re, uint8_t n_im);
uint8_t compute_pixel(double c_re, double c_im, double z_re, double z_im, uint8_t max_iter);

#endif
