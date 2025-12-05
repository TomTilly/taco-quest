#include "snake.h"
#include "level.h"

#include <assert.h>
#include <stdio.h>

#include <SDL2/SDL_scancode.h>

bool snake_init(Snake* snake, int32_t capacity) {
    snake->segments = calloc(capacity, sizeof(snake->segments[0]));
    if (snake->segments == NULL) {
        return false;
    }
    snake->capacity = capacity;
    return true;
}

SnakeSegment snake_init_segment(S16 x, S16 y, S8 segment_health) {
    return (SnakeSegment){
        .x = x,
        .y = y,
        .health = segment_health
    };
}

void snake_spawn(Snake* snake,
                 S16 x,
                 S16 y,
                 Direction direction,
                 S32 length,
                 S8 segment_health) {
    snake->length = length;
    snake->direction = direction;
    snake->chomp_cooldown = 0;
    snake->kill_damage_cooldown = 0;
    snake->life_state = SNAKE_LIFE_STATE_ALIVE;

    for (int i = 0; i < snake->length; i++) {
        snake->segments[i] = snake_init_segment(x, y, segment_health);
    }
}

void snake_turn(Snake* snake, Direction direction) {
    if (direction >= DIRECTION_COUNT ||
        direction == snake_segment_direction_to_tail(snake, 0)) {
        return;
    }

    snake->direction = direction;
}

void snake_draw(SDL_Renderer* renderer,
                SDL_Texture* texture,
                Snake* snake,
                S32 cell_size,
                S32 max_segment_health) {
    int tail_index = snake->length - 1;
    for (int i = 0; i < snake->length; i++) {
        SDL_Rect dest_rect = {
            .x = snake->segments[i].x * cell_size,
            .y = snake->segments[i].y * cell_size,
            .w = cell_size,
            .h = cell_size
        };

        double angle = 0.0;

        SDL_Rect source_rect = {0};

        source_rect.w = 16;
        source_rect.h = 16;

        if (i == 0) {
            source_rect.x = 48;
            source_rect.y = 0;
            angle = 90.0 * snake->direction;
        } else {
            // detect straight vs corner vs tail
            if (i == tail_index) {
                // tail
                source_rect.x = 0;
                source_rect.y = 0;

                int last_segment_x = snake->segments[i - 1].x;
                int last_segment_y = snake->segments[i - 1].y;

                if (snake->segments[i].y == last_segment_y &&
                    snake->segments[i].x == (last_segment_x - 1)) {
                    // east
                    angle = 90.0;
                } else if (snake->segments[i].y == (last_segment_y - 1) &&
                           snake->segments[i].x == last_segment_x) {
                    // south
                    angle = 180.0;
                } else if (snake->segments[i].y == last_segment_y &&
                           snake->segments[i].x == (last_segment_x + 1)) {
                    // west
                    angle = 270.0;
                }
            } else {
                SnakeSegmentShape shape = snake_segment_shape(snake, i);

                source_rect.y = shape.flipped ? 0 : 16;

                switch(shape.type) {
                case SNAKE_SEGMENT_SHAPE_TYPE_VERTICAL:
                    source_rect.x = 32;
                    angle = 90.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_TYPE_HORIZONTAL:
                    source_rect.x = 32;
                    break;
                case SNAKE_SEGMENT_SHAPE_TYPE_NORTH_EAST_CORNER:
                    source_rect.x = 16;
                    break;
                case SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_EAST_CORNER:
                    source_rect.x = 16;
                    angle = 90.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_WEST_CORNER:
                    source_rect.x = 16;
                    angle = 180.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_TYPE_NORTH_WEST_CORNER:
                    // corner top left
                    source_rect.x = 16;
                    angle = 270.0;
                    break;
                default:
                    break;
                }
            }
        }

        S32 health_frame = 0;
        if (snake->segments[i].health < max_segment_health) {
            health_frame = 2 - (S32)(((float)(snake->segments[i].health) / (float)(max_segment_health)) * 2.0);
        }

        source_rect.y += (2 * health_frame * source_rect.h);

        U8 hue = snake->chomp_cooldown ? 128 : 255;

        switch (snake->color) {
        case SNAKE_COLOR_RED:
            SDL_SetTextureColorMod(texture, hue, 0, 0);
            break;
        case SNAKE_COLOR_YELLOW:
            SDL_SetTextureColorMod(texture, hue, hue, 0);
            break;
        case SNAKE_COLOR_GREEN:
            SDL_SetTextureColorMod(texture, 0, hue, 0);
            break;
        case SNAKE_COLOR_CYAN:
            SDL_SetTextureColorMod(texture, 0, hue, hue);
            break;
        case SNAKE_COLOR_BLUE:
            SDL_SetTextureColorMod(texture, 0, 0, hue);
            break;
        case SNAKE_COLOR_PURPLE:
            SDL_SetTextureColorMod(texture, hue, 0, hue);
            break;
        default:
            break;
        }

        int rc = SDL_RenderCopyEx(renderer,
                                  texture,
                                  &source_rect,
                                  &dest_rect,
                                  angle,
                                  NULL,
                                  SDL_FLIP_NONE);
        if (rc != 0) {
            fprintf(stderr, "Tom F was wrong: %s\n", SDL_GetError());
            return;
        }
    }
}

