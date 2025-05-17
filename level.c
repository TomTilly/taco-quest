//
//  level.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "level.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

bool level_init(Level* level, int32_t width, int32_t height) {
    size_t size = width * height * sizeof(level->cells[0]);
    level->cells = malloc(size);

    if (level->cells == NULL) {
        return false;
    }

    memset(level->cells, 0, size);

    level->width = width;
    level->height = height;

    return true;
}

void level_destroy(Level* level) {
    free(level->cells);
    memset(level, 0, sizeof(*level));
}

int32_t level_get_cell_index(Level* level, S32 x, S32 y) {
    return y * level->width + x;
}

bool level_set_cell(Level* level, S32 x, S32 y, CellType value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->height) {
        return false;
    }

    int32_t index = level_get_cell_index(level, x, y);
    level->cells[index] = value;
    return true;
}

CellType level_get_cell(Level* level, S32 x, S32 y) {
    if (x < 0 || x >= level->width) {
        return CELL_TYPE_INVALID;
    }

    if (y < 0 || y >= level->width) {
        return CELL_TYPE_INVALID;
    }

    int32_t index = level_get_cell_index(level, x, y);
    return level->cells[index];
}

size_t level_serialize(const Level* level, void * buffer, size_t buffer_size) {
    size_t cells_size = level->width * level->height * sizeof(*level->cells);
    size_t total_size = sizeof(level->width) + sizeof(level->height) + cells_size;

    assert(total_size <= buffer_size && "buffer too small!");

    U8 * byte_buffer = buffer;

    memcpy(byte_buffer, &level->width, sizeof(level->width));
    byte_buffer += sizeof(level->width);

    memcpy(byte_buffer, &level->height, sizeof(level->height));
    byte_buffer += sizeof(level->height);

    memcpy(byte_buffer, level->cells, cells_size);

    return total_size;
}

size_t level_deserialize(void * buffer, size_t size, Level * out) {
    U8 * ptr = buffer;

    assert(size >= sizeof(out->width) * sizeof(out->height)
           && "buffer size too smol for level width and height!");

    S32 width = *(S32 *)ptr;
    ptr += sizeof(out->width);
    size -= sizeof(out->width);

    S32 height = *(S32 *)ptr;
    ptr += sizeof(out->height);
    size -= sizeof(out->height);

    if ( out->width != width || out->height != height ) {
        // TODO: a proper level realloc func that covers both these cases.
        if ( width * height > out->width * out->height ) {
            level_destroy(out);
            bool success = level_init(out, width, height);
            assert(success && "level_init failed!");
        } else {
            out->width = width;
            out->height = height;
        }
    }

    size_t cells_size = width * height * sizeof(*out->cells);
    assert(size >= cells_size && "buffer size too smol for level cells");

    memcpy(out->cells, ptr, cells_size);
    ptr += cells_size;
    size -= cells_size;

    return ptr - (U8 *)buffer;
}
