//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"

#include <assert.h>
#include <stdio.h> // TODO: remove

typedef struct {
    S16 snake_index;
    S16 segment_index;
} SnakeCollision;

typedef struct {
    SnakeSegmentShape shape;
    S32 x;
    S32 y;
} SnakeConstrictSegmentInfo;

void _print_game(Game* game) {
    char** string = malloc(game->level.height * sizeof(char*));
    S32 string_length = game->level.width + 1; // plus one for null terminator.
    for (S32 i = 0; i < game->level.height; i++) {
        string[i] = malloc(string_length);
        memset(string[i], 0, string_length);
    }

    for (S32 y = 0; y < game->level.height; y++) {
        for (S32 x = 0; x < game->level.width; x++) {
            CellType cell = level_get_cell(&game->level, x, y);
            switch(cell) {
            case CELL_TYPE_EMPTY:
                string[y][x] = '.';
                break;
            case CELL_TYPE_WALL:
                string[y][x] = 'W';
                break;
            case CELL_TYPE_TACO:
                string[y][x] = 'T';
                break;
            default:
                string[y][x] = ' ';
                break;
            }
        }
    }

    char base_chars[MAX_SNAKE_COUNT] = {'a', 'A', '0'};
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S32 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            string[segment->y][segment->x] = (char)(base_chars[s] + e);
        }
    }

    for (S32 i = 0; i < game->level.height; i++) {
        printf("%s\n", string[i]);
    }

    for (S32 i = 0; i < game->level.height; i++) {
        free(string[i]);
    }
    free(string);
}

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

bool game_init(Game* game, S32 level_width, S32 level_height, S32 max_taco_count) {
    if (!level_init(&game->level, level_width, level_height)) {
        return false;
    }

    int32_t snake_capacity = level_width * level_height;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (!snake_init(game->snakes + s, snake_capacity)) {
            return false;
        }
    }

    game->max_taco_count = max_taco_count;
    game->state = GAME_STATE_WAITING;
    return true;
}

void game_clone(Game* input, Game* output) {
    if (input->level.width != output->level.width ||
        input->level.height != output->level.height) {
        level_destroy(&output->level);
        level_init(&output->level, input->level.width, input->level.height);
    }

    for (S32 x = 0; x < input->level.width; x++) {
        for (S32 y = 0; y < input->level.height; y++) {
            level_set_cell(&output->level, x, y, level_get_cell(&input->level, x, y));
        }
    }

    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (output->snakes[i].capacity != input->snakes[i].capacity) {
            snake_destroy(output->snakes + i);
            snake_init(output->snakes + i, input->snakes[i].capacity);
        }

        // Copy the snakes, but save the segments since that is a pointer that must be owned by the
        // output snake.
        SnakeSegment* output_segments = output->snakes[i].segments;
        output->snakes[i] = input->snakes[i];
        output->snakes[i].segments = output_segments;
        for (S32 l = 0; l < input->snakes[i].length; l++) {
            output->snakes[i].segments[l] = input->snakes[i].segments[l];
        }
    }

    output->state = input->state;
    output->max_taco_count = input->max_taco_count;
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

