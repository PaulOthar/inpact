#include "inpact.h"
#include "inflate.h"
#include "file_parser.h"

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <sys/stat.h>
	#include <sys/types.h>
#endif

//---------------------------------------------------------------------------------------------------

void _push_str(char* src, char* dest, int* dest_siz){
	for(int i = 0; src[i]; i++){
		dest[dest_siz[0]++] = src[i];
	}
	dest[dest_siz[0]] = 0;
}

void _push_str_limited(char* src, int size, char* dest, int* dest_siz){
	for(int i = 0; src[i] && i < size; i++){
		dest[dest_siz[0]++] = src[i];
	}
	dest[dest_siz[0]] = 0;
}

//---------------------------------------------------------------------------------------------------

static void _unpack_entry_file(package_entry_header* header, void* data, char* path_buffer, int path_size){
	_push_str("/", path_buffer, &path_size);
	_push_str_limited((char*)&header->name, 8, path_buffer, &path_size);
	_push_str(".", path_buffer, &path_size);
	_push_str_limited((char*)&header->type, 4, path_buffer, &path_size);

	FILE* file = fopen(path_buffer, "w+b");
	if(!file){ return; }

	switch(header->type){
		case 0x706d62:{//0xpmb = .bmp}
			file_image img = {0};
			img.width = header->metadata.image.width;
			img.height = header->metadata.image.heigth;
			img.bits_per_pixel = header->metadata.image.bits_per_pixel;
			img.palette_size = header->metadata.image.palette_entries;
			img.data_size = header->original_size - (4 * img.palette_size);

			img.data = data;
			img.palette = (uint8_t*)data + img.data_size;

			file_parser_write_bmp(file, &img);
		break;}
	default:
		fwrite(data, 1, header->original_size, file);
		break;
	}

	fflush(file);
	fclose(file);
}

//---------------------------------------------------------------------------------------------------

static void _unpack_file_recursive(inpact_package_state* package, FILE* file, char* path_buffer, int path_size);

void inpact_unpack_file(FILE* file, char* path){
	char path_buffer[1024]; int path_size = 0;
	_push_str(path, path_buffer, &path_size);

	inpact_package_state pstate;
	inpact_package_read(&pstate, file);

	inpact_surf_file_point(&pstate, file, 0);
	_unpack_file_recursive(&pstate, file, path_buffer, path_size);
}

static void _unpack_file_recursive(inpact_package_state* package, FILE* file, char* path_buffer, int path_size){
	package_entry_header header = package->cursor.header;

	int wrote_stuff = 0;
	if(header.type != 0x524944){//if it is not a directory
		uint8_t data[header.original_size];
		inpact_surf_file_read(package, file, data);
		_unpack_entry_file(&header, data, path_buffer, path_size);
		wrote_stuff = 1;
	}

	if(package->cursor.index < package->header.entry_count){
		inpact_surf_file_point(package, file, package->cursor.index + 1);
	}

	if(wrote_stuff){ return; }

	_push_str("/", path_buffer, &path_size);
	_push_str_limited((char*)&header.name, 8, path_buffer, &path_size);

	mkdir(path_buffer, 0755);

	for(uint32_t i = 0; i < header.metadata.dir.file_count; i++){
		_unpack_file_recursive(package, file, path_buffer, path_size);
	}
}
