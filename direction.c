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

