//
//  level.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "level.h"
#include <stdlib.h>
#include <string.h>

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

int32_t level_get_cell_index(Level* level, int32_t x, int32_t y) {
    return y * level->width + x;
}

bool level_set_cell(Level* level, int32_t x, int32_t y, CellType value) {
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

CellType level_get_cell(Level* level, int32_t x, int32_t y) {
    if (x < 0 || x >= level->width) {
        return CELL_TYPE_INVALID;
    }

    if (y < 0 || y >= level->width) {
        return CELL_TYPE_INVALID;
    }

    int32_t index = level_get_cell_index(level, x, y);
    return level->cells[index];
}
