#ifndef snake_h
#define snake_h

#include "ints.h"
#include "direction.h"

#include <SDL2/SDL_render.h>
#include <stdbool.h>

#define INITIAL_SNAKE_LEN 3

typedef U8 SnakeAction;

// Acts as a bitfield for actions to apply to a snake.
typedef enum {
    SNAKE_ACTION_NONE = 0,
    SNAKE_ACTION_FACE_NORTH = (1 << DIRECTION_NORTH), // 1
    SNAKE_ACTION_FACE_EAST = (1 << DIRECTION_EAST),   // 2
    SNAKE_ACTION_FACE_SOUTH = (1 << DIRECTION_SOUTH), // 4
    SNAKE_ACTION_FACE_WEST = (1 << DIRECTION_WEST),   // 8
} SnakeActionFlags;

typedef struct {
    int x;
    int y;
} SnakeSegment;

typedef struct {
    SnakeSegment* segments;
    S32 length;
    S32 capacity; // I hate STL
    Direction direction;
} Snake;

bool snake_init(Snake* snake, S32 capacity);
void snake_spawn(Snake* snake, int x, int y, Direction direction);
void snake_grow(Snake* snake);
void snake_turn(Snake* snake, Direction direction);
void snake_draw(SDL_Renderer* renderer, Snake* snake, int32_t cell_size);
void snake_destroy(Snake* snake);
size_t snake_serialize(const Snake* snake, void * buffer, size_t buffer_size);
size_t snake_deserialize(void * buffer, size_t size, Snake* out);
Direction get_direction(SnakeAction action);
bool snake_actions_are_opposite(SnakeAction action1, SnakeAction action2);

#endif /* snake_h */
