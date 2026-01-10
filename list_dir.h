#ifndef list_dir_h
#define list_dir_h

#include "ints.h"

#if defined(PLATFORM_WINDOWS)
#define WINDOWS_MAP_SUFFIX_MATCHER "*.temap"
#endif

#include <stdbool.h>

typedef struct {
    char** file_names;
    S32 file_count;
} ListDir;

bool list_dir_insert(ListDir* list_dir, const char* file_name);
void list_dir_destroy(ListDir* list_dir);

#if defined(PLATFORM_WINDOWS)
ListDir list_files_in_dir(const char* directory);
#else
ListDir list_files_in_dir(const char* directory, const char* match);
#endif

#endif
