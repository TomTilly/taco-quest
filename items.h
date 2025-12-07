//
//  level.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef items_h
#define items_h

#include <stdlib.h>
#include <stdbool.h>

#include "ints.h"

#define LEVEL_WIDTH 24
#define LEVEL_HEIGHT 24

typedef S8 ItemType;
enum {
    ITEM_TYPE_INVALID = -1,
    ITEM_TYPE_EMPTY,
    ITEM_TYPE_TACO
};

typedef struct {
    ItemType* cells;
    S32 width;
    S32 height;
} Items;

bool items_init(Items* items, S32 width, S32 height);
void items_destroy(Items* items);
S32 items_get_index(Items* items, S32 x, S32 y);
bool items_set_cell(Items* items, S32 x, S32 y, ItemType value);
ItemType items_get_cell(Items* items, S32 x, S32 y);
size_t items_serialize(const Items* items, void * buffer, size_t buffer_size);
size_t items_deserialize(void * buffer, size_t size, Items *out);

#endif /* items_h */