void snake_destroy(Snake* snake) {
    if (snake->segments != NULL) {
        free(snake->segments);
        memset(snake, 0, sizeof(*snake));
    }
}

size_t snake_serialize(const Snake* snake, void* buffer, size_t buffer_size) {
    // Segments plus the direction as a single byte.
    size_t segments_size = (snake->length * sizeof(*snake->segments));

    // Total size of snake buffer as sent over network.
    size_t total_size = 0;
    total_size += sizeof(snake->length);
    total_size += segments_size;
    total_size += sizeof(U8); // direction
    total_size += sizeof(U8); // chomp cooldown
    total_size += sizeof(SnakeColor);

    assert(total_size <= buffer_size && "buffer too small!");

    U8 * ptr = buffer;

    memcpy(ptr, &snake->length, sizeof(snake->length));
    ptr += sizeof(snake->length);

    memcpy(ptr, snake->segments, segments_size);
    ptr += segments_size;

    *ptr = (U8)snake->direction;
    ptr += sizeof(U8);

    *ptr = snake->chomp_cooldown;
    ptr += sizeof(snake->chomp_cooldown);

    *ptr = snake->color;
    ptr += sizeof(snake->color);

    return total_size;
}

size_t snake_deserialize(void * buffer, size_t size, Snake* out) {
    U8 * ptr = buffer;

    assert(size >= sizeof(out->length)
           && "buffer size too smol for snake segment length!");

    S32 length = *(S32 *)ptr;
    ptr += sizeof(out->length);
    size -= sizeof(out->length);

    if ( out->length != length) {
        // TODO: a proper snake function to set the length.
        if ( length > out->length ) {
            snake_destroy(out);
            bool success = snake_init(out, length);
            assert(success && "snake_init() failed!");
        }
        out->length = length;
    }

    size_t segments_size = length * sizeof(*out->segments);
    assert(size >= segments_size && "buffer size too smol for snake segments");

    memcpy(out->segments, ptr, segments_size);
    ptr += segments_size;
    size -= segments_size;

    out->direction = (Direction)*ptr;
    ptr++;

    out->chomp_cooldown = *ptr;
    ptr++;

    out->color = *ptr;
    ptr += sizeof(out->color);

    return ptr - (U8 *)buffer;
}

SnakeAction snake_action_from_direction(Direction direction) {
    switch ( direction ) {
        case DIRECTION_NONE:
            return SNAKE_ACTION_NONE;
        case DIRECTION_NORTH:
            return SNAKE_ACTION_FACE_NORTH;
        case DIRECTION_SOUTH:
            return SNAKE_ACTION_FACE_SOUTH;
        case DIRECTION_EAST:
            return SNAKE_ACTION_FACE_EAST;
        case DIRECTION_WEST:
            return SNAKE_ACTION_FACE_WEST;
        default:
            return SNAKE_ACTION_NONE;
    }
}

bool snake_actions_are_opposite(SnakeAction action1, SnakeAction action2)
{
    return (action1 == SNAKE_ACTION_FACE_NORTH && action2 == SNAKE_ACTION_FACE_SOUTH)
    || (action1 == SNAKE_ACTION_FACE_SOUTH && action2 == SNAKE_ACTION_FACE_NORTH)
    || (action1 == SNAKE_ACTION_FACE_EAST && action2 == SNAKE_ACTION_FACE_WEST)
    || (action1 == SNAKE_ACTION_FACE_WEST && action2 == SNAKE_ACTION_FACE_EAST);
}

const char* snake_action_string(SnakeAction action) {
    switch (action) {
    case SNAKE_ACTION_NONE:
        return "none";
    case SNAKE_ACTION_FACE_NORTH:
        return "north";
    case SNAKE_ACTION_FACE_EAST:
        return "east";
    case SNAKE_ACTION_FACE_SOUTH:
        return "south";
    case SNAKE_ACTION_FACE_WEST:
        return "west";
    case SNAKE_ACTION_CHOMP:
        return "chomp";
    case SNAKE_ACTION_CONSTRICT_LEFT:
        return "constrict left";
    case SNAKE_ACTION_CONSTRICT_RIGHT:
        return "constrict right";
    default:
        break;
    }
    return "unknown";
}

