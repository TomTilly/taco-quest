//
//  game.h
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#ifndef game_h
#define game_h

#pragma warning(disable : 4201)

#include "snake.h"
#include "level.h"
#include "direction.h"

#define MAX_SNAKE_COUNT 3

typedef enum {
    GAME_STATE_WAITING,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
} GameState;

typedef enum {
    QUERIED_OBJECT_TYPE_NONE,
    QUERIED_OBJECT_TYPE_CELL,
    QUERIED_OBJECT_TYPE_SNAKE,
} QueriedObjectType;

typedef struct {
    S32 index;
    S32 segment_index;
} QueriedSnake;

typedef struct {
    QueriedObjectType type;
    union {
        CellType cell;
        QueriedSnake snake;
    };
} QueriedObject;

typedef struct {
    U8* cells;
} PushState;

typedef struct {
    Level level;
    Snake snakes[MAX_SNAKE_COUNT];
    GameState state;
    S32 max_taco_count;
} Game;

bool game_init(Game* game, S32 level_width, S32 level_height, S32 max_taco_count);
void game_clone(Game* input, Game* output);
void game_apply_snake_action(Game* game, SnakeAction snake_action, S32 snake_index);
QueriedObject game_query(Game* game, S32 x, S32 y);
void game_update(Game* game, SnakeAction* snake_actions);
void game_destroy(Game* game);

void game_spawn_taco(Game* game);
S32 game_count_tacos(Game* game);

size_t game_serialize(const Game* game, void* buffer, size_t buffer_size);
size_t game_deserialize(void * buffer, size_t size, Game * out);

bool snake_segment_push(Game* game, PushState* push_state, S32 snake_index, S32 segment_index, Direction direction);
bool snake_segment_constrict(Game* game, S32 snake_index, S32 segment_index, bool left);

void init_push_state(Game* game, PushState* push_state);

#endif /* game_h */
