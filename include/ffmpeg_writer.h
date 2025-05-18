#ifndef __FFMPEG_WRITER_H__
#define __FFMPEG_WRITER_H__

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdint.h>

#define VIDEO_BITRATE 10 * 1000 * 1000; // 10 Mbps
#define VIDEO_CFR "18"                  // 0 = lossless, 18 = visually lossless
#define VIDEO_CODEC AV_CODEC_ID_H264

typedef struct {
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  struct SwsContext *sws_ctx;
  AVFrame *frame;
  int frame_index;
  int width, height;
} ffmpeg_writer;

ffmpeg_writer *
ffmpeg_writer_create(const char *filename, int width, int height, int fps);
void ffmpeg_writer_add_frame(ffmpeg_writer *writer, uint8_t *rgb_data);

// Close writer and save video
void ffmpeg_writer_close(ffmpeg_writer *writer);

#endif
