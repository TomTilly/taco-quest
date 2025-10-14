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

typedef enum {
    SNAKE_KILL_CHECK_UNREACHABLE,
    SNAKE_KILL_CHECK_CELL,
    SNAKE_KILL_CHECK_WALL,
    SNAKE_KILL_CHECK_TACO,
    SNAKE_KILL_CHECK_OTHER_SNAKE,
    SNAKE_KILL_CHECK_SELF,
} SnakeKillCheck;

typedef struct {
    S32 current_x;
    S32 current_y;
    S32 previous_x;
    S32 previous_y;
    S32 next_x;
    S32 next_y;
} SnakeSegmentPosition;

void _track_snake_segment_position(Snake* snake, S32 segment_index, SnakeSegmentPosition* snake_segment_position) {
    memset(snake_segment_position, 0, sizeof(*snake_segment_position));
    if (segment_index < 0 || segment_index >= snake->length) {
        return;
    }

    SnakeSegment* segment = snake->segments + segment_index;
    snake_segment_position->current_x = segment->x;
    snake_segment_position->current_y = segment->y;

    S32 previous_segment_index = segment_index - 1;
    if (previous_segment_index >= 0) {
        SnakeSegment* previous_segment = snake->segments + previous_segment_index;
        snake_segment_position->previous_x = previous_segment->x;
        snake_segment_position->previous_y = previous_segment->y;
    }

    S32 next_segment_index = segment_index + 1;
    if (next_segment_index < snake->length) {
        SnakeSegment* next_segment = snake->segments + next_segment_index;
        snake_segment_position->next_x = next_segment->x;
        snake_segment_position->next_y = next_segment->y;
    }
}

bool _snake_segment_positions_equal(SnakeSegmentPosition* a, SnakeSegmentPosition* b) {
    return a->current_x == b->current_x &&
           a->current_y == b->current_y &&
           a->previous_x == b->previous_x &&
           a->previous_y == b->previous_y &&
           a->next_x == b->next_x &&
           a->next_y == b->next_y;
}

