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

DirectionRelationship direction_relationship(Direction a, Direction b) {
    if (a == b) {
        return DIRECTION_RELATIONSHIP_SAME;
    }

    if (opposite_direction(a) == b) {
        return DIRECTION_RELATIONSHIP_OPPOSITE;
    }

    if (rotate_clockwise(a) == b) {
        return DIRECTION_RELATIONSHIP_CLOCKWISE;
    }

    if (rotate_counter_clockwise(a) == b) {
        return DIRECTION_RELATIONSHIP_COUNTER_CLOCKWISE;
    }

    return DIRECTION_RELATIONSHIP_UNKNOWN;
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

Direction rotate_clockwise(Direction direction) {
    return (direction + 1) % DIRECTION_COUNT;
}

Direction rotate_counter_clockwise(Direction direction) {
    S32 result = direction - 1;
    if (result < 0) {
        result = DIRECTION_WEST;
    }
    return (Direction)(result);
}

bool directions_are_perpendicular(Direction a, Direction b) {
    if (rotate_clockwise(a) == b) {
        return true;
    }

    if (rotate_counter_clockwise(a) == b) {
        return true;
    }

    return false;
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

const char* direction_to_string(Direction direction) {
    switch (direction) {
    case DIRECTION_NONE:
        return "NO_DIR";
    case DIRECTION_NORTH:
        return "SOUTH";
    case DIRECTION_EAST:
        return "WEST";
    case DIRECTION_SOUTH:
        return "NORTH";
    case DIRECTION_WEST:
        return "EAST";
    }
    return "UNKNOWN_DIR";
}
