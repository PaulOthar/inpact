#include "inpact.h"
#include "file_parser.h"
#include "inflate.h"
#include <unistd.h>
#include <dirent.h>

static void _push_entry_to_file(inpact_package_state* package, FILE* file, void* data, int size){
	package_entry_header* header = &package->cursor.header;

	fseek(file, package->header.entry_end, SEEK_SET);

	fwrite(header, sizeof(package_entry_header), 1, file);
	fwrite(data, 1, size, file);
}

static void _push_entry(inpact_package_state* package, void* file, void* data, int size){
	package_entry_header* header = &package->cursor.header;

	header->size = size;
	header->original_size = size;
	header->flags = 0;

	int compressed_size = inflate_calculate_optimal_size(data, size, 0);
	uint8_t compressed[compressed_size];

	int optimal_size = size;
	void* optimal_buffer = data;

	if(compressed_size && (compressed_size < size)){
		inflate_compress(data, compressed, size, 0);

		optimal_buffer = compressed;
		optimal_size = compressed_size;

		header->size = compressed_size;
		header->flags = 1;
	}

	_push_entry_to_file(package, file, optimal_buffer, optimal_size);

	package->header.entry_end += optimal_size + sizeof(package_entry_header);
	package->header.entry_count += 1;
}

//---------------------------------------------------------------------------------------------------
//----- FILE VERSION -----
//---------------------------------------------------------------------------------------------------

void inpact_package_read(inpact_package_state* package, FILE* file){
	inpact_package_init(package);

	fseek(file, 0, SEEK_SET);
	fread(&package->header, sizeof(package_header), 1, file);
}

void inpact_package_write_update(inpact_package_state* package, FILE* file){
	fseek(file, 0, SEEK_SET);
	fwrite(&package->header, sizeof(package_header), 1, file);

	off_t curr_addr = sizeof(package_header);
	off_t entry_end = package->header.entry_end;
	off_t summary_end = package->header.entry_end;

	while(curr_addr < entry_end){
		fseek(file, curr_addr, SEEK_SET);

		package_entry_header entry = {0};

		fread(&entry, sizeof(package_entry_header), 1, file);

		package_entry_label label;
		label.name = entry.name;
		label.type = entry.type;
		label.location = curr_addr;

		fseek(file, summary_end, SEEK_SET);
		fwrite(&label, sizeof(package_entry_label), 1, file);

		curr_addr += entry.size + sizeof(package_entry_header);
		summary_end += sizeof(package_entry_label);
	}

	fflush(file);
	ftruncate(fileno(file), summary_end);
}

//---------------------------------------------------------------------------------------------------

void inpact_push_data(inpact_package_state* package, FILE* file, char* name, char* type, inpact_metadata metadata, void* data, int size){
	package_entry_header* header = &package->cursor.header;

	header->name = inpact_build_string_number(name, 8);
	header->type = inpact_build_string_number(type, 4);
	header->metadata = metadata;

	_push_entry(package, file, data, size);
}

void inpact_push_file(inpact_package_state* package, FILE* file, char* name, char* type, FILE* entry){
	package_entry_header* header = &package->cursor.header;

	header->name = inpact_build_string_number(name, 8);
	header->type = inpact_build_string_number(type, 4);

	switch(header->type){
	case 0x706d62:{//0xpmb = .bmp
		file_image img = {0};
		file_parser_read_bmp(entry, &img);

		int file_size = img.data_size + (img.palette_size * 4);
		uint8_t image_data[file_size];

		img.data = image_data;
		img.palette = image_data + img.data_size;
		file_parser_read_bmp(entry, &img);

		header->metadata.image.width = img.width;
		header->metadata.image.heigth = img.height;
		header->metadata.image.bits_per_pixel = img.bits_per_pixel;
		header->metadata.image.palette_entries = img.palette_size;

		_push_entry(package, file, image_data, file_size);
		break;
	}
	default:
		fseek(entry, 0, SEEK_END);
		int entry_size = ftell(entry);

		fseek(entry, 0, SEEK_SET);
		uint8_t data[entry_size];
		fread(data, 1, entry_size, entry);

		_push_entry(package, file, data, entry_size);
		break;
	}
}

void inpact_push_dir(inpact_package_state* package, FILE* file, char* name, int entry_count){
	package_entry_header* header = &package->cursor.header;

	header->name = inpact_build_string_number(name, 8);
	header->type = inpact_build_string_number("DIR", 4);
	header->metadata.dir.file_count = entry_count;

	_push_entry(package, file, 0, 0);
}

//---------------------------------------------------------------------------------------------------

static void _push_str(char* src, char* dest, int* dest_siz);
static void _pack_recursive_directory(inpact_package_state* package, void* file, char* path, char* path_buffer, int path_size);
static void _pack_recursive_file(inpact_package_state* package, void* file, char* name, char* path);