void _print_game(Game* game) {
    S32 print_height = game->level.height + 1; // For coordinates.
    char** string = malloc(print_height * sizeof(char*));
    S32 string_length = game->level.width + 2; // For coordinates plus null terminator.
    for (S32 i = 0; i < print_height; i++) {
        string[i] = malloc(string_length);
        memset(string[i], 0, string_length);
    }

    string[0][0] = ' ';
    for (S32 x = 0; x < game->level.width; x++) {
        string[0][x + 1] = '0' + (x % 10);
    }

    for (S32 y = 0; y < game->level.height; y++) {
        S32 print_y = y + 1;
        string[print_y][0] = '0' + (y % 10);
        for (S32 x = 0; x < game->level.width; x++) {
            CellType cell = level_get_cell(&game->level, x, y);
            S32 print_x = x + 1;
            switch(cell) {
            case CELL_TYPE_EMPTY:
                string[print_y][print_x] = '.';
                break;
            case CELL_TYPE_WALL:
                string[print_y][print_x] = 'W';
                break;
            case CELL_TYPE_TACO:
                string[print_y][print_x] = 'T';
                break;
            default:
                string[print_y][print_x] = ' ';
                break;
            }
        }
    }

    char base_chars[MAX_SNAKE_COUNT] = {'a', 'A', '0'};
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S32 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            string[segment->y + 1][segment->x + 1] = (char)(base_chars[s] + (e % 26));
        }
    }

    printf("\n");
    for (S32 i = 0; i < print_height; i++) {
        printf("%s\n", string[i]);
    }

    for (S32 i = 0; i < print_height; i++) {
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
    if (snake->length <= 0) {
        return;
    }

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

void init_push_state(Game* game, PushState* push_state) {
    S32 size = game->level.height * game->level.width;
    push_state->cells = malloc(size);
    memset(push_state->cells, 0, size);
}

bool has_been_pushed(PushState* push_state, Game* game, S32 x, S32 y, Direction direction) {
    assert(direction < DIRECTION_COUNT);
    S32 index = y * game->level.width + x;
    return push_state->cells[index] & (1 << direction);
}

void mark_pushed(PushState* push_state, Game* game, S32 x, S32 y, Direction direction) {
    assert(direction < DIRECTION_COUNT);
    S32 index = y * game->level.width + x;
    push_state->cells[index] |= (1 << direction);
}

MoveResult _game_object_push_impl(Game* game, PushState* push_state, S32 x, S32 y, Direction direction);

MoveResult _taco_push(Game* game, PushState* push_state, S32 x, S32 y, Direction direction) {
    S32 adjacent_x = x;
    S32 adjacent_y = y;
    adjacent_cell(direction, &adjacent_x, &adjacent_y);
    MoveResult move_result = _game_object_push_impl(game, push_state, adjacent_x, adjacent_y, direction);
    if (move_result == MOVE_OBJECT_SUCCESS || move_result == MOVE_OBJECT_EMPTY) {
        level_set_cell(&game->level, x, y, CELL_TYPE_EMPTY);
        level_set_cell(&game->level, adjacent_x, adjacent_y, CELL_TYPE_TACO);
    }
    return move_result;
}

bool game_empty_at(Game* game, S32 x, S32 y) {
    QueriedObject queried_object = game_query(game, x, y);
    return queried_object.type == QUERIED_OBJECT_TYPE_NONE;
}

MoveResult game_empty_push_result(Game* game, S32 x, S32 y) {
    bool check = game_empty_at(game, x, y);
    if (check) {
        return MOVE_OBJECT_FAIL;
    }
    return MOVE_OBJECT_SUCCESS;
}

MoveResult _game_object_push_impl(Game* game, PushState* push_state, S32 x, S32 y, Direction direction) {
    if (has_been_pushed(push_state, game, x, y, direction)) {
        return MOVE_OBJECT_FAIL;
    }
    mark_pushed(push_state, game, x, y, direction);

    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        return MOVE_OBJECT_EMPTY;
    case QUERIED_OBJECT_TYPE_CELL:
        switch(queried_object.cell) {
        case CELL_TYPE_EMPTY:
            return MOVE_OBJECT_EMPTY;
        case CELL_TYPE_TACO:
            return _taco_push(game, push_state, x, y, direction);
        case CELL_TYPE_WALL:
            return MOVE_OBJECT_FAIL;
        default:
            break;
        }
        break;
    case QUERIED_OBJECT_TYPE_SNAKE:
        if (push_state->original_snake_index == queried_object.snake.index) {
            return MOVE_OBJECT_FAIL;
        }
        return snake_segment_push(game,
                                  push_state,
                                  queried_object.snake.index,
                                  queried_object.snake.segment_index,
                                  direction);
    default:
        break;
    }

    return MOVE_OBJECT_EMPTY;
}

