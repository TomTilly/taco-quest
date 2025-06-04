//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"

#include <assert.h>
#include <stdio.h> // TODO: remove

#define MAX_SEGMENT_PATH 5
#define INNER_CORNER_CONSTRICTED_SEGMENTS 1
#define OUTPUT_CORNER_CONSTRICTED_SEGMENTS 5

typedef struct {
    S16 snake_index;
    S16 segment_index;
} SnakeCollision;

typedef struct {
    SnakeSegmentShape shape;
    S32 x;
    S32 y;
} SnakeConstrictSegmentInfo;

void _snake_chomp_segment(Game* game, SnakeCollision* snake_collision) {
    Snake* snake = game->snakes + snake_collision->snake_index;
    SnakeSegment* chomped_segment = snake->segments + snake_collision->segment_index;
    chomped_segment->health--;
    if (chomped_segment->health <= 0) {
        for (S32 e = snake_collision->segment_index; e < snake->length; e++) {
            SnakeSegment* segment = snake->segments + e;
            level_set_cell(&game->level, segment->x, segment->y, CELL_TYPE_TACO);
        }
        snake->length = snake_collision->segment_index;
    }
}

void _snake_chomp(Snake* snake, Game* game) {
    S32 new_snake_x = (S32)(snake->segments[0].x);
    S32 new_snake_y = (S32)(snake->segments[0].y);

    adjacent_cell(snake->direction,
                  &new_snake_x,
                  &new_snake_y);

    // Check if collided with other snake
    // TODO: This simplifies when we have an array of snakes.
    SnakeCollision snake_collision = {
        .snake_index = -1,
        .segment_index = -1
    };
    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 i = 0; i < game->snakes[s].length; i++) {
            SnakeSegment* segment = game->snakes[s].segments + i;
            if (segment->x == new_snake_x &&
                segment->y == new_snake_y) {
                snake_collision.snake_index = s;
                snake_collision.segment_index = i;
                break;
            }
        }
    }

    if (snake_collision.snake_index >= 0) {
        if (snake_collision.snake_index >= 0 && snake->chomp_cooldown <= 0) {
            _snake_chomp_segment(game, &snake_collision);
            snake->chomp_cooldown = SNAKE_CHOMP_COOLDOWN;
        }
    }
}

void _snake_move(Snake* snake, Game* game) {
    S32 new_snake_x = (S32)(snake->segments[0].x);
    S32 new_snake_y = (S32)(snake->segments[0].y);

    adjacent_cell(snake->direction,
                  &new_snake_x,
                  &new_snake_y);

    CellType cell_type = level_get_cell(&game->level, new_snake_x, new_snake_y);

    // TODO: Duplication with _snake_chomp() to figure out.
    SnakeCollision snake_collision = {
        .snake_index = -1,
        .segment_index = -1
    };
    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 i = 0; i < game->snakes[s].length; i++) {
            SnakeSegment* segment = game->snakes[s].segments + i;
            if (segment->x == new_snake_x &&
                segment->y == new_snake_y) {
                snake_collision.snake_index = s;
                snake_collision.segment_index = i;
                break;
            }
        }
    }

    if (snake_collision.snake_index >= 0) {
        return;
    }

    switch (cell_type) {
    case CELL_TYPE_EMPTY: {
        // Move the snake in the directon it is heading by moving all segments.
        for (int i = snake->length - 1; i >= 1; i--) {
            snake->segments[i].x = snake->segments[i - 1].x;
            snake->segments[i].y = snake->segments[i - 1].y;
        }
        snake->segments[0].x = (S16)(new_snake_x);
        snake->segments[0].y = (S16)(new_snake_y);
        break;
    }
    case CELL_TYPE_TACO: {
        // Grow the snake length by consuming the taco.
        level_set_cell(&game->level, new_snake_x, new_snake_y, CELL_TYPE_EMPTY);
        snake->length++;
        // Shift all segments one over toward the tail. The new segment is added where the head is.
        for ( int i = snake->length - 1; i >= 1; i-- ) {
            snake->segments[i] = snake->segments[i - 1];
        }
        // The new segment has max health.
        snake->segments[1].health = SNAKE_SEGMENT_MAX_HEALTH;
        // The head is moved into the position where the taco was.
        snake->segments[0].x = (S16)(new_snake_x);
        snake->segments[0].y = (S16)(new_snake_y);
        break;
    }
    case CELL_TYPE_WALL:
    default:
        break;
    }
}

