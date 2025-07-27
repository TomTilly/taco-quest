//
//  direction.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef direction_h
#define direction_h

#include "ints.h"

typedef enum {
    DIRECTION_NORTH,
    DIRECTION_EAST,
    DIRECTION_SOUTH,
    DIRECTION_WEST,

    DIRECTION_NONE,
} Direction;

void adjacent_cell(Direction direction, S32* x, S32* y);
Direction opposite_direction(Direction direction);
// A to B
Direction direction_between_cells(S32 a_x, S32 a_y, S32 b_x, S32 b_y);

#endif /* direction_h */