bool _snake_segment_can_expand(Game* game, S32 x, S32 y, Direction preferred_direction,
                               S32* result_x, S32* result_y) {
    // Prefer expanding straight ahead.
    S32 check_x = x;
    S32 check_y = y;
    adjacent_cell(preferred_direction, &check_x, &check_y);

    // TODO: We may need to push things out the way.
    if (game_empty_at(game, check_x, check_y)) {
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

        if (game_empty_at(game, check_x, check_y)) {
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

    S32 tail_index = snake->length - 1;
    for (S32 i = starting_segment; i < tail_index; i++) {
        snake->segments[i].x = snake->segments[i + 1].x;
        snake->segments[i].y = snake->segments[i + 1].y;
    }
    snake->segments[tail_index].x = (S16)(x);
    snake->segments[tail_index].y = (S16)(y);
}

MoveResult _game_if_cell_not_empty_try_push_impl(Game* game,
                                                 PushState* push_state,
                                                 S32 target_cell_x,
                                                 S32 target_cell_y,
                                                 Direction first_direction,
                                                 Direction second_direction) {

    MoveResult result = MOVE_OBJECT_EMPTY;
    if (!game_empty_at(game, target_cell_x, target_cell_y)) {
        // If not empty at the target, attempt to push in the specified direction.
        result = _game_object_push_impl(game, push_state, target_cell_x, target_cell_y, first_direction);

        if (result == MOVE_OBJECT_FAIL) {
            // If the first push failed, try again but in the second specified direction.
            result = _game_object_push_impl(game, push_state, target_cell_x, target_cell_y, second_direction);
        }
    }

    return result;
}

MoveResult _game_if_cell_not_empty_try_push(Game* game,
                                            S32 original_snake_index,
                                            S32 target_cell_x,
                                            S32 target_cell_y,
                                            Direction first_direction,
                                            Direction second_direction) {
    PushState push_state = {0};
    init_push_state(game, &push_state);
    // TODO: Maybe init_push_state() should take this as a param.
    push_state.original_snake_index = original_snake_index;

    MoveResult result = _game_if_cell_not_empty_try_push_impl(game,
                                                              &push_state,
                                                              target_cell_x,
                                                              target_cell_y,
                                                              first_direction,
                                                              second_direction);

    free(push_state.cells);
    return result;
}

MoveResult _pass_along_game_object_push(Game* game,
                                        PushState* push_state,
                                        S32 cell_x,
                                        S32 cell_y,
                                        Direction direction) {
    S32 adjacent_cell_to_push_x = cell_x;
    S32 adjacent_cell_to_push_y = cell_y;
    adjacent_cell(direction, &adjacent_cell_to_push_x, &adjacent_cell_to_push_y);
    MoveResult result = _game_object_push_impl(game,
                                               push_state,
                                               adjacent_cell_to_push_x,
                                               adjacent_cell_to_push_y,
                                               direction);
    if (result == MOVE_OBJECT_SUCCESS) {
        // Do we check if the game is empty and forward on the success if so ?
        return MOVE_OBJECT_PROGRESS;
    }
    if (result == MOVE_OBJECT_EMPTY) {
        // If the push is a pass through that doesn't move the current object and there is nothing
        // to push, then we didn't push the original object.
        return MOVE_OBJECT_FAIL;
    }
    return result;
}

// Push guarantees that if it returns true, the segment that was pushed moved and there is no
// segment at that cell.
MoveResult snake_segment_push(Game* game, PushState* push_state, S32 snake_index, S32 segment_index, Direction direction) {
    // TODO: More testing before we do something like this
    // if (!snake_segment_is_pushable(game, snake_index, segment_index, direction)) {
    //     return false;
    // }

    Snake* snake = game->snakes + snake_index;
    SnakeSegment* segment_to_move = snake->segments + segment_index;

    SnakeSegmentPosition original_segment_pos = {0};
    _track_snake_segment_position(snake, segment_index, &original_segment_pos);

    Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction direction_to_tail = snake_segment_direction_to_tail(snake, segment_index);

    // Case 1 The head be pushed.
    //
    // .>1..    .....
    // ..2.. -> ..21.
    // ..3..    ..3..
    //
    if (segment_index == 0) {
        if (!directions_are_perpendicular(direction, direction_to_tail)) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       first_cell_to_check_x,
                                                                       first_cell_to_check_y,
                                                                       direction,
                                                                       direction_to_tail);

        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_tail, &final_cell_move_x, &final_cell_move_y);

        push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                            push_state,
                                                            final_cell_move_x,
                                                            final_cell_move_y,
                                                            direction,
                                                            direction_to_tail);
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return MOVE_OBJECT_PROGRESS;
        }

        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
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
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction_to_tail, &first_cell_to_check_x, &first_cell_to_check_y);

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       final_cell_move_x,
                                                                       final_cell_move_y,
                                                                       direction_to_tail,
                                                                       direction_to_head);

        if (push_result == MOVE_OBJECT_FAIL) {
            // Since the final move is diagonal, if we fail to make that push, fall back to pass
            // through pushing on our adjacent segments.
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        } else if (push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        if ((segment_index + 1) < snake->length) {
            SnakeSegment* next_segment = segment_to_move + 1;
            if (next_segment->x == first_cell_to_check_x &&
                next_segment->y == first_cell_to_check_y) {
                segment_to_move->x = (S16)(final_cell_move_x);
                segment_to_move->y = (S16)(final_cell_move_y);
                return MOVE_OBJECT_SUCCESS;
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
        if (!directions_are_perpendicular(direction, direction_to_head)) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       first_cell_to_check_x,
                                                                       first_cell_to_check_y,
                                                                       direction,
                                                                       direction_to_head);
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                            push_state,
                                                            final_cell_move_x,
                                                            final_cell_move_y,
                                                            direction, direction_to_head);
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return MOVE_OBJECT_PROGRESS;
        }

        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
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
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }
    }

    S32 first_cell_to_check_x = segment_to_move->x;
    S32 first_cell_to_check_y = segment_to_move->y;
    adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

    MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                   push_state,
                                                                   first_cell_to_check_x,
                                                                   first_cell_to_check_y,
                                                                   direction,
                                                                   direction_to_head);
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return push_result;
    }

    SnakeSegmentPosition updated_segment_pos = {0};
    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    S32 second_cell_to_check_x = first_cell_to_check_x;
    S32 second_cell_to_check_y = first_cell_to_check_y;
    adjacent_cell(opposite_direction(direction_to_head), &second_cell_to_check_x, &second_cell_to_check_y);

    push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                        push_state,
                                                        second_cell_to_check_x,
                                                        second_cell_to_check_y,
                                                        direction,
                                                        direction_to_head);
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return MOVE_OBJECT_PROGRESS;
    }

    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    S32 final_cell_move_x = first_cell_to_check_x;
    S32 final_cell_move_y = first_cell_to_check_y;
    adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

    push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                        push_state,
                                                        final_cell_move_x,
                                                        final_cell_move_y,
                                                        direction,
                                                        direction_to_head);
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return MOVE_OBJECT_PROGRESS;
    }

    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    segment_to_move->x = (S16)(second_cell_to_check_x);
    segment_to_move->y = (S16)(second_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, first_cell_to_check_x, first_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, final_cell_move_x, final_cell_move_y);
    return MOVE_OBJECT_SUCCESS;
}

