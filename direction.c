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

void adjacent_full_direction_cell(FullDirection direction, S32* x, S32* y) {
    switch(direction) {
    default:
        break;
    case FULL_DIRECTION_NORTH:
        (*y)--;
        break;
    case FULL_DIRECTION_NORTH_EAST:
        (*y)--;
        (*x)++;
        break;
    case FULL_DIRECTION_EAST:
        (*x)++;
        break;
    case FULL_DIRECTION_SOUTH_EAST:
        (*x)++;
        (*y)++;
        break;
    case FULL_DIRECTION_SOUTH:
        (*y)++;
        break;
    case FULL_DIRECTION_SOUTH_WEST:
        (*y)++;
        (*x)--;
        break;
    case FULL_DIRECTION_WEST:
        (*x)--;
        break;
    case FULL_DIRECTION_NORTH_WEST:
        (*x)--;
        (*y)--;
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

DirectionRelationship full_direction_relationship(FullDirection a, FullDirection b) {
    if (a == b) {
        return DIRECTION_RELATIONSHIP_SAME;
    }

    if (opposite_direction(a) == b) {
        return DIRECTION_RELATIONSHIP_OPPOSITE;
    }

    if (rotate_clockwise_full_direction(a) == b ||
        rotate_clockwise_full_direction_for(a, 2) == b ||
        rotate_clockwise_full_direction_for(a, 3) == b) {
        return DIRECTION_RELATIONSHIP_CLOCKWISE;
    }

    if (rotate_counter_clockwise_full_direction(a) == b ||
        rotate_counter_clockwise_full_direction_for(a, 2) == b ||
        rotate_counter_clockwise_full_direction_for(a, 3) == b) {
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

FullDirection opposite_full_direction(FullDirection direction) {
    switch(direction) {
    case FULL_DIRECTION_NORTH:
        return FULL_DIRECTION_SOUTH;
    case FULL_DIRECTION_NORTH_EAST:
        return FULL_DIRECTION_SOUTH_WEST;
    case FULL_DIRECTION_EAST:
        return FULL_DIRECTION_WEST;
    case FULL_DIRECTION_SOUTH_EAST:
        return FULL_DIRECTION_NORTH_WEST;
    case FULL_DIRECTION_SOUTH:
        return FULL_DIRECTION_NORTH;
    case FULL_DIRECTION_SOUTH_WEST:
        return FULL_DIRECTION_NORTH_EAST;
    case FULL_DIRECTION_WEST:
        return FULL_DIRECTION_EAST;
    case FULL_DIRECTION_NORTH_WEST:
        return FULL_DIRECTION_SOUTH_EAST;
    default:
        break;
    }

    return FULL_DIRECTION_UNKNOWN;
}

FullDirection rotate_clockwise_full_direction(FullDirection direction) {
    if (direction < 0 || direction >= FULL_DIRECTION_COUNT) {
        return FULL_DIRECTION_UNKNOWN;
    }

    return (FullDirection)((direction + 1) % FULL_DIRECTION_COUNT);
}

FullDirection rotate_clockwise_full_direction_for(FullDirection direction, S32 count) {
    for (S32 i = 0; i < count; i++) {
        direction = rotate_clockwise_full_direction(direction);
    }
    return direction;
}

FullDirection rotate_counter_clockwise_full_direction(FullDirection direction) {
    if (direction < 0 || direction >= FULL_DIRECTION_COUNT) {
        return FULL_DIRECTION_UNKNOWN;
    }

    S32 result = direction - 1;
    if (result < 0) {
        result = FULL_DIRECTION_NORTH_WEST;
    }
    return (FullDirection)(result);
}

FullDirection rotate_counter_clockwise_full_direction_for(FullDirection direction, S32 count) {
    for (S32 i = 0; i < count; i++) {
        direction = rotate_counter_clockwise_full_direction(direction);
    }
    return direction;
}

FullDirection full_direction_in_between(FullDirection a, FullDirection b) {
    S32 clockwise_check = a + 2;
    if (clockwise_check == b) {
        return (FullDirection)(a + 1);
    }

    S32 counter_clockwise_check = a - 2;
    if (counter_clockwise_check < 0) {
        counter_clockwise_check += FULL_DIRECTION_COUNT;
    }

    if (counter_clockwise_check == b) {
        S32 result = a - 1;
        if (result < 0) {
            return FULL_DIRECTION_NORTH_WEST;
        }
        return (FullDirection)(result);
    }

    return FULL_DIRECTION_UNKNOWN;
}

bool directions_are_perpendicular(FullDirection a, FullDirection b) {
    S32 clockwise_check = a + 2;
    if (clockwise_check == b) {
        return true;
    }

    S32 counter_clockwise_check = a - 2;
    if (counter_clockwise_check < 0) {
        counter_clockwise_check += FULL_DIRECTION_COUNT;
    }

    if (counter_clockwise_check == b) {
        return true;
    }

    return false;
}