bool game_init(Game* game, int32_t level_width, int32_t level_height) {
    if (!level_init(&game->level, level_width, level_height)) {
        return false;
    }

    int32_t snake_capacity = level_width * level_height;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (!snake_init(game->snakes + s, snake_capacity)) {
            return false;
        }
    }

    game->state = GAME_STATE_WAITING;

    return true;
}

bool _game_is_solid_at(Game* game, S32 x, S32 y, SnakeCollision* snake_collision) {
    CellType cell_type = level_get_cell(&game->level, x, y);
    if (cell_type != CELL_TYPE_EMPTY) {
        return true;
    }

    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            if (segment->x == x &&
                segment->y == y) {
                snake_collision->snake_index = s;
                snake_collision->segment_index = e;
                return true;
            }
        }
    }

    return false;
}

void _snake_turn(Game* game, SnakeAction snake_action, S32 snake_index) {
    Direction direction = DIRECTION_NONE;
    // TODO: snake_index check
    Snake* snake = game->snakes + snake_index;
    if (snake_action & SNAKE_ACTION_FACE_NORTH) {
        direction = DIRECTION_NORTH;
    }
    if (snake_action & SNAKE_ACTION_FACE_EAST) {
        direction = DIRECTION_EAST;
    }
    if (snake_action & SNAKE_ACTION_FACE_SOUTH) {
        direction = DIRECTION_SOUTH;
    }
    if (snake_action & SNAKE_ACTION_FACE_WEST) {
        direction = DIRECTION_WEST;
    }
    // TODO: Should native snake_turn() take the SnakeAction instead of having to convert to a direction here ?
    snake_turn(snake, direction);
}

void _snake_drag(Snake* snake, S32 segment_index, S32 new_x, S32 new_y) {
    // From the tail to the specified segment, move each segment closer to the head by replacing
    // the segment's position before it.
    for (S32 e = (snake->length - 1); e > segment_index; e--) {
        SnakeSegment* curr_segment = snake->segments + e;
        SnakeSegment* prev_segment = curr_segment - 1;
        curr_segment->x = prev_segment->x;
        curr_segment->y = prev_segment->y;
    }
    // Finally move the specified segment to the new location.
    snake->segments[segment_index].x = (S16)(new_x);
    snake->segments[segment_index].y = (S16)(new_y);
}

SnakeSegment _adjacent_snake_segment(SnakeSegment original_segment, Direction direction) {
    S32 x = original_segment.x;
    S32 y = original_segment.y;
    adjacent_cell(direction, &x, &y);
    SnakeSegment result = {
        .x = (S16)(x),
        .y = (S16)(y),
    };
    return result;
}

SnakeSegment _diagonal_snake_segment(SnakeSegment original_segment, Direction horizontal_direction, Direction vertical_direction) {
    S32 x = original_segment.x;
    S32 y = original_segment.y;
    adjacent_cell(horizontal_direction, &x, &y);
    adjacent_cell(vertical_direction, &x, &y);
    SnakeSegment result = {
        .x = (S16)(x),
        .y = (S16)(y),
    };
    return result;
}

void _segment_path_add(SnakeSegment new_segment, SnakeSegment* segment_path, S32* segment_path_count) {
    assert(*segment_path_count < MAX_SEGMENT_PATH);
    segment_path[*segment_path_count] = new_segment;
    (*segment_path_count)++;
}

