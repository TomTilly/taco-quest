//
//  direction.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef direction_h
#define direction_h

#include "ints.h"

#include <stdbool.h>

typedef enum {
    DIRECTION_NORTH,
    DIRECTION_EAST,
    DIRECTION_SOUTH,
    DIRECTION_WEST,
    DIRECTION_COUNT,
    DIRECTION_NONE,
} Direction;

typedef enum {
    DIRECTION_RELATIONSHIP_SAME,
    DIRECTION_RELATIONSHIP_CLOCKWISE,
    DIRECTION_RELATIONSHIP_COUNTER_CLOCKWISE,
    DIRECTION_RELATIONSHIP_OPPOSITE,
    DIRECTION_RELATIONSHIP_UNKNOWN,
} DirectionRelationship;

void adjacent_cell(Direction direction, S32* x, S32* y);

// TODO: Document what direction_relationship() means, and possibly update the name.
DirectionRelationship direction_relationship(Direction a, Direction b);
Direction opposite_direction(Direction direction);
Direction rotate_clockwise(Direction direction);
Direction rotate_counter_clockwise(Direction direction);
bool directions_are_perpendicular(Direction a, Direction b);

#endif /* direction_h */
