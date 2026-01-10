//
//  items.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "items.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

bool items_init(Items* items, int32_t width, int32_t height) {
    size_t size = width * height * sizeof(items->cells[0]);
    items->cells = malloc(size);

    if (items->cells == NULL) {
        return false;
    }

    memset(items->cells, 0, size);

    items->width = width;
    items->height = height;

    return true;
}

void items_destroy(Items* items) {
    if (items->cells != NULL) {
        free(items->cells);
        memset(items, 0, sizeof(*items));
    }
}

int32_t items_get_cell_index(Items* items, S32 x, S32 y) {
    return y * items->width + x;
}

bool items_set_cell(Items* items, S32 x, S32 y, ItemType value) {
    if (x < 0 || x >= items->width) {
        return false;
    }

    if (y < 0 || y >= items->height) {
        return false;
    }

    int32_t index = items_get_cell_index(items, x, y);
    items->cells[index] = value;
    return true;
}

ItemType items_get_cell(Items* items, S32 x, S32 y) {
    if (x < 0 || x >= items->width) {
        return ITEM_TYPE_INVALID;
    }

    if (y < 0 || y >= items->height) {
        return ITEM_TYPE_INVALID;
    }

    int32_t index = items_get_cell_index(items, x, y);
    return items->cells[index];
}

size_t items_serialize(const Items* items, void * buffer, size_t buffer_size) {
    size_t cells_size = items->width * items->height * sizeof(*items->cells);
    size_t total_size = sizeof(items->width) + sizeof(items->height) + cells_size;

    assert(total_size <= buffer_size && "buffer too small!");

    U8 * byte_buffer = buffer;

    memcpy(byte_buffer, &items->width, sizeof(items->width));
    byte_buffer += sizeof(items->width);

    memcpy(byte_buffer, &items->height, sizeof(items->height));
    byte_buffer += sizeof(items->height);

    memcpy(byte_buffer, items->cells, cells_size);

    return total_size;
}

size_t items_deserialize(void * buffer, size_t size, Items * out) {
    assert(out != NULL && "level_deserialize out param is NULL!");

    U8 * ptr = buffer;

    assert(size >= sizeof(out->width) * sizeof(out->height)
           && "buffer size too smol for items width and height!");

    S32 width = *(S32 *)ptr;
    ptr += sizeof(out->width);
    size -= sizeof(out->width);

    S32 height = *(S32 *)ptr;
    ptr += sizeof(out->height);
    size -= sizeof(out->height);

    if ( out->width != width || out->height != height ) {
        // TODO: a proper items realloc func that covers both these cases.
        if ( width * height > out->width * out->height ) {
            items_destroy(out);
            bool success = items_init(out, width, height);
            assert(success && "level_init failed!");
        } else {
            out->width = width;
            out->height = height;
        }
    }

    size_t cells_size = width * height * sizeof(*out->cells);
    assert(size >= cells_size && "buffer size too smol for items cells");

    memcpy(out->cells, ptr, cells_size);
    ptr += cells_size;
    size -= cells_size;

    return ptr - (U8 *)buffer;
}
