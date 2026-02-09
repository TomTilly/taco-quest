#ifndef snake_h
#define snake_h

#include "ints.h"
#include "direction.h"

#include <SDL3/SDL_render.h>
#include <stdbool.h>

#define INITIAL_SNAKE_LEN 5
#define ACTION_BUF_SIZE 2
#define SNAKE_KILL_DAMAGE_COOLDOWN 4
#define SNAKE_DAMAGE_FRAMES 3
#define CHOMP_POINT_CHECK_COUNT 3
#define ALL_SNAKE_ACTION_MOVEMENTS (SNAKE_ACTION_FACE_NORTH | SNAKE_ACTION_FACE_EAST | SNAKE_ACTION_FACE_WEST | SNAKE_ACTION_FACE_SOUTH)
#define SNAKE_CONSTRCT_ACTIONS (SNAKE_ACTION_CONSTRICT_LEFT | SNAKE_ACTION_CONSTRICT_RIGHT)
#define ALL_SNAKE_OTHER_ACTIONS (SNAKE_ACTION_CHOMP | SNAKE_CONSTRCT_ACTIONS | SNAKE_ACTION_LUNGE)

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
    SNAKE_ACTION_LUNGE = 128,
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
    bool face_north;
    bool face_west;
    bool face_south;
    bool face_east;
    bool chomp;
    bool constrict_left;
    bool constrict_right;
    bool lunge;
} SnakeActionKeyState;

typedef struct {
    SnakeSegmentShapeType type;
    bool flipped;
} SnakeSegmentShape;

typedef struct {
    S16 x;
    S16 y;
    S8 health;
    bool clamped;
    bool lunge_able;
} SnakeSegment;

typedef enum {
    SNAKE_CONSTRICT_STATE_NONE,
    SNAKE_CONSTRICT_STATE_LEFT,
    SNAKE_CONSTRICT_STATE_RIGHT,
} SnakeConstrictState;

typedef enum {
    SNAKE_LIFE_STATE_ALIVE,
    SNAKE_LIFE_STATE_DEAD,
} SnakeLifeState;

typedef enum {
    SNAKE_COLOR_RED,
    SNAKE_COLOR_YELLOW,
    SNAKE_COLOR_GREEN,
    SNAKE_COLOR_CYAN,
    SNAKE_COLOR_BLUE,
    SNAKE_COLOR_PURPLE,
    SNAKE_COLOR_COUNT,
} SnakeColor;

typedef enum {
    SNAKE_CHOMP_STATE_NONE,
    SNAKE_CHOMP_STATE_CLAMPING,
    SNAKE_CHOMP_STATE_BITE,
} SnakeChompState;

typedef struct {
    SnakeSegment* segments;
    S32 length;
    S32 capacity; // I hate STL
    Direction direction;
    S8 tacos_for_chomp;
    S8 chomp_count;
    SnakeChompState chomp_state;
    S8 chomp_cooldown;
    bool chomp_canceled;
    S8 kill_damage_cooldown;
    SnakeLifeState life_state;
    SnakeConstrictState constrict_state;
    SnakeColor color;
} Snake;

typedef struct {
    int count;
    SnakeAction actions[ACTION_BUF_SIZE];
    bool chomp_reset;
} ActionBuffer;

bool snake_init(Snake* snake, S32 capacity);
void snake_clone(Snake* new_snake, Snake* to_be_cloned);
void snake_destroy(Snake* snake);

void snake_spawn(Snake* snake,
                 S16 x,
                 S16 y,
                 Direction direction,
                 S32 length,
                 S8 segment_health);
void snake_turn(Snake* snake, Direction direction);
void snake_draw_head(SDL_Renderer* renderer,
                     SDL_Texture* texture,
                     Snake* snake,
                     S32 cell_size,
                     S32 camera_offset_x,
                     S32 camera_offset_y,
                     S32 max_segment_health,
                     S32 frame_tick);
void snake_draw_body(SDL_Renderer* renderer,
                     SDL_Texture* texture,
                     Snake* snake,
                     S32 cell_size,
                     S32 camera_offset_x,
                     S32 camera_offset_y,
                     S32 max_segment_health,
                     S32 frame_tick);

size_t snake_serialize(const Snake* snake, void * buffer, size_t buffer_size);
size_t snake_deserialize(void * buffer, size_t size, Snake* out);

SnakeAction snake_action_from_direction(Direction direction);
bool snake_actions_are_opposite(SnakeAction action1, SnakeAction action2);
const char* snake_action_string(SnakeAction action);
void print_snake_action(SnakeAction action);
SnakeAction snake_other_action_highest_priority(SnakeAction action);
void snake_action_handle_keystate(const bool* keyboard_state,
                                  SnakeActionKeyState* prev_action_key_state,
                                  SnakeAction* actions);

const char* snake_color_string(SnakeColor color);

void action_buffer_add(ActionBuffer * buf, SnakeAction action);
SnakeAction action_buffer_remove(ActionBuffer * buf);

SnakeSegmentShape snake_segment_shape(Snake* snake, S32 segment_index);
Direction snake_segment_direction_to_head(Snake* snake, S32 segment_index);
Direction snake_segment_direction_to_tail(Snake* snake, S32 segment_index);

#endif /* snake_h */