MoveResult snake_segment_constrict(Game* game, S32 snake_index, S32 segment_index, bool left) {
    Snake* snake = game->snakes + snake_index;

    S32 segment_to_move_index = segment_index + 1;
    if (snake->length <= segment_to_move_index) {
        return MOVE_OBJECT_FAIL;
    }

    SnakeSegment* segment_to_move = snake->segments + segment_to_move_index;

    Direction current_direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction next_direction_to_head = snake_segment_direction_to_head(snake, segment_to_move_index);

    assert(current_direction_to_head < DIRECTION_COUNT);
    assert(next_direction_to_head < DIRECTION_COUNT);

    Direction rotation_direction = (left) ?
        rotate_counter_clockwise(next_direction_to_head) :
        rotate_clockwise(next_direction_to_head);

    // Limit so we don't rotate in from of the current segment.
    if (rotation_direction == current_direction_to_head) {
        return MOVE_OBJECT_FAIL;
    }

    SnakeSegmentPosition original_segment_pos = {0};
    _track_snake_segment_position(snake, segment_to_move_index, &original_segment_pos);

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

    if (segment_is_corner) {
        // Case 2
        //
        // ..12.    ..1..
        // ..43. -> ..2..
        // .....    ..3..
        // .....    ..4..
        //

        if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            S32 segment_to_check_index = after_segment_to_move_index + 1;
            if (segment_to_check_index < snake->length) {
                SnakeSegment* check_segment = snake->segments + segment_to_check_index;
                if (check_segment->x == final_cell_move_x &&
                    check_segment->y == final_cell_move_y) {
                    S32 tail_index = (snake->length - 1);
                    Direction tail_direction_to_expand = snake_segment_direction_to_tail(snake, tail_index);
                    S32 first_expanded_x = 0;
                    S32 first_expanded_y = 0;
                    S32 second_expanded_x = 0;
                    S32 second_expanded_y = 0;
                    SnakeSegment* tail = snake->segments + (snake->length - 1);
                    if (_snake_segment_can_expand(game, tail->x, tail->y,
                                                  tail_direction_to_expand, &first_expanded_x, &first_expanded_y) &&
                        _snake_segment_can_expand(game, first_expanded_x, first_expanded_y,
                                                  tail_direction_to_expand, &second_expanded_x, &second_expanded_y)) {
                        _snake_segment_expand(snake, segment_to_move_index, first_expanded_x, first_expanded_y);
                        _snake_segment_expand(snake, segment_to_move_index, second_expanded_x, second_expanded_y);
                        return MOVE_OBJECT_SUCCESS;
                    }
                    return MOVE_OBJECT_FAIL;
                }
            }

            MoveResult push_result = _game_if_cell_not_empty_try_push(game,
                                                                      snake_index,
                                                                      final_cell_move_x,
                                                                      final_cell_move_y,
                                                                      rotation_direction,
                                                                      next_direction_to_head);

            if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
                return push_result;
            }

            SnakeSegmentPosition updated_segment_pos = {0};
            _track_snake_segment_position(snake, segment_to_move_index, &updated_segment_pos);
            if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
                return MOVE_OBJECT_FAIL;
            }
        }

        // If the next segment creates a corner with the previous segment, just move towards the diagonal.
        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
    }

    // Case 3
    //
    // ..1..    ..12.
    // ..2.. -> ..43.
    // ..3..    .....
    // ..4..    .....
    //

    // Clone the game before making the push.
    Game first_cloned_game = {0};
    game_clone(game, &first_cloned_game);

    MoveResult first_push_first_result =
        _game_if_cell_not_empty_try_push(&first_cloned_game,
                                          snake_index,
                                          initial_cell_move_x,
                                          initial_cell_move_y,
                                          rotation_direction,
                                          next_direction_to_head);

    MoveResult first_push_second_result =
        _game_if_cell_not_empty_try_push(&first_cloned_game,
                                         snake_index,
                                         final_cell_move_x,
                                         final_cell_move_y,
                                         rotation_direction,
                                         next_direction_to_head);

    if (first_push_first_result == MOVE_OBJECT_SUCCESS && first_push_second_result == MOVE_OBJECT_SUCCESS) {
        game_clone(&first_cloned_game, game);
        game_destroy(&first_cloned_game);
    } else {
        // If either of the first attempts fail, try reversing the order of the pushes. We found
        // scenarios where this retry works and keeps our rules simple.
        Game second_cloned_game = {0};
        game_clone(game, &second_cloned_game);

        MoveResult second_push_first_result =
            _game_if_cell_not_empty_try_push(&second_cloned_game,
                                             snake_index,
                                             final_cell_move_x,
                                             final_cell_move_y,
                                             rotation_direction,
                                             next_direction_to_head);

        MoveResult second_push_second_result =
            _game_if_cell_not_empty_try_push(&second_cloned_game,
                                             snake_index,
                                             initial_cell_move_x,
                                             initial_cell_move_y,
                                             rotation_direction,
                                             next_direction_to_head);

        if (second_push_first_result == MOVE_OBJECT_SUCCESS && second_push_second_result == MOVE_OBJECT_SUCCESS) {
            // If the second succeeded in both use that, since we know the first did not succeed in both.
            game_clone(&second_cloned_game, game);
            game_destroy(&first_cloned_game);
            game_destroy(&second_cloned_game);
        } else {
            bool first_pair_has_fail =
                (first_push_first_result == MOVE_OBJECT_FAIL || first_push_second_result == MOVE_OBJECT_FAIL);
            bool second_pair_has_fail =
                (second_push_first_result == MOVE_OBJECT_FAIL || second_push_second_result == MOVE_OBJECT_FAIL);

            // If we find a fail in both push pairs, exit out.
            if (first_pair_has_fail) {
                if (second_pair_has_fail) {
                    // TODO: These return code paths that need to destroy are rough !
                    game_destroy(&first_cloned_game);
                    game_destroy(&second_cloned_game);
                    return MOVE_OBJECT_FAIL;
                } else {
                    game_clone(&second_cloned_game, game);

                    if (second_push_first_result == MOVE_OBJECT_PROGRESS ||
                        second_push_second_result == MOVE_OBJECT_PROGRESS) {
                        game_destroy(&first_cloned_game);
                        game_destroy(&second_cloned_game);
                        return MOVE_OBJECT_PROGRESS;
                    }
                }
            } else {
                game_clone(&first_cloned_game, game);

                if (first_push_first_result == MOVE_OBJECT_PROGRESS ||
                    first_push_second_result == MOVE_OBJECT_PROGRESS) {
                    game_destroy(&first_cloned_game);
                    game_destroy(&second_cloned_game);
                    return MOVE_OBJECT_PROGRESS;
                }
            }

            game_destroy(&first_cloned_game);
            game_destroy(&second_cloned_game);
        }
    }

    // Update snake pointer after game pointer changed.
    snake = game->snakes + snake_index;

    // Check that both cells are empty after our successful pushes, because the first could have
    // succeed, then the second one succeeded and pushed something into the first.
    if (!game_empty_at(game, initial_cell_move_x, initial_cell_move_y) ||
        !game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
        return MOVE_OBJECT_FAIL;
    }

    SnakeSegmentPosition updated_segment_pos = {0};
    _track_snake_segment_position(snake, segment_to_move_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return MOVE_OBJECT_FAIL;
    }

    // As long as the adjacent squares are empty, we can drag the snake's body through it.
    // TODO: If we can push things out of the way, that works too.
    _snake_drag_segments(snake, segment_to_move_index, initial_cell_move_x, initial_cell_move_y);
    _snake_drag_segments(snake, segment_to_move_index, final_cell_move_x, final_cell_move_y);
    return MOVE_OBJECT_SUCCESS;
}