void inpact_push_recursive(inpact_package_state* package, FILE* file, char* path){
	char path_buffer[1024];

	int path_size = 0;
	int slash_index = 0;
	for(int i = 0; path[i]; i++){ if(path[i] == '/'){ slash_index = i; } }

	int temp = 0;

	if(slash_index){//This is a very layered path
		path[slash_index] = 0;
		_push_str(path, path_buffer, &path_size);
		_push_str("/", path_buffer, &path_size);
		_push_str(path + slash_index + 1, path, &temp);
	}

	_pack_recursive_directory(package, file, path, path_buffer, path_size);
}

void _push_str(char* src, char* dest, int* dest_siz){
	for(int i = 0; src[i]; i++){
		dest[dest_siz[0]++] = src[i];
	}
	dest[dest_siz[0]] = 0;
}

static void _pack_recursive_directory(inpact_package_state* package, void* file, char* path, char* path_buffer, int path_size){
	_push_str(path, path_buffer, &path_size);

	DIR* dir = opendir(path_buffer);
	if(!dir){//This may be a file...
		_pack_recursive_file(package, file, path, path_buffer);
		return;
	}

	int count = 0;
	for(struct dirent* entry = readdir(dir); entry; entry = readdir(dir)){
		if(entry->d_name[0] == '.'){ continue; }//im not interested in "." nor ".."
		count++;
	}
	inpact_push_dir(package, file, path, count);

	_push_str("/", path_buffer, &path_size);

	rewinddir(dir);

	for(struct dirent* entry = readdir(dir); entry; entry = readdir(dir)){
		if(entry->d_name[0] == '.'){ continue; }//im not interested in "." nor ".."
		_pack_recursive_directory(package, file, entry->d_name, path_buffer, path_size);
	}

	closedir(dir);
}

static void _pack_recursive_file(inpact_package_state* package, void* file, char* name, char* path){
	char* extention = name;
	while(extention[0]){
		if(extention[0] == '.'){ (extention++)[0] = 0; break; }
		extention++;
	}

	FILE* fptr = fopen(path, "rb");
	if(!fptr){ return; }

	inpact_push_file(package, file, name, extention, fptr);

	fclose(fptr);
}

//---------------------------------------------------------------------------------------------------

void inpact_surf_file_point(inpact_package_state* package, FILE* file, int entry_index){
	package->cursor.index = entry_index;
	package->cursor.label_address = package->header.entry_end + (entry_index * sizeof(package_entry_label));

	package_entry_label label = {0};

	fseek(file, package->cursor.label_address, SEEK_SET);
	fread(&label, sizeof(package_entry_label), 1, file);

	package->cursor.header_address = label.location;

	fseek(file, package->cursor.header_address, SEEK_SET);
	fread(&package->cursor.header, sizeof(package_entry_header), 1, file);
}

int _file_find_name(FILE* file, int address, int end, uint64_t name){
	fseek(file, address, SEEK_SET);

	while(address < end){
		package_entry_label label = {0};

		fseek(file, address, SEEK_SET);//Seems unnecessary, but without it, the system loses itself
		fread(&label, sizeof(package_entry_label), 1, file);

		if(label.name == name){ return address; }

		address += sizeof(package_entry_label);
	}

	return 0;
}

int _file_find_type(FILE* file, int address, int end, uint64_t type){
	fseek(file, address, SEEK_SET);

	while(address < end){
		package_entry_label label = {0};

		fread(&label, sizeof(package_entry_label), 1, file);

		if(label.type == type){ return address; }

		address += sizeof(package_entry_label);
	}

	return 0;
}

int inpact_surf_file_find(inpact_package_state* package, FILE* file, char* name, char* type){
	int address = package->cursor.label_address;
	int end = package->header.entry_end + (package->header.entry_count * sizeof(package_entry_label));

	uint64_t intname = inpact_build_string_number(name, 8);
	uint32_t inttype = type ? inpact_build_string_number(type, 4) : 0;

	do{
		address = _file_find_name(file, address, end, intname);

		if(!address){ return 0; }
		if(!inttype){ break; }

		int type_found = _file_find_type(file, address, end, inttype);
		if(!type_found){ return 0; }
		if(type_found == address){ break; }

		address = type_found;
	}while(1);

	int index = (address - package->header.entry_end) / sizeof(package_entry_label);
	inpact_surf_file_point(package, file, index);

	return 1;
}

void inpact_surf_file_map(inpact_package_state* package, FILE* file, package_entry_header* map){
	int size = package->header.entry_count;
	for(int i = 0; i < size; i++){
		inpact_surf_file_point(package, file, i);
		map[i] = package->cursor.header;
	}
}

void inpact_surf_file_read(inpact_package_state* package, FILE* file, void* buffer){
	off_t address = package->cursor.header_address + sizeof(package_entry_header);

	fseek(file, address, SEEK_SET);

	if(!package->cursor.header.flags){//It is not compressed
		fread(buffer, 1, package->cursor.header.original_size, file);
		return;
	}

	uint8_t data[package->cursor.header.size];
	fread(data, 1, package->cursor.header.size, file);
	inflate_decompress(data, buffer);
}