void _snake_drag_segments(Snake* snake, S32 segment_index, S32 new_x, S32 new_y) {
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

typedef struct {
    S32 x;
    S32 y;
} CellMove;

typedef enum {
    PUSH_OBJECT_EMPTY,
    PUSH_OBJECT_SUCCESS,
    PUSH_OBJECT_FAIL
} PushResult;

PushResult _game_object_push(Game* game, S32 x, S32 y, Direction direction);

bool _taco_push(Game* game, S32 x, S32 y, Direction direction) {
    S32 adjacent_x = x;
    S32 adjacent_y = y;
    adjacent_cell(direction, &adjacent_x, &adjacent_y);
    if (_game_object_push(game, adjacent_x, adjacent_y, direction) == PUSH_OBJECT_FAIL) {
        return false;
    }

    level_set_cell(&game->level, x, y, CELL_TYPE_EMPTY);
    level_set_cell(&game->level, adjacent_x, adjacent_y, CELL_TYPE_TACO);
    return true;
}

bool _game_empty_at(Game* game, S32 x, S32 y) {
    QueriedObject queried_object = game_query(game, x, y);
    return queried_object.type == QUERIED_OBJECT_TYPE_NONE;
}

PushResult _game_object_push(Game* game, S32 x, S32 y, Direction direction) {
    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        return PUSH_OBJECT_EMPTY;
    case QUERIED_OBJECT_TYPE_CELL:
        switch(queried_object.cell) {
        case CELL_TYPE_EMPTY:
            return PUSH_OBJECT_EMPTY;
        case CELL_TYPE_TACO:
            if (_taco_push(game, x, y, direction)) {
                return PUSH_OBJECT_SUCCESS;
            }
            return PUSH_OBJECT_FAIL;
        default:
            break;
        }
        break;
    case QUERIED_OBJECT_TYPE_SNAKE:
        if (snake_segment_push(game,
                               queried_object.snake.index,
                               queried_object.snake.segment_index,
                               direction)) {
            return PUSH_OBJECT_SUCCESS;
        }
        return PUSH_OBJECT_FAIL;
    default:
        break;
    }

    return false;
}

bool _snake_segment_can_expand(Game* game, S32 x, S32 y, Direction preferred_direction,
                               S32* result_x, S32* result_y) {
    // Prefer expanding straight ahead.
    S32 check_x = x;
    S32 check_y = y;
    adjacent_cell(preferred_direction, &check_x, &check_y);

    // TODO: We may need to push things out the way.
    if (_game_empty_at(game, check_x, check_y)) {
        *result_x = check_x;
        *result_y = check_y;
        return true;
    }

    for (S8 d = 0; d < DIRECTION_COUNT; d++) {
        Direction direction = (Direction)(d);
        if (direction == preferred_direction) {
            continue;
        }

        check_x = x;
        check_y = y;
        adjacent_cell(direction, &check_x, &check_y);

        if (_game_empty_at(game, check_x, check_y)) {
            *result_x = check_x;
            *result_y = check_y;
            return true;
        }
    }

    return false;
}

void _snake_segment_expand(Snake* snake, S32 starting_segment, S32 x, S32 y) {
    if (snake->length <= 0) {
        return;
    }

    for (S32 i = starting_segment; i < (snake->length - 1); i++) {
        snake->segments[i].x = snake->segments[i + 1].x;
        snake->segments[i].y = snake->segments[i + 1].y;
    }
    S32 tail_index = snake->length - 1;
    snake->segments[tail_index].x = (S16)(x);
    snake->segments[tail_index].y = (S16)(y);
}

