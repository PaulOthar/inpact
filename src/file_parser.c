#include "file_parser.h"
#include <stdint.h>

static void _read_bytes(FILE* file, void* dest, int offset, int size, int count){
	fseek(file, offset, SEEK_SET);
	fread(dest, size, count, file);
}

static int _read_value(FILE* file, int offset, int size, int count){
	int value = 0;
	_read_bytes(file, &value, offset, size, count);
	return value;
}

static void _write_bytes(FILE* file, void* src, int offset, int size, int count){
	fseek(file, offset, SEEK_SET);
	fwrite(src, size, count, file);
}

static void _write_value(FILE* file, int value, int offset, int size, int count){
	_write_bytes(file, &value, offset, size, count);
}

int file_parser_read_bmp(FILE* file, file_image* buffer){
	if(_read_value(file, 0, 2, 1) != 0x4D42){ return 1; }//byte 0-1 = Magic number

	_read_bytes(file, &buffer->width, 18, 4, 1);//byte 18-21
	_read_bytes(file, &buffer->height, 22, 4, 1);//byte 22-25
	_read_bytes(file, &buffer->bits_per_pixel, 28, 1, 1);//byte 28

	if(buffer->bits_per_pixel < 16){
		_read_bytes(file, &buffer->palette_size, 46, 4, 1);//byte 46-49
		if(buffer->palette){//Palette is somewhat optional
			_read_bytes(file, buffer->palette, 54, 4, buffer->palette_size);//byte 54-57
		}
	}

	//buffer->data_size = (buffer->width * buffer->height * buffer->bits_per_pixel) / 8;
	buffer->data_size = _read_value(file, 34, 4, 1);//byte 34-37 = Data size

	if(!buffer->data){ return -1; }

	fseek(file, 0, SEEK_END);

	int line_size = (buffer->bits_per_pixel * buffer->width) / 8;
	int byte_index = ftell(file) - line_size;
	int height = buffer->height;

	uint8_t* data = buffer->data;
	for(int i = 0; i < height; i++){
		_read_bytes(file, data, byte_index, 1, line_size);
		data += line_size;
		byte_index -= line_size;
	}

	if(buffer->bits_per_pixel >= 16){ return 0; }

	data = buffer->data;

	int mask = (1 << buffer->bits_per_pixel) - 1;
	int total_size = (buffer->width * buffer->height * buffer->bits_per_pixel) / 8;
	for(int i = 0; i < total_size; i++){
		int fix_buffer = 0;
		int byte = data[i];

		for(int l = 0; l < 8; l += buffer->bits_per_pixel){
			fix_buffer <<= buffer->bits_per_pixel;
			fix_buffer |= byte & mask;
			byte >>= buffer->bits_per_pixel;
		}

		data[i] = fix_buffer;
	}

	return 0;
}

void file_parser_write_bmp(FILE* file, file_image* img){
	_write_value(file, 0x4D42, 0, 2, 1);//byte 0-1 = Magic number
	_write_value(file, 54 + img->data_size + (img->palette_size * 4), 2, 4, 1);//byte 2-5 = File size
	//Reserved 1
	//Reserved 2
	_write_value(file, 54, 10, 4, 1);//byte 10-13 = Header offset

	_write_value(file, 40, 14, 4, 1);//byte 14-17 = Second header size
	_write_value(file, img->width, 18, 4, 1);//byte 18-21 = Width in pixels
	_write_value(file, img->height, 22, 4, 1);//byte 22-25 = Height in pixels
	_write_value(file, 1, 26, 2, 1);//byte 26-27 = Color planes, which is always 1 (??)
	_write_value(file, img->bits_per_pixel, 28, 2, 1);//byte 28-29 = Bits per pixel
	_write_value(file, 0, 30, 4, 1);//byte 30-33 = Compression, 0 = none
	_write_value(file, img->data_size, 34, 4, 1);//byte 34-37 = Data size (in bytes)
	_write_value(file, 2834, 38, 4, 1);//byte 38-41 = biXPelsPerMeter (???)
	_write_value(file, 2834, 42, 4, 1);//byte 38-45 = biYPelsPerMeter (???)
	_write_value(file, img->palette_size, 46, 4, 1);//byte 46-49 = Palette size
	_write_value(file, img->palette_size, 50, 4, 1);//byte 50-53 = Important palette size

	_write_bytes(file, img->palette, 54, 4, img->palette_size);//Palette

	//for 32bpp, it should die exactly here, since we dont have indexing

	int line_size = (img->bits_per_pixel * img->width) / 8;

	uint8_t* original_data = img->data;
	uint8_t inverted_data[img->data_size];

	int mask = (1 << img->bits_per_pixel) - 1;

	//Need to write from bottom to top linewise, and reverse the pixel position within the byte
	for(int i = 0; i < img->data_size;){
		int inverted_index = img->data_size - i - line_size;
		for(int l = 0; l < line_size; l++){
			int fix_buffer = 0;
			int byte = original_data[i++];

			for(int l = 0; l < 8; l += img->bits_per_pixel){
				fix_buffer <<= img->bits_per_pixel;
				fix_buffer |= byte & mask;
				byte >>= img->bits_per_pixel;
			}

			inverted_data[inverted_index++] = fix_buffer;
		}
	}

	_write_bytes(file, inverted_data, 54 + (img->palette_size * 4), 1, img->data_size);
}