void print_snake_action(SnakeAction action) {
    if (action == 0) {
        printf("%s\n", snake_action_string(action));
    }

    for (S32 i = 0; i < 8; i++) {
        U8 action_to_check = 1 << i;
        if (action_to_check & action) {
            printf("%s | ", snake_action_string(action_to_check));
        }
    }
    printf("\n");
}

void action_buffer_add(ActionBuffer * buf, SnakeAction actions) {
    if (buf->count == ACTION_BUF_SIZE) {
        return;
    }

    if (actions == SNAKE_ACTION_NONE) {
        return;
    }

    // filter out previous actions in the buffer.
    SnakeAction prev_actions = {0};
    for (S32 i = 0; i < buf->count; i++) {
        prev_actions |= buf->actions[i];
    }
    SnakeAction filtered_action = actions & (~prev_actions);

    if (filtered_action == 0) {
        return; // Tried to press the same direction again, ignore
    }

    SnakeAction prioritized_action = snake_action_highest_priority(filtered_action);
    buf->actions[buf->count++] = prioritized_action;
}

SnakeAction snake_action_highest_priority(SnakeAction action) {
    for (S32 i = 0; i < 8; i++) {
        U8 action_to_check = 1 << i;
        if (action_to_check & action) {
            return (SnakeAction)(action_to_check);
        }
    }
    return SNAKE_ACTION_NONE;
}

void snake_action_handle_keystate(const U8* keyboard_state,
                                  SnakeActionKeyState* prev_snake_actions_key_state,
                                  SnakeAction* snake_actions) {
    SnakeActionKeyState current_snake_actions_key_state = {0};
    current_snake_actions_key_state.face_north = keyboard_state[SDL_SCANCODE_W];
    current_snake_actions_key_state.face_west = keyboard_state[SDL_SCANCODE_A];
    current_snake_actions_key_state.face_south = keyboard_state[SDL_SCANCODE_S];
    current_snake_actions_key_state.face_east = keyboard_state[SDL_SCANCODE_D];
    current_snake_actions_key_state.chomp = keyboard_state[SDL_SCANCODE_SPACE];
    current_snake_actions_key_state.constrict_left = keyboard_state[SDL_SCANCODE_Q];
    current_snake_actions_key_state.constrict_right = keyboard_state[SDL_SCANCODE_E];

    // Actions are triggered on the press event.
    if (!prev_snake_actions_key_state->face_north && current_snake_actions_key_state.face_north) {
        *snake_actions |= SNAKE_ACTION_FACE_NORTH;
    }

    if (!prev_snake_actions_key_state->face_west && current_snake_actions_key_state.face_west) {
        *snake_actions |= SNAKE_ACTION_FACE_WEST;
    }

    if (!prev_snake_actions_key_state->face_south && current_snake_actions_key_state.face_south) {
        *snake_actions |= SNAKE_ACTION_FACE_SOUTH;
    }

    if (!prev_snake_actions_key_state->face_east && current_snake_actions_key_state.face_east) {
        *snake_actions |= SNAKE_ACTION_FACE_EAST;
    }

    if (!prev_snake_actions_key_state->chomp && current_snake_actions_key_state.chomp) {
        *snake_actions |= SNAKE_ACTION_CHOMP;
    }

    // Constricting acts different, where you can hold it down.
    if (current_snake_actions_key_state.constrict_left) {
        *snake_actions |= SNAKE_ACTION_CONSTRICT_LEFT;
    }

    if (current_snake_actions_key_state.constrict_right) {
        *snake_actions |= SNAKE_ACTION_CONSTRICT_RIGHT;
    }

    *prev_snake_actions_key_state = current_snake_actions_key_state;
}

const char* snake_color_string(SnakeColor color) {
    switch (color) {
    case SNAKE_COLOR_RED:
        return "red";
    case SNAKE_COLOR_YELLOW:
        return "yellow";
    case SNAKE_COLOR_GREEN:
        return "green";
    case SNAKE_COLOR_CYAN:
        return "cyan";
    case SNAKE_COLOR_BLUE:
        return "blue";
    case SNAKE_COLOR_PURPLE:
        return "purple";
    default:
        break;
    }
    return "unknown";
}

SnakeAction action_buffer_remove(ActionBuffer * buf) {
    SnakeAction action = SNAKE_ACTION_NONE;
    if ( buf->count > 0 ) {
        // grab the first element.
        action = buf->actions[0];
        // shift over the all the elements overwriting the first element.
        for ( int i = 0; i < buf->count - 1; i++ ) {
            buf->actions[i] = buf->actions[i + 1];
        }
        buf->count--;
    }

    return action;
}

