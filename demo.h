#ifndef demo_h
#define demo_h

#include <stdio.h>
#include <stdint.h>
#include "ints.h"

typedef struct {
    U32 version;
    char map_name[64];
} DemoHeader;

FILE* create_demo_file(void);

int demo_write_header(DemoHeader* header, FILE* file);

// Caller should free returned memory
DemoHeader* demo_read_header(FILE* file);

#endif /* demo_h */