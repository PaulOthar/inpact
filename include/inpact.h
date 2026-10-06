#ifndef INPACT
#define INPACT

#include <stdint.h>
#include <stdio.h>

typedef union inpact_metadata{
	uint64_t raw;

	struct /*metadata_image*/{
		uint16_t width;
		uint16_t heigth;
		uint16_t reserved;
		uint8_t bits_per_pixel;
		uint8_t palette_entries;
	}image;

	struct /*metadata_dir*/{
		uint64_t file_count;
	}dir;
}inpact_metadata;

typedef struct /*package_header*//*16B*/{
	uint64_t package_name;

	uint32_t entry_count;
	uint32_t entry_end;
}package_header;

typedef struct /*package_entry_header*//*32B*/{
	uint64_t name;
	uint32_t type;

	uint32_t flags;

	uint32_t size;
	uint32_t original_size;

	inpact_metadata metadata;
}package_entry_header;

typedef struct /*package_entry_label*//*16B*/{
	uint64_t name;
	uint32_t type;
	uint32_t location;
}package_entry_label;

//---------------------------------------------------------------------------------------------------

typedef struct/*inpact_package_cursor*/{
	uint32_t index;
	uint32_t header_address;
	uint32_t label_address;

	package_entry_header header;
}inpact_package_cursor;

typedef struct /*inpact_package_state*/{
	package_header header;
	inpact_package_cursor cursor;
}inpact_package_state;

void inpact_package_init(inpact_package_state* package);

//---------------------------------------------------------------------------------------------------

void inpact_unpack_file(FILE* file, char* path);
void inpact_package_read(inpact_package_state* package, FILE* file);
void inpact_package_write_update(inpact_package_state* package, FILE* file);

void inpact_push_data(inpact_package_state* package, FILE* file, char* name, char* type, inpact_metadata metadata, void* data, int size);
void inpact_push_file(inpact_package_state* package, FILE* file, char* name, char* type, FILE* entry);
void inpact_push_dir(inpact_package_state* package, FILE* file, char* name, int entry_count);
void inpact_push_recursive(inpact_package_state* package, FILE* file, char* path);

//---------------------------------------------------------------------------------------------------

void inpact_unpack_memory(void* file, char* path);
void inpact_package_read_memory(inpact_package_state* package, void* file);

//---------------------------------------------------------------------------------------------------

void inpact_surf_file_point(inpact_package_state* package, FILE* file, int entry_index);
int inpact_surf_file_find(inpact_package_state* package, FILE* file, char* name, char* type);
void inpact_surf_file_map(inpact_package_state* package, FILE* file, package_entry_header* map);
void inpact_surf_file_read(inpact_package_state* package, FILE* file, void* buffer);

void inpact_surf_memory_point(inpact_package_state* package, void* file, int entry_index);
int inpact_surf_memory_find(inpact_package_state* package, void* file, char* name, char* type);
void inpact_surf_memory_map(inpact_package_state* package, FILE* file, package_entry_header* map);
void inpact_surf_memory_read(inpact_package_state* package, void* file, void* buffer);

int inpact_surf_find_mapped(package_entry_header* map, char* name, char* type);

//---------------------------------------------------------------------------------------------------

uint64_t inpact_build_string_number(char* str, int size);

#endif
