#include "demo.h"

#include <limits.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <errno.h>
#include "ints.h"

int demo_write_header(DemoHeader* header, FILE* file) {
    if (file == NULL || header == NULL) return -1;

    // TODO: Write custom wrapper around fwrite and fread to handle distinguishing between end-of-file reached and error)
    if (fwrite(header, sizeof(*header), 1, file) != 1) {
        printf("Failed to write demo header \n");
    }

    return 0;
}

DemoHeader* demo_read_header(FILE* file) {
    if (file == NULL) return NULL;

    DemoHeader* demo_header = (DemoHeader*)malloc(sizeof(DemoHeader));
    if (demo_header == NULL) {
        printf("Failed to allocate memory for demo header \n");
        return NULL;
    }

    // TODO: Write custom wrapper around fwrite and fread to handle distinguishing between end-of-file reached and error)
    if (fread(demo_header, sizeof(*demo_header), 1, file) != 1) {
        printf("Failed to read demo file header \n");
        return NULL;
    }

    return demo_header;
}

// TODO: Move to separate module
FILE* create_demo_file(void) {
    // Get application preferences path
    const char * pref_path = SDL_GetPrefPath("three_guys", "taco_quest");
    if (pref_path == NULL) {
        fprintf(stderr, "Failed to get pref path");
        return NULL;
    }

    // TODO: Use snprintf
    // Create demos directory
    char path[PATH_MAX] = {0};
    strcat(path, pref_path);
    SDL_free((void *)pref_path); // idaho lives
    strcat(path, "demos/");
    bool dir_created = SDL_CreateDirectory(path);
    if (!dir_created) {
        fprintf(stderr, "Failed to create demos directory: %s\n", SDL_GetError());
        return NULL;
    }

    // Append demo filename
    char filename[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(
        filename,
        sizeof(filename),
        "demo_%y_%m_%d_%H%M%S.sgd",
        tm_info
    );
    strcat(path, filename);

    printf("Creating demo file: %s\n", path);

    // Open demos file
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Failed to create demo file: %s\n", strerror(errno));
        return NULL;
    }

    return file;
}

// TODO
// bool write_demo_file(FILE *file) {

// }