bool snake_segment_is_pushable(Game* game, S32 snake_index, S32 segment_index, Direction from) {
    if (snake_index < 0 || snake_index >= MAX_SNAKE_COUNT) {
        return false;
    }

    Snake* snake = game->snakes + snake_index;

    if (segment_index < 0 || segment_index >= snake->length) {
        return false;
    }

    // Corners are always pushable. This is a bit of a hack cause we assume you have to succeed in
    // pushing the adjacent segments to even push an inner corner.
    if (directions_are_perpendicular(snake_segment_direction_to_head(snake, segment_index),
                                     snake_segment_direction_to_tail(snake, segment_index))
                                     // The head and tail cannot be corners.
                                     && segment_index != 0 && segment_index != (snake->length - 1) ) {
        return true;
    }

    Direction towards_head_turn = DIRECTION_COUNT;
    Direction towards_tail_turn = DIRECTION_COUNT;

    S32 prev_segment_index = segment_index - 1;
    Direction original_direction = opposite_direction(snake_segment_direction_to_tail(snake, segment_index));
    while (prev_segment_index >= 0) {
        Direction current_direction = snake_segment_direction_to_head(snake, prev_segment_index);
        if (current_direction != original_direction) {
            towards_head_turn = current_direction;
            break;
        }
        prev_segment_index--;
    }

    S32 next_segment_index = segment_index + 1;
    original_direction = opposite_direction(snake_segment_direction_to_head(snake, segment_index));
    while (next_segment_index < snake->length) {
        Direction current_direction = snake_segment_direction_to_tail(snake, next_segment_index);
        if (current_direction != original_direction) {
            towards_tail_turn = current_direction;
            break;
        }
        next_segment_index++;
    }

    if (towards_head_turn == towards_tail_turn) {
        // If the snake is straight, it is pushable.
        if (towards_head_turn == DIRECTION_COUNT) {
            return true;
        }

        // With 2 inner corners on either side, we cannot be pushed.
        if (towards_head_turn == opposite_direction(from)) {
            return false;
        }
    // Uncomment this to enable the head and tail to act as inner anchors as well.
    // } else {
    //     // If either end is the head or tail, use the other (since we know they are not equal).
    //     if (towards_head_turn == DIRECTION_COUNT && towards_tail_turn == opposite_direction(from)) {
    //         return false;
    //     } else if (towards_tail_turn == DIRECTION_COUNT && towards_head_turn == opposite_direction(from)) {
    //         return false;
    //     }
    }

    return true;
}