bool _expand_outer_corner_if_open(Game* game,
                                  Snake* snake,
                                  S32 current_segment_index,
                                  SnakeSegment first_path[2],
                                  SnakeSegment second_path[2],
                                  Direction expand_horizontal_direction,
                                  Direction expand_vertical_direction) {
    SnakeCollision snake_collision = {0};
    SnakeSegment segment_path[MAX_SEGMENT_PATH];
    S32 segment_path_count = 0;

    // Check if the paths are clear first, if both are clear, we also expand southeast.
    bool first_path_is_clear =
        !_game_is_solid_at(game, first_path[0].x, first_path[0].y, &snake_collision) &&
        !_game_is_solid_at(game, first_path[1].x, first_path[1].y, &snake_collision);

    bool second_path_is_clear =
        !_game_is_solid_at(game, second_path[0].x, second_path[0].y, &snake_collision) &&
        !_game_is_solid_at(game, second_path[1].x, second_path[1].y, &snake_collision);

    // Add the paths to the segment path array.
    if (first_path_is_clear) {
        _segment_path_add(first_path[0], segment_path, &segment_path_count);
        _segment_path_add(first_path[1], segment_path, &segment_path_count);

        if (second_path_is_clear) {
            // Check if the expanded corner is available.
            SnakeSegment corner_segment = _diagonal_snake_segment(snake->segments[current_segment_index],
                                                                  expand_horizontal_direction,
                                                                  expand_vertical_direction);
            if (!_game_is_solid_at(game, corner_segment.x, corner_segment.y, &snake_collision)) {
                _segment_path_add(corner_segment, segment_path, &segment_path_count);
            } else {
                // Fall back to the original location.
                _segment_path_add(snake->segments[current_segment_index], segment_path, &segment_path_count);
            }
        } else {
            // Pass through the original location.
            _segment_path_add(snake->segments[current_segment_index], segment_path, &segment_path_count);
        }
    }

    if (second_path_is_clear) {
        if (!first_path_is_clear) {
            _segment_path_add(snake->segments[current_segment_index], segment_path, &segment_path_count);
        }
        _segment_path_add(second_path[0], segment_path, &segment_path_count);
        _segment_path_add(second_path[1], segment_path, &segment_path_count);
    }

    // Apply the segment path array.
    if (segment_path_count > 0) {
        // The current segment goes to the first location.
        snake->segments[current_segment_index].x = segment_path[0].x;
        snake->segments[current_segment_index].y = segment_path[0].y;

        // The rest of the segment path is followed by the next segment.
        for (S32 i = (segment_path_count - 1); i > 0; i--) {
            _snake_drag(snake, current_segment_index + 1, segment_path[i].x, segment_path[i].y);
        }
        return true;
    }

    return false;
}