SnakeSegmentShape snake_segment_shape(Snake* snake, S32 segment_index) {
    SnakeSegmentShape result = {0};
    if (segment_index >= snake->length) {
        return result;
    }

    if (segment_index == 0) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_HEAD;
        return result;
    } else if (segment_index == (snake->length - 1)) {
        SnakeSegment* curr_segment = snake->segments + segment_index;
        SnakeSegment* prev_segment = curr_segment - 1;

        if (curr_segment->x == prev_segment->x) {
            result.type = SNAKE_SEGMENT_SHAPE_TYPE_VERTICAL;
            result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_SOUTH);
            return result;
        }

        result.type = SNAKE_SEGMENT_SHAPE_TYPE_HORIZONTAL;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_EAST);
        return result;
    }

    SnakeSegment* curr_segment = snake->segments + segment_index;
    SnakeSegment* prev_segment = curr_segment - 1;
    SnakeSegment* next_segment = curr_segment + 1;

    bool has_east = (curr_segment->x == (prev_segment->x - 1) &&
                     curr_segment->y == prev_segment->y) ||
                    (curr_segment->x == (next_segment->x - 1) &&
                     curr_segment->y == next_segment->y);
    bool has_west = (curr_segment->x == (prev_segment->x + 1) &&
                      curr_segment->y == prev_segment->y) ||
                     (curr_segment->x == (next_segment->x + 1) &&
                      curr_segment->y == next_segment->y);
    bool has_north = (curr_segment->y == (prev_segment->y + 1) &&
                     curr_segment->x == prev_segment->x) ||
                     (curr_segment->y == (next_segment->y + 1) &&
                     curr_segment->x == next_segment->x);
    bool has_south = (curr_segment->y == (prev_segment->y - 1) &&
                     curr_segment->x == prev_segment->x) ||
                     (curr_segment->y == (next_segment->y - 1) &&
                     curr_segment->x == next_segment->x);

    if (has_north && has_south && !has_east && !has_west) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_VERTICAL;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_SOUTH);
        return result;
    } else if (has_east && has_west && !has_north && !has_south) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_HORIZONTAL;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_EAST);
        return result;
    } else if (has_east && has_north && !has_west && !has_south) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_NORTH_EAST_CORNER;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_EAST);
        return result;
    } else if (has_east && has_south && !has_west && !has_north) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_EAST_CORNER;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_SOUTH);
        return result;
    } else if (has_west && has_south && !has_east && !has_north) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_WEST_CORNER;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_WEST);
        return result;
    } else if (has_west && has_north && !has_east && !has_south) {
        result.type = SNAKE_SEGMENT_SHAPE_TYPE_NORTH_WEST_CORNER;
        result.flipped = (snake_segment_direction_to_head(snake, segment_index) == DIRECTION_NORTH);
        return result;
    }

    // TODO: Handle furled
    return result;
}

Direction _direction_between_segments(SnakeSegment* first, SnakeSegment* second) {
    if (first->x == second->x && first->y == (second->y - 1)) {
        return DIRECTION_SOUTH;
    }
    if (first->x == second->x && first->y == (second->y + 1)) {
        return DIRECTION_NORTH;
    }

    if (first->x == (second->x + 1) && first->y == second->y) {
        return DIRECTION_WEST;
    }

    if (first->x == (second->x - 1) && first->y == second->y) {
        return DIRECTION_EAST;
    }

    return DIRECTION_NONE;
}

Direction snake_segment_direction_to_head(Snake* snake, S32 segment_index) {
    if (segment_index >= snake->length) {
        return DIRECTION_NONE;
    }

    if (segment_index == 0) {
        return opposite_direction(snake_segment_direction_to_tail(snake, segment_index));
    }

    SnakeSegment* curr_segment = snake->segments + segment_index;
    SnakeSegment* prev_segment = curr_segment - 1;

    return _direction_between_segments(curr_segment, prev_segment);
}

Direction snake_segment_direction_to_tail(Snake* snake, S32 segment_index) {
    // Check out of bounds but also check for tail, since there is no direction from the tail to
    // itself.
    if (snake->length == 0) {
        return DIRECTION_NONE;
    }

    SnakeSegment* curr_segment = snake->segments + segment_index;

    if(segment_index >= (snake->length - 1)) {
        if (snake->length == 1) {
            return opposite_direction(snake->direction);
        }

        SnakeSegment* prev_segment = snake->segments + (segment_index - 1);
        return opposite_direction(_direction_between_segments(curr_segment, prev_segment));
    }

    SnakeSegment* next_segment = curr_segment + 1;
    return _direction_between_segments(curr_segment, next_segment);
}