SnakeKillCheck* kill_check_entry(Game* game, SnakeKillCheck* kill_checks, S32 x, S32 y) {
    return kill_checks + (y * game->level.width) + x;
}

void _flood_fill_kill_checks(Game* game, SnakeKillCheck* kill_checks, S32 snake_index, S32 x, S32 y) {
    SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);

    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        *entry = SNAKE_KILL_CHECK_CELL;
        break;
    case QUERIED_OBJECT_TYPE_SNAKE:
        if (queried_object.snake.index == snake_index) {
            *entry = SNAKE_KILL_CHECK_SELF;
        } else {
            *entry = SNAKE_KILL_CHECK_OTHER_SNAKE;
        }
        break;
    case QUERIED_OBJECT_TYPE_CELL:
        if (queried_object.cell == CELL_TYPE_WALL) {
            *entry = SNAKE_KILL_CHECK_WALL;
            // Creates a boundary that we do not pass.
            return;
        } else if (queried_object.cell == CELL_TYPE_TACO) {
            *entry = SNAKE_KILL_CHECK_TACO;
        } else {
            *entry = SNAKE_KILL_CHECK_CELL;
        }
        break;
    }

    for (S32 d = 0; d < DIRECTION_COUNT; d++) {
        S32 next_x = x;
        S32 next_y = y;
        adjacent_cell(d, &next_x, &next_y);
        if (next_x < 0 || next_x >= game->level.width || next_y < 0 || next_y >= game->level.height) {
            continue;
        }

        SnakeKillCheck* next_entry = kill_check_entry(game, kill_checks, next_x, next_y);
        if (*next_entry == SNAKE_KILL_CHECK_UNREACHABLE) {
            _flood_fill_kill_checks(game, kill_checks, snake_index, next_x, next_y);
        }
    }
}

