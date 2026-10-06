#include "inpact.h"
#include "inflate.h"
#include <string.h>

static void _mread(int size, void* dest, int dest_off, void* src, int src_off){
	memcpy(((char*)dest) + dest_off, ((char*)src) + src_off, size);
}

static void* _target_address(void* ptr, int addr){
	return ((uint8_t*)ptr) + addr;
}

//---------------------------------------------------------------------------------------------------
//----- MEMORY VERSION -----
//---------------------------------------------------------------------------------------------------

void inpact_package_read_memory(inpact_package_state* package, void* file){
	inpact_package_init(package);

	package_header* header = _target_address(file, 0);
	package->header = header[0];
}

//---------------------------------------------------------------------------------------------------

void inpact_surf_memory_point(inpact_package_state* package, void* file, int entry_index){
	package->cursor.index = entry_index;
	package->cursor.label_address = package->header.entry_end + (entry_index * sizeof(package_entry_label));

	package_entry_label* label = _target_address(file, package->cursor.label_address);

	package->cursor.header_address = label->location;

	package_entry_header* header = _target_address(file, package->cursor.header_address);
	package->cursor.header = header[0];
}

int inpact_surf_memory_find(inpact_package_state* package, void* file, char* name, char* type){
	int address = package->cursor.label_address;
	int end = package->header.entry_end + (package->header.entry_count * sizeof(package_entry_label));

	uint64_t intname = inpact_build_string_number(name, 8);
	uint32_t inttype = type ? inpact_build_string_number(type, 4) : 0;
	do{
		for(package_entry_label* label = _target_address(file, address); address != end; label++, address += sizeof(package_entry_label)){
			if(label[0].name == intname){ break; }
		}

		if(address == end){ return 0; }
		if(!inttype){ break; }

		int type_found = address;
		for(package_entry_label* label = _target_address(file, type_found); type_found != end; label++, type_found += sizeof(package_entry_label)){
			if(label[0].type == inttype){ break; }
		}

		if(type_found == end){ return 0; }
		if(type_found == address){ break; }

		address = type_found;
	}while(1);

	int index = (address - package->header.entry_end) / sizeof(package_entry_label);
	inpact_surf_memory_point(package, file, index);

	return 1;
}

void inpact_surf_memory_map(inpact_package_state* package, FILE* file, package_entry_header* map){
	int size = package->header.entry_count;
	for(int i = 0; i < size; i++){
		inpact_surf_memory_point(package, file, i);
		map[i] = package->cursor.header;
	}
}

void inpact_surf_memory_read(inpact_package_state* package, void* file, void* buffer){
	int address = package->cursor.header_address + sizeof(package_entry_header);

	void* data_ptr = _target_address(file, address);

	if(!package->cursor.header.flags){//It is not compressed
		_mread(package->cursor.header.original_size, buffer, 0, data_ptr, 0);
		return;
	}

	uint8_t data[package->cursor.header.size];
	_mread(package->cursor.header.size, data, 0, data_ptr, 0);
	inflate_decompress(data, buffer);
}
