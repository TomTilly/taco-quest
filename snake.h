#ifndef snake_h
#define snake_h

#include "ints.h"
#include "direction.h"

#include <SDL2/SDL_render.h>
#include <stdbool.h>

#define INITIAL_SNAKE_LEN 5
#define ACTION_BUF_SIZE 2
#define SNAKE_SEGMENT_MAX_HEALTH 3
#define SNAKE_CHOMP_COOLDOWN 10
typedef U8 SnakeAction;

// Acts as a bitfield for actions to apply to a snake.
typedef enum {
    SNAKE_ACTION_NONE = 0,
    SNAKE_ACTION_FACE_NORTH = (1 << DIRECTION_NORTH), // 1
    SNAKE_ACTION_FACE_EAST = (1 << DIRECTION_EAST),   // 2
    SNAKE_ACTION_FACE_SOUTH = (1 << DIRECTION_SOUTH), // 4
    SNAKE_ACTION_FACE_WEST = (1 << DIRECTION_WEST),   // 8
    SNAKE_ACTION_CHOMP = 16,
} SnakeActionFlags;

typedef struct {
    S16 x;
    S16 y;
    S8 health;
} SnakeSegment;

typedef struct {
    SnakeSegment* segments;
    S32 length;
    S32 capacity; // I hate STL
    Direction direction;
    S8 chomp_cooldown;
} Snake;

typedef struct {
    int count;
    SnakeAction actions[ACTION_BUF_SIZE];
} ActionBuffer;

bool snake_init(Snake* snake, S32 capacity);
void snake_destroy(Snake* snake);

void snake_spawn(Snake* snake, S16 x, S16 y, Direction direction);
void snake_turn(Snake* snake, Direction direction);
void snake_draw(SDL_Renderer* renderer,
                SDL_Texture* texture,
                Snake* snake,
                int32_t cell_size);

size_t snake_serialize(const Snake* snake, void * buffer, size_t buffer_size);
size_t snake_deserialize(void * buffer, size_t size, Snake* out);

SnakeAction snake_action_from_direction(Direction direction);
bool snake_actions_are_opposite(SnakeAction action1, SnakeAction action2);
const char* snake_action_string(SnakeAction action);

void action_buffer_add(ActionBuffer * buf, SnakeAction action, Direction snake_current_direction);
SnakeAction action_buffer_remove(ActionBuffer * buf);

#endif /* snake_h */