void _snake_constrict(Snake* snake, Game* game) {
    // SnakeConstrictSegmentInfo* constrict_segment_info = malloc(sizeof(*constrict_segment_info) * snake->length);

    // First pass finds the shapes to operate on. This is so we don't operate on corners that we
    // create during this pass and only operate on the existing ones.
    S32 last_segment_index = snake->length - 1;

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    S32 constricted_elements = 0;
    SnakeCollision snake_collision = {0};
    for (S32 e = snake->constrict_state.index; e < last_segment_index; e++) {
        if (constricted_elements > 0) {
            snake->constrict_state.index = e + constricted_elements;
            break;
        }
        // If our constricting earlier in the loop has caused the shapes to change, do not operate on them.
        // bool operate_on_shape = false;
        SnakeSegmentShape current_segment_shape = snake_segment_shape(snake, e);
        Direction direction_to_head = snake_segment_direction_to_head(snake, e);

        switch(current_segment_shape) {
        case SNAKE_SEGMENT_SHAPE_NORTH_WEST_CORNER:
            if (snake->constrict_state.left) {
                if (direction_to_head == DIRECTION_NORTH) {
                    // p: the original path
                    // 1: new segment
                    // ......    .......
                    // ...p..    ...p...
                    // ...p.. -> ..1p...
                    // .ppp..    .pp....
                    // ......    .......
                    // ......    .......

                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_NORTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_WEST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_WEST) {
                    // p: the original path
                    // c: the corner being expanded
                    // 1: new path
                    // 2: second path
                    // 3: The corner to expand into if both first and second path is available. If
                    //    not, it falls back to the known center position.
                    // ......    .......
                    // ...p..    ...p...
                    // ...p.. -> ...p2..
                    // .ppp..    .pp32..
                    // ......    ..113..
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_SOUTH);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_EAST);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_EAST);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_NORTH);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_EAST, DIRECTION_SOUTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            } else {
                if (direction_to_head == DIRECTION_NORTH) {
                    // ......    .......
                    // ...p..    ...p...
                    // ...p.. -> ...p1..
                    // .ppc..    .pp31..
                    // ......    ..223..
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand east
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_EAST);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_SOUTH);

                    // Expand south
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_SOUTH);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_WEST);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_EAST, DIRECTION_SOUTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                } else if (direction_to_head == DIRECTION_WEST) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_NORTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_WEST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                }
            }
            break;
        case SNAKE_SEGMENT_SHAPE_NORTH_EAST_CORNER:
            if (snake->constrict_state.left) {
                if (direction_to_head == DIRECTION_NORTH) {
                    // ......    .......
                    // ..p...    ..p....
                    // ..p... -> .1p....
                    // ..cpp.    .13pp..
                    // ......    .322...
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_WEST);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_SOUTH);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_SOUTH);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_EAST);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_WEST, DIRECTION_SOUTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                } else if (direction_to_head == DIRECTION_EAST) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_NORTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_EAST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                }
            } else {
                if (direction_to_head == DIRECTION_NORTH) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_NORTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_EAST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_EAST) {
                    // ......    .......
                    // ..p...    ..p....
                    // ..p... -> .2p....
                    // ..cpp.    .23pp..
                    // ......    .311...
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_SOUTH);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_WEST);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_WEST);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_NORTH);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_WEST, DIRECTION_SOUTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            }
            break;
        case SNAKE_SEGMENT_SHAPE_SOUTH_WEST_CORNER:
            if (snake->constrict_state.left) {
                if (direction_to_head == DIRECTION_WEST) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_SOUTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_WEST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_SOUTH) {
                    // ......    .......
                    // ......    ...223.
                    // .ppc.. -> ..pp31.
                    // ...p..    ....p1.
                    // ...p..    ....p..
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_EAST);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_NORTH);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_NORTH);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_WEST);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_EAST, DIRECTION_NORTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            } else {
                if (direction_to_head == DIRECTION_SOUTH) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_SOUTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_WEST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_WEST) {
                    // ......    .......
                    // ......    ...113.
                    // .ppc.. -> ..pp32.
                    // ...p..    ....p2.
                    // ...p..    ....p..
                    // ......    .......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_NORTH);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_EAST);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_EAST);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_SOUTH);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_EAST, DIRECTION_NORTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            }
            break;
        case SNAKE_SEGMENT_SHAPE_SOUTH_EAST_CORNER:
            if (snake->constrict_state.left) {
                if (direction_to_head == DIRECTION_SOUTH) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_SOUTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_EAST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_EAST) {
                    // ......    ......
                    // ......    .311..
                    // ..cpp. -> .23pp.
                    // ..p...    .2p...
                    // ..p...    ..p...
                    // ......    ......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_NORTH);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_WEST);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_WEST);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_SOUTH);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_WEST, DIRECTION_NORTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            } else {
                if (direction_to_head == DIRECTION_EAST) {
                    S32 new_x = snake->segments[e].x;
                    S32 new_y = snake->segments[e].y;
                    adjacent_cell(DIRECTION_SOUTH, &new_x, &new_y);
                    adjacent_cell(DIRECTION_EAST, &new_x, &new_y);
                    if (!_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                        constricted_elements = INNER_CORNER_CONSTRICTED_SEGMENTS;
                        snake->segments[e].x = (S16)(new_x);
                        snake->segments[e].y = (S16)(new_y);
                    }
                } else if (direction_to_head == DIRECTION_SOUTH) {
                    // ......    ......
                    // ......    .322..
                    // ..cpp. -> .13pp.
                    // ..p...    .1p...
                    // ..p...    ..p...
                    // ......    ......
                    SnakeSegment first_path[2] = {};
                    SnakeSegment second_path[2] = {};

                    // Expand south
                    first_path[0] = _adjacent_snake_segment(snake->segments[e - 1], DIRECTION_WEST);
                    first_path[1] = _adjacent_snake_segment(first_path[0], DIRECTION_NORTH);

                    // Expand east
                    second_path[0] = _adjacent_snake_segment(snake->segments[e], DIRECTION_NORTH);
                    second_path[1] = _adjacent_snake_segment(second_path[0], DIRECTION_EAST);

                    if (_expand_outer_corner_if_open(game, snake, e, first_path, second_path, DIRECTION_WEST, DIRECTION_NORTH)) {
                        constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
                    }
                }
            }
            break;
        case SNAKE_SEGMENT_SHAPE_VERTICAL: {
            // Figure out the directions we'd want to move.
            Direction first_move_dir = DIRECTION_NONE;
            Direction second_move_dir = DIRECTION_NONE;

            if (direction_to_head == DIRECTION_SOUTH) {
                first_move_dir = DIRECTION_SOUTH;
                if (snake->constrict_state.left) {
                    second_move_dir = DIRECTION_EAST;
                } else {
                    second_move_dir = DIRECTION_WEST;
                }
            } else if (direction_to_head == DIRECTION_NORTH) {
                first_move_dir = DIRECTION_NORTH;
                if (snake->constrict_state.left) {
                    second_move_dir = DIRECTION_WEST;
                } else {
                    second_move_dir = DIRECTION_EAST;
                }
            }

            // Find the longest chain of vertical segments starting from here.
            S32 start_index = e;
            S32 end_index = e;
            while (true) {
                S32 new_x = snake->segments[end_index].x;
                S32 new_y = snake->segments[end_index].y;
                adjacent_cell(second_move_dir, &new_x, &new_y);
                if (snake_segment_shape(snake, end_index) != SNAKE_SEGMENT_SHAPE_VERTICAL) {
                    end_index--;
                    break;
                }
                if (_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                    break;
                }
                if (end_index < last_segment_index) {
                    end_index++;
                } else {
                    break;
                }
            }

            // If the vertical segment count is less than 2 (inclusive) then we don't do anything.
            if ((end_index - start_index) <= 1) {
                break;
            }

            // Move each of the segments after the first one in the direction of the constriction.
            for (S32 i = start_index + 1; i < end_index; i++) {
                S32 new_x = snake->segments[i].x;
                S32 new_y = snake->segments[i].y;
                adjacent_cell(first_move_dir, &new_x, &new_y);
                adjacent_cell(second_move_dir, &new_x, &new_y);
                snake->segments[i].x = (S16)(new_x);
                snake->segments[i].y = (S16)(new_y);
            }

            // For the final segment, drag the tail along with it.
            S32 new_x = snake->segments[end_index].x;
            S32 new_y = snake->segments[end_index].y;
            adjacent_cell(first_move_dir, &new_x, &new_y);
            _snake_drag(snake, end_index, new_x, new_y);
            adjacent_cell(second_move_dir, &new_x, &new_y);
            _snake_drag(snake, end_index, new_x, new_y);
            constricted_elements = (end_index - start_index) + 1;
            break;
        }
        case SNAKE_SEGMENT_SHAPE_HORIZONTAL: {
            // Figure out the directions we'd want to move.
            Direction first_move_dir = DIRECTION_NONE;
            Direction second_move_dir = DIRECTION_NONE;

            if (direction_to_head == DIRECTION_EAST) {
                first_move_dir = DIRECTION_EAST;
                if (snake->constrict_state.left) {
                    second_move_dir = DIRECTION_NORTH;
                } else {
                    second_move_dir = DIRECTION_SOUTH;
                }
            } else if (direction_to_head == DIRECTION_WEST) {
                first_move_dir = DIRECTION_WEST;
                if (snake->constrict_state.left) {
                    second_move_dir = DIRECTION_SOUTH;
                } else {
                    second_move_dir = DIRECTION_NORTH;
                }
            }

            // TODO: The rest of the code in this branch is identical to the code above, consolidate !
            // Find the longest chain of vertical segments starting from here.
            S32 start_index = e;
            S32 end_index = e;
            while (true) {
                S32 new_x = snake->segments[end_index].x;
                S32 new_y = snake->segments[end_index].y;
                adjacent_cell(second_move_dir, &new_x, &new_y);
                if (snake_segment_shape(snake, end_index) != SNAKE_SEGMENT_SHAPE_HORIZONTAL) {
                    end_index--;
                    break;
                }
                if (_game_is_solid_at(game, new_x, new_y, &snake_collision)) {
                    break;
                }
                if (end_index < last_segment_index) {
                    end_index++;
                } else {
                    break;
                }
            }

            // If the vertical segment count is less than 2 (inclusive) then we don't do anything.
            if ((end_index - start_index) <= 1) {
                break;
            }

            // Move each of the segments after the first one in the direction of the constriction.
            for (S32 i = start_index + 1; i < end_index; i++) {
                S32 new_x = snake->segments[i].x;
                S32 new_y = snake->segments[i].y;
                adjacent_cell(first_move_dir, &new_x, &new_y);
                adjacent_cell(second_move_dir, &new_x, &new_y);
                snake->segments[i].x = (S16)(new_x);
                snake->segments[i].y = (S16)(new_y);
            }

            // For the final segment, drag the tail along with it.
            S32 new_x = snake->segments[end_index].x;
            S32 new_y = snake->segments[end_index].y;
            adjacent_cell(first_move_dir, &new_x, &new_y);
            _snake_drag(snake, end_index, new_x, new_y);
            adjacent_cell(second_move_dir, &new_x, &new_y);
            _snake_drag(snake, end_index, new_x, new_y);
            constricted_elements = (end_index - start_index) + 1;
            break;
        }
        default:
            break;
        }
    }

    if (constricted_elements == 0) {
        snake->constrict_state.index = -1;
    }
}

