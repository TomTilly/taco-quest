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

typedef struct {
    S32 x;
    S32 y;
} CellMove;

bool _snake_segment_push(Game* game, S32 snake_index, S32 segment_index, Direction direction);
bool _game_object_push(Game* game, S32 x, S32 y, Direction direction);

bool _taco_push(Game* game, S32 x, S32 y, Direction direction) {
    S32 adjacent_x = x;
    S32 adjacent_y = y;
    adjacent_cell(direction, &adjacent_x, &adjacent_y);
    if (!_game_object_push(game, adjacent_x, adjacent_y, direction)) {
        return false;
    }

    level_set_cell(&game->level, x, y, CELL_TYPE_EMPTY);
    level_set_cell(&game->level, adjacent_x, adjacent_y, CELL_TYPE_TACO);
    return true;
}

bool _game_object_push(Game* game, S32 x, S32 y, Direction direction) {
    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        return true;
    case QUERIED_OBJECT_TYPE_CELL:
        switch(queried_object.cell) {
        case CELL_TYPE_EMPTY:
            return true;
        case CELL_TYPE_TACO:
            return _taco_push(game, x, y, direction);
        default:
            break;
        }
    case QUERIED_OBJECT_TYPE_SNAKE: {
        bool first_push = _snake_segment_push(game,
                                              queried_object.snake.index,
                                              queried_object.snake.segment_index,
                                              direction);
        if (first_push) {
            // It's possible pushing a snake segment causes another snake segment to replace it, try to push again.
            // TODO: Make ascii diagram with example.
            return _game_object_push(game, x, y, direction);
        }
        return false;
    }
    default:
        break;
    }

    return false;
}

bool _snake_segment_push(Game* game, S32 snake_index, S32 segment_index, Direction direction) {
    Snake* snake = game->snakes + snake_index;
    Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);

    if (!directions_are_perpendicular(direction_to_head, direction)) {
        return false;
    }

    CellMove first_move = {
        .x = snake->segments[segment_index].x,
        .y = snake->segments[segment_index].y,
    };

    adjacent_cell(direction, &first_move.x, &first_move.y);
    if (_game_object_push(game, first_move.x, first_move.y, direction)) {
        CellMove second_move = first_move;
        adjacent_cell(direction_to_head, &second_move.x, &second_move.y);
        if (_game_object_push(game, second_move.x, second_move.y, direction_to_head) ||
            _game_object_push(game, second_move.x, second_move.y, direction)) {
            _snake_drag(snake, segment_index, first_move.x, first_move.y);
            _snake_drag(snake, segment_index, second_move.x, second_move.y);
            return true;
        }
    } else {
        Direction direction_to_tail = snake_segment_direction_to_tail(snake, segment_index);

        if (direction == direction_to_tail) {
            adjacent_cell(direction_to_head, &first_move.x, &first_move.y);
            if (_game_object_push(game, first_move.x, first_move.y, direction) ||
                _game_object_push(game, first_move.x, first_move.y, direction_to_head)) {
                snake->segments[segment_index].x = (S16)(first_move.x);
                snake->segments[segment_index].y = (S16)(first_move.y);
                return true;
            }
        }
    }

    return false;
}

void _snake_constrict(Game* game, S32 snake_index) {
    Snake* snake = game->snakes + snake_index;
    // TODO: update this comment.
    // First pass finds the shapes to operate on. This is so we don't operate on corners that we
    // create during this pass and only operate on the existing ones.

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    S32 constricted_elements = 0;
    for (S32 segment_index = snake->constrict_state.index; segment_index < snake->length; segment_index++) {
        if (segment_index == 1) {
            SnakeSegmentShape shape = snake_segment_shape(snake, segment_index);
            if (shape == SNAKE_SEGMENT_SHAPE_VERTICAL ||
                shape == SNAKE_SEGMENT_SHAPE_HORIZONTAL) {
                continue;
            }
        }

        Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);
        Direction push_direction = DIRECTION_NONE;
        if (snake->constrict_state.left) {
            push_direction = rotate_counter_clockwise(direction_to_head);
        } else {
            push_direction = rotate_clockwise(direction_to_head);
        }

        Game cloned_game = {0};
        game_clone(game, &cloned_game);

        // Attempt the push on the cloned game, if it succeeds, then ta
        if (_snake_segment_push(&cloned_game, snake_index, segment_index, push_direction)) {
            // Apply the push by overwriting the original game with the clone.
            game_clone(&cloned_game, game);
            game_destroy(&cloned_game);

            // Update our snake pointer in case any re-allocation was required.
            snake = game->snakes + snake_index;

            // TODO: Change index based on the type of constriction.
            constricted_elements = 2;
            snake->constrict_state.index = segment_index + constricted_elements;
            break;
        }

        game_destroy(&cloned_game);
    }

    if (constricted_elements == 0) {
        snake->constrict_state.index = -1;
    }
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
            // if (((snake_action & SNAKE_ACTION_CONSTRICT_LEFT_END) &&
            //     game->snakes[s].constrict_state.left) ||
            //     ((snake_action & SNAKE_ACTION_CONSTRICT_RIGHT_END) &&
            //     !game->snakes[s].constrict_state.left)) {
            //     game->snakes[s].constrict_state.index = -1;
            // } else {
            //     _snake_constrict(game, s);
            // }
            game->snakes[s].constrict_state.index = -1;
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_LEFT) {
            game->snakes[s].constrict_state.index = 1;
            game->snakes[s].constrict_state.left = true;
            _snake_constrict(game, s);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_RIGHT) {
            game->snakes[s].constrict_state.index = 1;
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
