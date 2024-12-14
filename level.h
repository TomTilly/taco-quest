//
//  level.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef level_h
#define level_h

#include <stdlib.h>
#include <stdbool.h>

#include "ints.h"

typedef enum {
    CELL_TYPE_INVALID = -1,
    CELL_TYPE_EMPTY,
    CELL_TYPE_WALL,
    CELL_TYPE_TACO
} CellType;

typedef struct {
    CellType* cells;
    S32 width;
    S32 height;
} Level;

bool level_init(Level* level, S32 width, S32 height);
void level_destroy(Level* level);
S32 level_get_cell_index(Level* level, S32 x, S32 y);
bool level_set_cell(Level* level, S32 x, S32 y, CellType value);
CellType level_get_cell(Level* level, S32 x, S32 y);
size_t level_serialize(const Level* level, void * buffer, size_t buffer_size);
size_t level_deserialize(void * buffer, size_t size, Level * out);

#endif /* level_h */