void game_update(Game* game, SnakeAction* snake_actions) {
    S32 snakes_alive = 0;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        SnakeAction snake_action = snake_actions[s];
        _snake_turn(game, snake_action, s);
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (game->snakes[s].chomp_cooldown > 0) {
            game->snakes[s].chomp_cooldown--;
        }
        SnakeAction snake_action = snake_actions[s];
        if (snake_action & SNAKE_ACTION_CHOMP) {
            _snake_chomp(game->snakes + s, game);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        SnakeAction snake_action = snake_actions[s];
        if (game->snakes[s].constrict_state.index >= 0) {
            _snake_constrict(game->snakes + s, game);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_LEFT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = true;
            _snake_constrict(game->snakes + s, game);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_RIGHT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = false;
            _snake_constrict(game->snakes + s, game);
        }

    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        _snake_move(game->snakes + s, game);
        if (game->snakes[s].length != 0) {
            snakes_alive++;
        }
    }

    S32 taco_count = game_count_tacos(game);
    for (size_t i = taco_count; i < MAX_TACO_COUNT; i++) {
        game_spawn_taco(game);
    }

    if (snakes_alive == 1) {
        game->state = GAME_STATE_GAME_OVER;
    }
}

void game_destroy(Game* game) {
    level_destroy(&game->level);
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        snake_destroy(game->snakes + s);
    }
}

