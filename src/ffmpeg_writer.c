#include "ffmpeg_writer.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <string.h>

struct ffmpeg_writer {
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  AVFrame *frame;
  struct SwsContext *sws_ctx;
  int width, height;
  int frame_index;
};

ffmpeg_writer *
ffmpeg_writer_create(const char *filename, int width, int height, int fps) {
  ffmpeg_writer *w = calloc(1, sizeof(ffmpeg_writer));
  if (!w)
    return NULL;

  w->width = width;
  w->height = height;
  w->frame_index = 0;

  avformat_alloc_output_context2(&w->fmt_ctx, NULL, NULL, filename);
  if (!w->fmt_ctx)
    return NULL;

  const AVCodec *codec = avcodec_find_encoder(VIDEO_CODEC);
  if (!codec)
    return NULL;

  w->stream = avformat_new_stream(w->fmt_ctx, NULL);
  if (!w->stream)
    return NULL;
  w->stream->id = 0;

  w->codec_ctx = avcodec_alloc_context3(codec);
  if (!w->codec_ctx)
    return NULL;

  w->codec_ctx->codec_id = codec->id;
  w->codec_ctx->bit_rate = VIDEO_BITRATE;
  w->codec_ctx->width = width;
  w->codec_ctx->height = height;
  w->codec_ctx->time_base = (AVRational){1, fps};
  w->codec_ctx->framerate = (AVRational){fps, 1};
  w->codec_ctx->gop_size = 10;
  w->codec_ctx->max_b_frames = 1;
  w->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  av_opt_set(w->codec_ctx->priv_data, "crf", VIDEO_CFR, 0);

  if (w->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    w->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  w->stream->time_base = w->codec_ctx->time_base;

  if (avcodec_open2(w->codec_ctx, codec, NULL) < 0)
    return NULL;

  if (avcodec_parameters_from_context(w->stream->codecpar, w->codec_ctx) < 0)
    return NULL;

  if (!(w->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&w->fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0)
      return NULL;
  }

  if (avformat_write_header(w->fmt_ctx, NULL) < 0)
    return NULL;

  w->frame = av_frame_alloc();
  if (!w->frame)
    return NULL;

  w->frame->format = w->codec_ctx->pix_fmt;
  w->frame->width = width;
  w->frame->height = height;
  if (av_frame_get_buffer(w->frame, 32) < 0)
    return NULL;

  w->sws_ctx = sws_getContext(
      width, height, AV_PIX_FMT_RGB24, width, height, AV_PIX_FMT_YUV420P,
      SWS_BICUBIC, NULL, NULL, NULL
  );
  if (!w->sws_ctx)
    return NULL;

  return w;
}

void ffmpeg_writer_add_frame(ffmpeg_writer *w, uint8_t *rgb_data) {
  const uint8_t *src_slice[1] = {rgb_data};
  int src_stride[1] = {3 * w->width};

  sws_scale(
      w->sws_ctx, src_slice, src_stride, 0, w->height, w->frame->data,
      w->frame->linesize
  );

  w->frame->pts = w->frame_index++;

  AVPacket *pkt = av_packet_alloc();
  if (!pkt)
    return;

  if (avcodec_send_frame(w->codec_ctx, w->frame) < 0) {
    av_packet_free(&pkt);
    return;
  }

  while (1) {
    int ret = avcodec_receive_packet(w->codec_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0)
      break;

    av_packet_rescale_ts(pkt, w->codec_ctx->time_base, w->stream->time_base);
    pkt->stream_index = w->stream->index;

    av_interleaved_write_frame(w->fmt_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
}

void ffmpeg_writer_close(ffmpeg_writer *w) {
  if (!w)
    return;

  AVPacket *pkt = av_packet_alloc();
  avcodec_send_frame(w->codec_ctx, NULL);
  while (avcodec_receive_packet(w->codec_ctx, pkt) == 0) {
    av_packet_rescale_ts(pkt, w->codec_ctx->time_base, w->stream->time_base);
    pkt->stream_index = w->stream->index;
    av_interleaved_write_frame(w->fmt_ctx, pkt);
    av_packet_unref(pkt);
  }
  av_packet_free(&pkt);

  av_write_trailer(w->fmt_ctx);

  if (!(w->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_close(w->fmt_ctx->pb);

  sws_freeContext(w->sws_ctx);
  av_frame_free(&w->frame);
  avcodec_free_context(&w->codec_ctx);
  avformat_free_context(w->fmt_ctx);
  free(w);
}
