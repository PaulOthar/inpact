#ifndef FILE_PARSER
#define FILE_PARSER

#include <stdio.h>

typedef struct /*file_image*/{
	int width, height;
	int bits_per_pixel;
	int palette_size;

	int data_size;

	void* palette;
	void* data;
}file_image;

/**
 * Reads a bitmap, and stores the pixel content to a buffer.
 * If the buffer's data pointer is null, it will just fill the header.
 * The specified file should he already open, once this function is called.
 * @fn int file_parser_read_bmp(FILE, file_image*)
 * @param file file to be parsed
 * @param buffer
 * @return 0 if success | error code
 */
int file_parser_read_bmp(FILE* file, file_image* buffer);
void file_parser_write_bmp(FILE* file, file_image* buffer);

#endif
