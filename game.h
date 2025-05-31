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
#define MAX_SNAKE_COUNT 3

typedef enum {
    GAME_STATE_WAITING,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER
} GameState;

// Higher values take priority
typedef enum {
    SOUND_NONE,
    SOUND_EAT_TACO,
    SOUND_SNAKE_RECHARGE,
    SOUND_DAMAGE_SNAKE,
    SOUND_DESTROY_SNAKE,
} Sound;

typedef struct {
    Level level;
    Snake snakes[MAX_SNAKE_COUNT];
    GameState state;
    Sound current_sound;
} Game;

void snake_update(Snake* snake, Game* game, bool chomp);
bool game_init(Game* game, int32_t level_width, int32_t level_height);
void game_apply_snake_action(Game* game, SnakeAction snake_action, S32 snake_index);
void game_update(Game* game, SnakeAction* snake_actions);
void game_destroy(Game* game);
void game_set_sound(Game* game, Sound sound);
void game_play_current_sound(Game* game);

void game_spawn_taco(Game* game);
S32 game_count_tacos(Game* game);

size_t game_serialize(const Game* game, void* buffer, size_t buffer_size);
size_t game_deserialize(void * buffer, size_t size, Game * out);

#endif /* game_h */