void game_spawn_taco(Game* game) {
    // If there are no tacos on the map, generate one in an empty cell.
    int32_t attempts = 0;
    while (attempts < 10) {
        int taco_x = rand() % game->level.width;
        int taco_y = rand() % game->level.height;

        CellType cell_type = level_get_cell(&game->level, taco_x, taco_y);
        if (cell_type != CELL_TYPE_EMPTY) {
            attempts++;
            continue;
        }
        bool spawned_on_snake = false;
        for (S32 s = 0; s < MAX_SNAKE_COUNT && !spawned_on_snake; s++) {
            for (S32 e = 0; e < game->snakes[s].length; e++) {
                if (game->snakes[s].segments[e].x == taco_x &&
                    game->snakes[s].segments[e].y == taco_y) {
                    spawned_on_snake = true;
                    break;
                }
            }
        }
        if (spawned_on_snake) {
            attempts++;
            continue;
        }

        level_set_cell(&game->level, taco_x, taco_y, CELL_TYPE_TACO);
        break;
    }
}

S32 game_count_tacos(Game* game) {
    S32 taco_count = 0;
    for(S32 y = 0; y < game->level.height; y++) {
        for(S32 x = 0; x < game->level.width; x++) {
            CellType cell_type = level_get_cell(&game->level, x, y);
            if (cell_type == CELL_TYPE_TACO) {
                taco_count++;
            }
        }
    }
    return taco_count;
}

size_t game_serialize(const Game* game, void* buffer, size_t buffer_size)
{
    U8 * byte_buffer = buffer;

    size_t msg_size = level_serialize(&game->level, byte_buffer, buffer_size);
    byte_buffer += msg_size;

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        msg_size = snake_serialize(game->snakes + s,
                                   byte_buffer,
                                   buffer_size - (byte_buffer - (U8*)buffer));
        byte_buffer += msg_size;
    }

    *byte_buffer = (U8)game->state;
    byte_buffer += sizeof(U8);

    return byte_buffer - (U8*)buffer;
}

size_t game_deserialize(void * buffer, size_t size, Game * out)
{
    U8 * byte_buffer = buffer;

    size_t msg_size = level_deserialize(byte_buffer, size, &out->level);
    byte_buffer += msg_size;

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        msg_size = snake_deserialize(byte_buffer,
                                                size - (byte_buffer - (U8*)buffer),
                                                &out->snakes[s]);
        byte_buffer += msg_size;
    }

    size_t size_of_serialized_game_state = sizeof(U8);
    out->state = *byte_buffer;
    byte_buffer += size_of_serialized_game_state;

    return byte_buffer - (U8*)buffer;
}
