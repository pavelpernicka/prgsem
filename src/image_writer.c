#include <png.h>
#include <jpeglib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool save_image_png(const char *path, uint8_t *image, int w, int h) {
  FILE *fp = fopen(path, "wb");
  if (!fp)
    return false;

  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    return false;
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    return false;
  if (setjmp(png_jmpbuf(png_ptr)))
    return false;

  png_init_io(png_ptr, fp);
  png_set_IHDR(
      png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png_ptr, info_ptr);

  for (int y = 0; y < h; ++y)
    png_write_row(png_ptr, image + y * w * 3);

  png_write_end(png_ptr, NULL);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
  return true;
}

bool save_image_jpg(const char *path, uint8_t *image, int w, int h) {
  FILE *fp = fopen(path, "wb");
  if (!fp)
    return false;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fp);

  cinfo.image_width = w;
  cinfo.image_height = h;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 95, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1] = {image + cinfo.next_scanline * w * 3};
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(fp);
  return true;
}