// Push guarantees that if it returns true, the segment that was pushed moved and there is no
// segment at that cell.
bool snake_segment_push(Game* game, S32 snake_index, S32 segment_index, Direction direction) {
    Snake* snake = game->snakes + snake_index;
    SnakeSegment* segment_to_move = snake->segments + segment_index;

    S32 original_x = segment_to_move->x;
    S32 original_y = segment_to_move->y;

    Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction direction_to_tail = snake_segment_direction_to_tail(snake, segment_index);

    // Case 1 The head be pushed.
    //
    // .>1..    .....
    // ..2.. -> ..21.
    // ..3..    ..3..
    //
    if (segment_index == 0) {
        if (direction == direction_to_head || direction == direction_to_tail) {
            return false;
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        if (!_game_empty_at(game, first_cell_to_check_x, first_cell_to_check_y)) {
            PushResult first_push = _game_object_push(game, first_cell_to_check_x, first_cell_to_check_y, direction);
            PushResult second_push = PUSH_OBJECT_EMPTY;
            if (first_push == PUSH_OBJECT_FAIL) {
                second_push = _game_object_push(game, first_cell_to_check_x, first_cell_to_check_y, direction_to_tail);
                if (second_push == PUSH_OBJECT_FAIL) {
                    return false;
                }
            }
            if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
                PushResult push_result = _game_object_push(game, original_x, original_y, direction);
                return push_result != PUSH_OBJECT_FAIL;
            }
        }

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_tail, &final_cell_move_x, &final_cell_move_y);

        if (!_game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            PushResult first_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction);
            PushResult second_push = PUSH_OBJECT_EMPTY;
            if (first_push == PUSH_OBJECT_FAIL) {
                second_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction_to_tail);
                if (second_push == PUSH_OBJECT_FAIL) {
                    return false;
                }
            }
            if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
                PushResult push_result = _game_object_push(game, original_x, original_y, direction);
                return push_result != PUSH_OBJECT_FAIL;
            }
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return true;
    }

    // Case 2 The body be pushed.
    //
    // ..1..    ..12.
    // .>23. -> ...3.
    // .....    .....
    //
    bool segment_is_corner = directions_are_perpendicular(direction_to_head, direction_to_tail);
    if (segment_is_corner) {
        if (direction == opposite_direction(direction_to_head) ||
            direction == opposite_direction(direction_to_tail)) {
            return false;
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction_to_tail, &first_cell_to_check_x, &first_cell_to_check_y);

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        if (!_game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            PushResult first_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction_to_tail);
            PushResult second_push = PUSH_OBJECT_EMPTY;
            if (first_push == PUSH_OBJECT_FAIL) {
                second_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction_to_head);
                if (second_push == PUSH_OBJECT_FAIL) {
                    return false;
                }
            }
            if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
                PushResult push_result = _game_object_push(game, original_x, original_y, direction);
                return push_result != PUSH_OBJECT_FAIL;
            }
        }

        if ((segment_index + 1) < snake->length) {
            SnakeSegment* next_segment = segment_to_move + 1;
            if (next_segment->x == first_cell_to_check_x &&
                next_segment->y == first_cell_to_check_y) {
                segment_to_move->x = (S16)(final_cell_move_x);
                segment_to_move->y = (S16)(final_cell_move_y);
                return true;
            }
        }
    }

    // Case 3
    //
    // ..1..    ..1..
    // ..2.. -> ..23.
    // .>3..    .....
    // .....    .....
    //
    if (segment_index == (snake->length - 1)) {
        if (direction == direction_to_head || direction == direction_to_tail) {
            return false;
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        if (!_game_empty_at(game, first_cell_to_check_x, first_cell_to_check_y)) {
            PushResult first_push = _game_object_push(game, first_cell_to_check_x, first_cell_to_check_y, direction);
            PushResult second_push = PUSH_OBJECT_EMPTY;
            if (first_push == PUSH_OBJECT_FAIL) {
                second_push = _game_object_push(game, first_cell_to_check_x, first_cell_to_check_y, direction_to_tail);
                if (second_push == PUSH_OBJECT_FAIL) {
                    return false;
                }
            }
            if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
                PushResult push_result = _game_object_push(game, original_x, original_y, direction);
                return push_result != PUSH_OBJECT_FAIL;
            }
        }

        if (!_game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            PushResult first_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction);
            PushResult second_push = PUSH_OBJECT_EMPTY;
            if (first_push == PUSH_OBJECT_FAIL) {
                second_push = _game_object_push(game, final_cell_move_x, final_cell_move_y, direction_to_tail);
                if (second_push == PUSH_OBJECT_FAIL) {
                    return false;
                }
            }
            if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
                PushResult push_result = _game_object_push(game, original_x, original_y, direction);
                return push_result != PUSH_OBJECT_FAIL;
            }
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return true;
    }

    // Case 4
    //
    // ..1..    ..12.
    // .>2.. -> ...3.
    // ..3..    ...4.
    // ..4..    .....
    //
    if (!segment_is_corner) {
        // Handling exiting early for case 3.
        if (direction == direction_to_head || direction == direction_to_tail) {
            return false;
        }
    }

    S32 first_cell_to_check_x = segment_to_move->x;
    S32 first_cell_to_check_y = segment_to_move->y;
    adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

    S32 second_cell_to_check_x = first_cell_to_check_x;
    S32 second_cell_to_check_y = first_cell_to_check_y;
    adjacent_cell(opposite_direction(direction_to_head), &second_cell_to_check_x, &second_cell_to_check_y);

    S32 final_cell_move_x = first_cell_to_check_x;
    S32 final_cell_move_y = first_cell_to_check_y;
    adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

    if (!_game_empty_at(game, first_cell_to_check_x, first_cell_to_check_y)) {
        PushResult push = _game_object_push(game, first_cell_to_check_x, first_cell_to_check_y, direction_to_tail);
        if (push == PUSH_OBJECT_FAIL) {
            return false;
        }
        if (push == PUSH_OBJECT_SUCCESS) {
            PushResult push_result = _game_object_push(game, original_x, original_y, direction);
            return push_result != PUSH_OBJECT_FAIL;
        }
    }

    if (!_game_empty_at(game, second_cell_to_check_x, second_cell_to_check_y)) {
        PushResult first_push = _game_object_push(game, second_cell_to_check_x, second_cell_to_check_y, direction_to_tail);
        PushResult second_push = PUSH_OBJECT_EMPTY;
        if (first_push == PUSH_OBJECT_FAIL) {
            second_push = _game_object_push(game, second_cell_to_check_x, second_cell_to_check_y, opposite_direction(direction_to_head));
            if (second_push == PUSH_OBJECT_FAIL) {
                return false;
            }
        }
        if (first_push == PUSH_OBJECT_SUCCESS || second_push == PUSH_OBJECT_SUCCESS) {
            PushResult push_result = _game_object_push(game, original_x, original_y, direction);
            return push_result != PUSH_OBJECT_FAIL;
        }
    }

    segment_to_move->x = (S16)(second_cell_to_check_x);
    segment_to_move->y = (S16)(second_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, first_cell_to_check_x, first_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, final_cell_move_x, final_cell_move_y);
    return true;
}

