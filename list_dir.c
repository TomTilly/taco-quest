#include "list_dir.h"

#if defined(PLATFORM_WINDOWS)
    #include <fileapi.h>
    #include <handleapi.h>
#define WINDOWS_MAP_SUFFIX_MATCHER "*.temap"
#endif

#include <assert.h>
#include <stdlib.h>

bool list_dir_insert(ListDir* list_dir, const char* file_name) {
    S32 new_count = list_dir->file_count + 1;
    char** reallocation = realloc(list_dir->file_names,
                                  new_count * sizeof(*list_dir->file_names));
    if (reallocation == NULL) {
        return false;
    }

    list_dir->file_names = reallocation;
    list_dir->file_names[list_dir->file_count] = _strdup(file_name);
    list_dir->file_count = new_count;
    return true;
}

void list_dir_destroy(ListDir* list_dir) {
    for (S32 i = 0; i < list_dir->file_count; i++) {
        free(list_dir->file_names[i]);
    }
    free(list_dir->file_names);
    memset(list_dir, 0, sizeof(*list_dir));
}

#if defined(PLATFORM_WINDOWS)
ListDir list_files_in_dir(const char* directory) {
     ListDir result = {0};
     WIN32_FIND_DATA find_files_result;
     memset(&find_files_result, 0, sizeof(find_files_result));

     HANDLE find_files_handle = FindFirstFileA(directory,
                                               &find_files_result);
     if (find_files_handle == INVALID_HANDLE_VALUE) {
          return result;
     }

     do{
         if ((find_files_result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
             assert(list_dir_insert(&result, find_files_result.cFileName));
         }
     }while(FindNextFileA(find_files_handle, &find_files_result));

     FindClose(find_files_handle);
     return result;
}

#else

ListDir list_files_in_dir(const char* directory, const char* match) {
     ListDir result = {0};
     DIR* os_dir = opendir(directory);
     if(!os_dir){
          return result;
     }

     struct dirent* node = NULL;
     while((node = readdir(os_dir)) != NULL){
         if (node->d_type != DT_DIR &&
             strstr(node->d_name, match) != NULL) {
             assert(list_dir_insert(&result, node->d_name));
         }
     }

     closedir(os_dir);

     return result;
}
#endif
