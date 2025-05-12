#include "computation.h"
#include "messages.h"
#include "common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

comp_ctx* computation_create(void) {
    comp_ctx *ctx = safe_alloc(sizeof(comp_ctx));
    *ctx = (comp_ctx) {
        .c_re = -0.4,
        .c_im = 0.6,
        .n = 60,
        .range_re_min = -1.6,
        .range_re_max = 1.6,
        .range_im_min = -1.1,
        .range_im_max = 1.1,
        .grid_w = 640,
        .grid_h = 480,
        .chunk_n_re = 64,
        .chunk_n_im = 48
    };
    return ctx;
}

void ctx_update(comp_ctx *ctx) {
    int w = ctx->grid_w;
    int h = ctx->grid_h;

    if (ctx->chunk_n_re == 0) ctx->chunk_n_re = 64;
    if (ctx->chunk_n_im == 0) ctx->chunk_n_im = 48;

    ctx->d_re = (ctx->range_re_max - ctx->range_re_min) / (1.0 * w);
    ctx->d_im = -(ctx->range_im_max - ctx->range_im_min) / (1.0 * h);
    ctx->nbr_chunks = (w * h) / (ctx->chunk_n_re * ctx->chunk_n_im);
	debug("Updated nbrchunks: %d", ctx->nbr_chunks);
    ctx->cid = 0;
    ctx->cur_x = 0;
    ctx->cur_y = 0;
    ctx->chunk_re = ctx->range_re_min;
    ctx->chunk_im = ctx->range_im_max;
    ctx->computing = false;
    ctx->done = false;
    ctx->abort = false;

    free(ctx->grid);
    ctx->grid = safe_alloc(w * h);
}

void computation_destroy(comp_ctx *ctx) {
    if (!ctx) return;
    free(ctx->grid);
    free(ctx);
}

bool is_computing(comp_ctx *ctx) { return ctx->computing; }
bool is_done(comp_ctx *ctx) { return ctx->done; }
bool is_abort(comp_ctx *ctx) { return ctx->abort; }

void get_grid_size(comp_ctx *ctx, int *w, int *h) {
    *w = ctx->grid_w;
    *h = ctx->grid_h;
}

void abort_comp(comp_ctx *ctx) {
    ctx->abort = false;
    ctx->done = true;
    ctx->computing = false;
}

bool set_compute(comp_ctx *ctx, message *msg) {
    assertion(msg != NULL, __func__, __LINE__, __FILE__);
    bool ret = !ctx->computing;
    if (ret) {
        msg->type = MSG_SET_COMPUTE;
        msg->data.set_compute.c_re = ctx->c_re;
		msg->data.set_compute.c_im = ctx->c_im;
		msg->data.set_compute.d_re = ctx->d_re;
		msg->data.set_compute.d_im = ctx->d_im;
		msg->data.set_compute.n = ctx->n;
        ctx->done = false;
    }
    return ret;
}

bool compute(comp_ctx *ctx, message *msg) {
    assertion(msg != NULL, __func__, __LINE__, __FILE__);
	debug("COMPUTE: cid=%d / %d", ctx->cid, ctx->nbr_chunks);
    if (!ctx->computing) {
        // First chunk
        reset_cid(ctx);
        ctx->computing = true;
        ctx->done = false;
    } else {
        // Next chunk
        ctx->cid++;
        if (ctx->cid >= ctx->nbr_chunks) {
            return false;
        }

        ctx->cur_x += ctx->chunk_n_re;
        ctx->chunk_re += ctx->chunk_n_re * ctx->d_re;

        if (ctx->cur_x >= ctx->grid_w) {
            ctx->cur_x = 0;
            ctx->cur_y += ctx->chunk_n_im;
            ctx->chunk_re = ctx->range_re_min;
            ctx->chunk_im += ctx->chunk_n_im * ctx->d_im;
        }
    }

    msg->type = MSG_COMPUTE;
    msg->data.compute.cid = ctx->cid;
    msg->data.compute.re = ctx->chunk_re;
    msg->data.compute.im = ctx->chunk_im;
    msg->data.compute.n_re = ctx->chunk_n_re;
    msg->data.compute.n_im = ctx->chunk_n_im;

    return true;
}

void update_image(comp_ctx *ctx, int w, int h, unsigned char *img) {
    assertion(img && ctx->grid && w == ctx->grid_w && h == ctx->grid_h, __func__, __LINE__, __FILE__);
    for (int i = 0; i < w * h; ++i) {
        double t = 1.0 * ctx->grid[i] / (ctx->n + 1.0);
        *(img++) = 9 * (1 - t) * t * t * t * 255;
        *(img++) = 15 * (1 - t) * (1 - t) * t * t * 255;
        *(img++) = 8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255;
    }
}

void update_data(comp_ctx *ctx, const msg_compute_data *data) {
    assertion(data != NULL, __func__, __LINE__, __FILE__);
    debug("RECEIVED: data->cid=%d, ctx->cid=%d", data->cid, ctx->cid);
    if (data->cid == ctx->cid) {
        int idx = ctx->cur_x + data->i_re + (ctx->cur_y + data->i_im) * ctx->grid_w;
        if (idx >= 0 && idx < (ctx->grid_w * ctx->grid_h)) {
            ctx->grid[idx] = data->iter;
        }
        if ((ctx->cid + 1) >= ctx->nbr_chunks &&
            (data->i_re + 1) == ctx->chunk_n_re &&
            (data->i_im + 1) == ctx->chunk_n_im) {
            ctx->done = true;
            ctx->computing = false;
        }
    } else {
        error("Received chunk with unexpected chunk id (cid): %d", ctx->cid);
    }
}

void clear_grid(comp_ctx *ctx) {
    if (ctx->grid) {
        memset(ctx->grid, 0, ctx->grid_w * ctx->grid_h);
    }
}

int get_current_cid(comp_ctx *ctx) {
    return ctx->cid;
}

void reset_cid(comp_ctx *ctx) {
    ctx->cid = 0;
    ctx->cur_x = 0;
    ctx->cur_y = 0;
    ctx->chunk_re = ctx->range_re_min;
    ctx->chunk_im = ctx->range_im_max;
    ctx->computing = false;
}

uint8_t* get_internal_grid(comp_ctx *ctx) {
    return ctx->grid;
}