bool snake_segment_constrict(Game* game, S32 snake_index, S32 segment_index, bool left) {
    Snake* snake = game->snakes + snake_index;

    S32 segment_to_move_index = segment_index + 1;
    if (snake->length <= segment_to_move_index) {
        return false;
    }

    SnakeSegment* segment_to_move = snake->segments + segment_to_move_index;

    Direction current_direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction next_direction_to_head = snake_segment_direction_to_head(snake, segment_to_move_index);

    Direction rotation_direction = (left) ?
        rotate_counter_clockwise(next_direction_to_head) :
        rotate_clockwise(next_direction_to_head);

    // Limit so we don't rotate in from of the current segment.
    if (rotation_direction == current_direction_to_head) {
        return false;
    }

    // Figure out adjacent cells we plan to move through.
    S32 initial_cell_move_x = segment_to_move->x;
    S32 initial_cell_move_y = segment_to_move->y;
    adjacent_cell(rotation_direction, &initial_cell_move_x, &initial_cell_move_y);

    // Case 1
    //
    // ..1..    ..12.
    // ..23. -> ...3.
    // .....    .....
    //

    bool segment_is_corner = false;
    S32 after_segment_to_move_index = segment_to_move_index + 1;
    if (after_segment_to_move_index < snake->length) {
        SnakeSegment* next_segment = snake->segments + after_segment_to_move_index;
        if (next_segment->x == initial_cell_move_x &&
            next_segment->y == initial_cell_move_y) {
            segment_is_corner = true;
        }
    }

    S32 final_cell_move_x = initial_cell_move_x;
    S32 final_cell_move_y = initial_cell_move_y;
    adjacent_cell(next_direction_to_head, &final_cell_move_x, &final_cell_move_y);

    // In the cases below, the we are operating on segment 1

    if (!_game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
        // if (!_game_object_push(game, final_cell_move_x, final_cell_move_y, rotation_direction) &&
        //     !_game_object_push(game, final_cell_move_x, final_cell_move_y, next_direction_to_head)) {

        // Case 2
        //
        // ..12.    ..1..
        // ..43. -> ..2..
        // .....    ..3..
        // .....    ..4..
        //

        S32 segment_to_check_index = after_segment_to_move_index + 1;
        if (segment_is_corner && segment_to_check_index < snake->length) {
            SnakeSegment* check_segment = snake->segments + segment_to_check_index;
            if (check_segment->x == final_cell_move_x &&
                check_segment->y == final_cell_move_y) {
                Direction after_next_to_head = snake_segment_direction_to_tail(snake, segment_to_check_index);
                S32 first_expanded_x = 0;
                S32 first_expanded_y = 0;
                S32 second_expanded_x = 0;
                S32 second_expanded_y = 0;
                SnakeSegment* tail = snake->segments + (snake->length - 1);
                if (_snake_segment_can_expand(game, tail->x, tail->y,
                                              after_next_to_head, &first_expanded_x, &first_expanded_y) &&
                    _snake_segment_can_expand(game, first_expanded_x, first_expanded_y,
                                              after_next_to_head, &second_expanded_x, &second_expanded_y)) {
                    _snake_segment_expand(snake, segment_to_move_index, first_expanded_x, first_expanded_y);
                    _snake_segment_expand(snake, segment_to_move_index, second_expanded_x, second_expanded_y);
                    return true;
                }
                return false;
            }
        }

        return false;
        // }
    }

    // If the next segment creates a corner with the previous segment, just move towards the diagonal.
    if (segment_is_corner) {
        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return true;
    }

    // Case 3
    //
    // ..1..    ..12.
    // ..2.. -> ..43.
    // ..3..    .....
    // ..4..    .....
    //

    if (!_game_empty_at(game, initial_cell_move_x, initial_cell_move_y)) {
        // if (!_game_object_push(game, initial_cell_move_x, initial_cell_move_y, rotation_direction) &&
        //     !_game_object_push(game, initial_cell_move_x, initial_cell_move_y, next_direction_to_head)) {
        return false;
        // }
    }

    // As long as the adjacent squares are empty, we can drag the snake's body through it.
    // TODO: If we can push things out of the way, that works too.
    _snake_drag_segments(snake, segment_to_move_index, initial_cell_move_x, initial_cell_move_y);
    _snake_drag_segments(snake, segment_to_move_index, final_cell_move_x, final_cell_move_y);
    return true;
}

