#ifndef FS_MEMORY
#define FS_MEMORY

#include <stdint.h>

typedef struct {
	uint8_t* data;
	unsigned int size;
	int valid;
} HeapData;



int mem_alloc(HeapData*, int);
int mem_realloc(HeapData*, int);
int mem_free(HeapData);
int mem_write(HeapData*, int, uint8_t);
int mem_write_section(HeapData*, int, HeapData);

// This function is an exception - the error checking is 
// passed as an argument. This is so that the returned value
// is stack allocated within the scope of the calling function
uint8_t mem_read(HeapData, int, int*);

int mem_dump(HeapData, char*);
int mem_dump_section(HeapData, char*, int, int); 
int mem_write_binary(void*, int, char*);

int mem_check_access(HeapData, int );
void malloc_failed();
#endif
