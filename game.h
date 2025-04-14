//
//  game.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef game_h
#define game_h

#include "snake.h"
#include "level.h"
#include "direction.h"

#define MAX_TACO_COUNT 6
#define MAX_SNAKE_COUNT 2

typedef struct {
    Level level;
    Snake snakes[MAX_SNAKE_COUNT];
} Game;

void snake_update(Snake* snake, Game* game, bool chomp);
bool game_init(Game* game, int32_t level_width, int32_t level_height);
void game_apply_snake_action(Game* game, SnakeAction snake_action, S32 snake_index);
bool game_snake_chomp(Game* game, S32 x, S32 y);
void game_update(Game* game, SnakeAction* snake_actions);
void game_destroy(Game* game);

void game_spawn_taco(Game* game);
S32 game_count_tacos(Game* game);

#endif /* game_h */
