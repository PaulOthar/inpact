#include "inpact.h"

void inpact_package_init(inpact_package_state* package){
	inpact_package_state zeroed_state = {0};
	package[0] = zeroed_state;

	package->header.entry_end = sizeof(package_header);

	package->cursor.index = -1;
	package->cursor.header_address = -1;
	package->cursor.label_address = -1;
}

uint64_t inpact_build_string_number(char* str, int size){
	uint64_t result = 0;
	char* resptr = (char*)&result;
	for(int i = 0; i < size && str[i]; i++){
		resptr[i] = str[i];
	}
	return result;
}
