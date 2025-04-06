//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"


void snake_update(Snake* snake, Game* game) {
    int32_t new_snake_x = snake->segments[0].x;
    int32_t new_snake_y = snake->segments[0].y;

    adjacent_cell(snake->direction,
                  &new_snake_x,
                  &new_snake_y);

    CellType cell_type = level_get_cell(&game->level, new_snake_x, new_snake_y);

    bool collide_with_snake = false;

    // TODO: This simplifies when we have an array of snakes.
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (int i = 0; i < game->snakes[s].length; i++) {
            SnakeSegment* segment = game->snakes[s].segments + i;
            if (segment->x == new_snake_x &&
                segment->y == new_snake_y) {
                collide_with_snake = true;
                break;
            }
        }
    }

    if ((cell_type == CELL_TYPE_EMPTY || cell_type == CELL_TYPE_TACO) &&
        !collide_with_snake) {
        for (int i = snake->length; i >= 1; i--) {
            snake->segments[i].x = snake->segments[i - 1].x;
            snake->segments[i].y = snake->segments[i - 1].y;
        }

        snake->segments[0].x = new_snake_x;
        snake->segments[0].y = new_snake_y;

        if (cell_type == CELL_TYPE_TACO) {
            level_set_cell(&game->level, new_snake_x, new_snake_y, CELL_TYPE_EMPTY);
            snake_grow(snake);
            game_spawn_taco(game);
        }
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

    return true;
}

void game_apply_snake_action(Game* game, SnakeAction snake_action, S32 snake_index) {
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
    snake_turn(snake, direction);
}

void game_update(Game* game, SnakeAction snake_action, SnakeAction other_snake_action) {
    game_apply_snake_action(game, snake_action, false /* is_other_snake */);
    game_apply_snake_action(game, other_snake_action, true /* is_other_snake */);

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        snake_update(game->snakes + s, game);
    }

    size_t taco_count = 0;
    for(S32 y = 0; y < game->level.height; y++) {
        for(S32 x = 0; x < game->level.width; x++) {
            CellType cell_type = level_get_cell(&game->level, x, y);
            if (cell_type == CELL_TYPE_TACO) {
                taco_count++;
            }
        }
    }

    for (size_t i = taco_count; i < MAX_TACO_COUNT; i++) {
        game_spawn_taco(game);
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
