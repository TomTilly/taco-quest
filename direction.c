//
//  direction.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "direction.h"

void adjacent_cell(Direction direction, S32* x, S32* y) {
    switch(direction) {
    case DIRECTION_NONE:
        break;
    case DIRECTION_NORTH:
        (*y)--;
        break;
    case DIRECTION_EAST:
        (*x)++;
        break;
    case DIRECTION_SOUTH:
        (*y)++;
        break;
    case DIRECTION_WEST:
        (*x)--;
        break;
    }
}

Direction opposite_direction(Direction direction) {
    switch(direction) {
    case DIRECTION_NONE:
        break;
    case DIRECTION_NORTH:
        return DIRECTION_SOUTH;
    case DIRECTION_EAST:
        return DIRECTION_WEST;
    case DIRECTION_SOUTH:
        return DIRECTION_NORTH;
    case DIRECTION_WEST:
        return DIRECTION_EAST;
    }

    return DIRECTION_NONE;
}

Direction direction_between_cells(S32 a_x, S32 a_y, S32 b_x, S32 b_y) {
    if (a_x == b_x) {
        if (a_y == (b_y - 1)) {
            return DIRECTION_NORTH; 
        }
        if (a_y == (b_y + 1)) {
            return DIRECTION_SOUTH; 
        }
    }

    if (a_y == b_y) {
        if (a_x == (b_x - 1)) {
            return DIRECTION_WEST; 
        }
        if (a_x == (b_x + 1)) {
            return DIRECTION_EAST; 
        }
    }

    return DIRECTION_NONE;
}
