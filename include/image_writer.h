#ifndef __IMAGE_WRITER_H__
#define __IMAGE_WRITER_H__

bool save_image_png(const char *path, uint8_t *image, int w, int h);
bool save_image_jpg(const char *path, uint8_t *image, int w, int h);

#endif
