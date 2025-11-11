#ifndef snake_h
#define snake_h

#include "ints.h"
#include "direction.h"

#include <SDL2/SDL_render.h>
#include <stdbool.h>

#define INITIAL_SNAKE_LEN 5
#define ACTION_BUF_SIZE 2
#define SNAKE_SEGMENT_MAX_HEALTH 2
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
    SNAKE_ACTION_CONSTRICT_LEFT = 32,
    SNAKE_ACTION_CONSTRICT_RIGHT = 64,
} SnakeActionFlags;

typedef enum {
    SNAKE_SEGMENT_SHAPE_TYPE_UNKNOWN,
    SNAKE_SEGMENT_SHAPE_TYPE_HEAD,
    SNAKE_SEGMENT_SHAPE_TYPE_VERTICAL,
    SNAKE_SEGMENT_SHAPE_TYPE_HORIZONTAL,
    SNAKE_SEGMENT_SHAPE_TYPE_NORTH_WEST_CORNER,
    SNAKE_SEGMENT_SHAPE_TYPE_NORTH_EAST_CORNER,
    SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_WEST_CORNER,
    SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_EAST_CORNER,
} SnakeSegmentShapeType;

typedef struct {
    SnakeSegmentShapeType type;
    bool flipped;
} SnakeSegmentShape;

typedef struct {
    S16 x;
    S16 y;
    S8 health;
} SnakeSegment;

typedef enum {
    SNAKE_CONSTRICT_STATE_NONE,
    SNAKE_CONSTRICT_STATE_LEFT,
    SNAKE_CONSTRICT_STATE_RIGHT,
} SnakeConstrictState;

typedef enum {
    SNAKE_LIFE_STATE_ALIVE,
    SNAKE_LIFE_STATE_DYING,
    SNAKE_LIFE_STATE_DEAD,
} SnakeLifeState;

typedef struct {
    SnakeSegment* segments;
    S32 length;
    S32 capacity; // I hate STL
    Direction direction;
    S8 chomp_cooldown;
    SnakeLifeState life_state;
    SnakeConstrictState constrict_state;
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
                S32 snake_index,
                S32 cell_size);

size_t snake_serialize(const Snake* snake, void * buffer, size_t buffer_size);
size_t snake_deserialize(void * buffer, size_t size, Snake* out);

SnakeAction snake_action_from_direction(Direction direction);
bool snake_actions_are_opposite(SnakeAction action1, SnakeAction action2);
const char* snake_action_string(SnakeAction action);

void action_buffer_add(ActionBuffer * buf, SnakeAction action, Direction snake_current_direction);
SnakeAction action_buffer_remove(ActionBuffer * buf);

SnakeSegmentShape snake_segment_shape(Snake* snake, S32 segment_index);
Direction snake_segment_direction_to_head(Snake* snake, S32 segment_index);
Direction snake_segment_direction_to_tail(Snake* snake, S32 segment_index);

#endif /* snake_h */
