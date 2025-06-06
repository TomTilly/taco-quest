//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"

typedef struct {
    S16 snake_index;
    S16 segment_index;
} SnakeCollision;

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