void snake_constrict(Game* game, S32 snake_index, SnakeConstrictState constrict_state) {
    Snake* snake = game->snakes + snake_index;
    // TODO: update this comment.
    // First pass finds the shapes to operate on. This is so we don't operate on corners that we
    // create during this pass and only operate on the existing ones.
    if (constrict_state == SNAKE_CONSTRICT_STATE_NONE) {
        return;
    }

    // Allocate an array of the size of the level wherel each cell is flood filled inside where
    // the snake is constricting.
    S32 cell_count = game->level.width * game->level.height;
    SnakeKillCheck* kill_checks = malloc(cell_count * sizeof(*kill_checks));

    // TODO: Evaluate if we want to go back to doing a single segment constrict per tick.

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    for (S32 segment_index = 0; segment_index < snake->length; segment_index++) {
        bool left = (constrict_state == SNAKE_CONSTRICT_STATE_LEFT);

        // if we haven't unfurled yet, skip.
        if (segment_index > 0 &&
            snake->segments[segment_index].x == snake->segments[segment_index - 1].x &&
            snake->segments[segment_index].y == snake->segments[segment_index - 1].y) {
            continue;
        }

        if (segment_index < (snake->length - 1) &&
            snake->segments[segment_index].x == snake->segments[segment_index + 1].x &&
            snake->segments[segment_index].y == snake->segments[segment_index + 1].y) {
            continue;
        }

        S32 original_segment_index = segment_index;
        if (snake_segment_constrict(game, snake_index, segment_index, left) == MOVE_OBJECT_SUCCESS) {
            segment_index++;
        }

        {
            // Always check for a kill !

            // Reset the board.
            for (S32 i = 0; i < cell_count; i++) {
                kill_checks[i] = SNAKE_KILL_CHECK_UNREACHABLE;
            }

            // Populate the snake segments.
            for (S32 i = 0; i < snake->length; i++) {
                SnakeSegment* segment = snake->segments + i;

                SnakeKillCheck* entry = kill_check_entry(game, kill_checks, segment->x, segment->y);
                *entry = SNAKE_KILL_CHECK_SELF;
            }

            // If a segment failed to constrict, check if we killed another snake !
            SnakeSegment* segment = snake->segments + original_segment_index;

            Direction direction_to_head = snake_segment_direction_to_head(snake, original_segment_index);
            Direction direction_to_tail = snake_segment_direction_to_tail(snake, original_segment_index);

            if (opposite_direction(direction_to_head) != direction_to_tail) {
                continue;
            }

            S32 cell_to_fill_x = segment->x;
            S32 cell_to_fill_y = segment->y;
            Direction direction_to_cell = DIRECTION_NONE;
            if (left) {
                direction_to_cell = rotate_counter_clockwise(direction_to_head);
            } else {
                direction_to_cell = rotate_clockwise(direction_to_head);
            }

            adjacent_cell(direction_to_cell, &cell_to_fill_x, &cell_to_fill_y);

            // Flood fill inside the snake's constriction.
            _flood_fill_kill_checks(game, kill_checks, snake_index, cell_to_fill_x, cell_to_fill_y);

            // Debug printing.
            // printf("\n");
            // for (S32 y = 0; y < game->level.height; y++) {
            //     for (S32 x = 0; x < game->level.width; x++) {
            //         SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);

            //         // point out the current segment.
            //         if (x == segment->x && y == segment->y) {
            //             printf("c");
            //             continue;
            //         }

            //         switch(*entry){
            //         case SNAKE_KILL_CHECK_UNREACHABLE:
            //             printf(" ");
            //             break;
            //         case SNAKE_KILL_CHECK_WALL:
            //             printf("w");
            //             break;
            //         case SNAKE_KILL_CHECK_TACO:
            //             printf("T");
            //             break;
            //         case SNAKE_KILL_CHECK_CELL:
            //             printf(".");
            //             break;
            //         case SNAKE_KILL_CHECK_OTHER_SNAKE:
            //             printf("s");
            //             break;
            //         case SNAKE_KILL_CHECK_SELF:
            //             printf("m");
            //             break;
            //         }
            //     }
            //     printf("\n");
            // }

            // There are configurations where a single empty cell is unavoidable, but as long as
            // there are not multiple empty cells together, then we kill the any snake inside.
            S32 adjacent_empty_cells = 0;
            for (S32 y = 0; y < game->level.height; y++) {
                for (S32 x = 0; x < game->level.width; x++) {
                    SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);
                    if (*entry == SNAKE_KILL_CHECK_CELL) {
                        for (S8 d = 0; d < DIRECTION_COUNT; d++) {
                            S32 adjacent_x = x;
                            S32 adjacent_y = y;
                            adjacent_cell(d, &adjacent_x, &adjacent_y);

                            SnakeKillCheck* adjacent_entry = kill_check_entry(game, kill_checks, adjacent_x, adjacent_y);
                            if (*adjacent_entry == SNAKE_KILL_CHECK_CELL) {
                                adjacent_empty_cells++;
                            }
                        }
                    }
                }
            }

            if (adjacent_empty_cells == 0) {
                // Track how many snake segments are inside the constriction and number of adjacent cells.
                S32 snake_segment_counts[MAX_SNAKE_COUNT];
                memset(snake_segment_counts, 0, sizeof(snake_segment_counts[0]) * MAX_SNAKE_COUNT);
                for (S32 y = 0; y < game->level.height; y++) {
                    for (S32 x = 0; x < game->level.width; x++) {
                        SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);
                        if (*entry == SNAKE_KILL_CHECK_OTHER_SNAKE) {
                            QueriedObject queried_object = game_query(game, x, y);
                            if (queried_object.type == QUERIED_OBJECT_TYPE_SNAKE) {
                                snake_segment_counts[queried_object.snake.index]++;
                            }
                        }
                    }
                }

                // If all of the segments are found within the constriction, kill the snake and replace with tacos.
                for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
                    Snake* snake_to_kill = game->snakes + i;
                    if (snake_segment_counts[i] == snake_to_kill->length) {
                        for (S32 l = 0; l < snake_to_kill->length; l++) {
                            SnakeSegment* segment_to_kill = snake_to_kill->segments + l;
                            level_set_cell(&game->level, segment_to_kill->x, segment_to_kill->y, CELL_TYPE_TACO);
                        }
                        game->snakes[i].length = 0;
                    }
                }
            }
        }
    }

    free(kill_checks);
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

    SnakeConstrictState constrict_state[MAX_SNAKE_COUNT];
    memset(constrict_state, 0, sizeof(constrict_state[0]) * MAX_SNAKE_COUNT);

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        SnakeAction snake_action = snake_actions[s];
        if (snake_action & SNAKE_ACTION_CONSTRICT_LEFT) {
            constrict_state[s] = SNAKE_CONSTRICT_STATE_LEFT;
            snake_constrict(game, s, constrict_state[s]);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_RIGHT) {
            constrict_state[s] = SNAKE_CONSTRICT_STATE_RIGHT;
            snake_constrict(game, s, constrict_state[s]);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        // Only allow movement if we aren't constricting.
        if (constrict_state[s] == SNAKE_CONSTRICT_STATE_NONE) {
            _snake_move(game->snakes + s, game);
        }
        if (game->snakes[s].length != 0) {
            snakes_alive++;
        }
    }

    S32 taco_count = game_count_tacos(game);
    for (size_t i = taco_count; i < (size_t)game->max_taco_count; i++) {
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