void _snake_constrict(Game* game, S32 snake_index) {
    Snake* snake = game->snakes + snake_index;
    // TODO: update this comment.
    // First pass finds the shapes to operate on. This is so we don't operate on corners that we
    // create during this pass and only operate on the existing ones.

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    for (S32 segment_index = snake->constrict_state.index; segment_index < snake->length; segment_index++) {
        Game cloned_game = {0};
        game_clone(game, &cloned_game);

        if (snake_segment_constrict(&cloned_game, snake_index, segment_index, snake->constrict_state.left)) {
            // Apply the push by overwriting the original game with the clone.
            game_clone(&cloned_game, game);
            game_destroy(&cloned_game);

            // Update our snake pointer in case any re-allocation was required.
            snake = game->snakes + snake_index;

            S32 new_constrict_index = segment_index + 2;
            if (new_constrict_index >= snake->length) {
                snake->constrict_state.index = -1;
            } else {
                snake->constrict_state.index = new_constrict_index;
            }
            return;
        }

        game_destroy(&cloned_game);
    }

    // If we made it to the end without constricting, reset the state.
    snake->constrict_state.index = -1;
}

QueriedObject game_query(Game* game, S32 x, S32 y) {
    QueriedObject result = {0};

    CellType cell_type = level_get_cell(&game->level, x, y);
    if (cell_type != CELL_TYPE_EMPTY) {
        result.type = QUERIED_OBJECT_TYPE_CELL;
        result.cell = cell_type;
        return result;
    }

    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            if (segment->x == x && segment->y == y) {
                result.type = QUERIED_OBJECT_TYPE_SNAKE;
                result.snake.index = s;
                result.snake.segment_index = e;
                return result;
            }
        }
    }

    return result;
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
            _snake_constrict(game, s);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_LEFT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = true;
            _snake_constrict(game, s);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_RIGHT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = false;
            _snake_constrict(game, s);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        // Only allow movement if we aren't constricting.
        if (game->snakes[s].constrict_state.index == -1) {
            _snake_move(game->snakes + s, game);
        }
        if (game->snakes[s].length != 0) {
            snakes_alive++;
        }
    }

    S32 taco_count = game_count_tacos(game);
    for (size_t i = taco_count; i < game->max_taco_count; i++) {
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
