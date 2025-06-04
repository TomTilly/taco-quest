#include "snake.h"
#include "level.h"

#include <assert.h>
#include <stdio.h>

bool snake_init(Snake* snake, int32_t capacity) {
    snake->segments = calloc(capacity, sizeof(snake->segments[0]));
    if (snake->segments == NULL) {
        return false;
    }
    snake->capacity = capacity;
    snake->chomp_cooldown = 0;
    snake->constrict_state.index = -1;
    return true;
}

SnakeSegment snake_init_segment(S16 x, S16 y) {
    return (SnakeSegment){
        .x = x,
        .y = y,
        .health = SNAKE_SEGMENT_MAX_HEALTH
    };
}

void snake_spawn(Snake* snake, S16 x, S16 y, Direction direction) {
    snake->length = INITIAL_SNAKE_LEN;
    snake->direction = direction;

    for (int i = 0; i < snake->length; i++) {
        snake->segments[i] = snake_init_segment(x, y);
    }
}

void snake_turn(Snake* snake, Direction direction) {
    switch(direction) {
    case DIRECTION_NORTH:
        if (snake->direction != DIRECTION_SOUTH) {
            snake->direction = direction;
        }
        break;
    case DIRECTION_EAST:
        if (snake->direction != DIRECTION_WEST) {
            snake->direction = direction;
        }
        break;
    case DIRECTION_SOUTH:
        if (snake->direction != DIRECTION_NORTH) {
            snake->direction = direction;
        }
        break;
    case DIRECTION_WEST:
        if (snake->direction != DIRECTION_EAST) {
            snake->direction = direction;
        }
        break;
    default:
        break;
    }
}

void snake_draw(SDL_Renderer* renderer,
                SDL_Texture* texture,
                Snake* snake,
                S32 snake_index,
                S32 cell_size) {
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

                switch(shape) {
                case SNAKE_SEGMENT_SHAPE_VERTICAL:
                    source_rect.x = 32;
                    source_rect.y = 0;
                    angle = 90.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_HORIZONTAL:
                    source_rect.x = 32;
                    source_rect.y = 0;
                    break;
                case SNAKE_SEGMENT_SHAPE_NORTH_EAST_CORNER:
                    source_rect.x = 16;
                    source_rect.y = 0;
                    break;
                case SNAKE_SEGMENT_SHAPE_SOUTH_EAST_CORNER:
                    source_rect.x = 16;
                    source_rect.y = 0;
                    angle = 90.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_SOUTH_WEST_CORNER:
                    source_rect.x = 16;
                    source_rect.y = 0;
                    angle = 180.0;
                    break;
                case SNAKE_SEGMENT_SHAPE_NORTH_WEST_CORNER:
                    // corner top left
                    source_rect.x = 16;
                    source_rect.y = 0;
                    angle = 270.0;
                    break;
                default:
                    break;
                }
            }
        }

        source_rect.y += (snake->segments[i].health - 1) * source_rect.w;

        U8 hue = snake->chomp_cooldown ? 128 : 255;

        if (snake_index == 0) {
            SDL_SetTextureColorMod(texture, 0, hue, 0);
        } else if (snake_index == 1) {
            SDL_SetTextureColorMod(texture, hue, hue / 2, hue / 2);
        } else {
            SDL_SetTextureColorMod(texture, 0, hue, hue);
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
    free(snake->segments);
    memset(snake, 0, sizeof(*snake));
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
    default:
        break;
    }
    return "unknown";
}

void action_buffer_add(ActionBuffer * buf, SnakeAction action, Direction snake_current_direction) {
    if ( buf->count == ACTION_BUF_SIZE ) {
        return;
    }

    if (action == SNAKE_ACTION_NONE) {
        return;
    }

    SnakeAction prev_action = (buf->count == 0) ?
    snake_action_from_direction(snake_current_direction) :
    buf->actions[buf->count - 1];

    if ( action == prev_action ) {
        return; // Tried to press the same direction again, ignore
    }

    if ( snake_actions_are_opposite(action, prev_action) ) {
        return;
    }

    buf->actions[buf->count++] = action;
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
    if (segment_index >= snake->length) {
        return SNAKE_SEGMENT_SHAPE_UNKNOWN;
    }

    if (segment_index == 0) {
        return SNAKE_SEGMENT_SHAPE_HEAD;
    } else if (segment_index == (snake->length - 1)) {
        SnakeSegment* curr_segment = snake->segments + segment_index;
        SnakeSegment* prev_segment = curr_segment - 1;

        if (curr_segment->x == prev_segment->x) {
            return SNAKE_SEGMENT_SHAPE_VERTICAL;
        }
        return SNAKE_SEGMENT_SHAPE_HORIZONTAL;
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
        return SNAKE_SEGMENT_SHAPE_VERTICAL;
    } else if (has_east && has_west && !has_north && !has_south) {
        return SNAKE_SEGMENT_SHAPE_HORIZONTAL;
    } else if (has_east && has_north && !has_west && !has_south) {
        return SNAKE_SEGMENT_SHAPE_NORTH_EAST_CORNER;
    } else if (has_east && has_south && !has_west && !has_north) {
        return SNAKE_SEGMENT_SHAPE_SOUTH_EAST_CORNER;
    } else if (has_west && has_south && !has_east && !has_north) {
        return SNAKE_SEGMENT_SHAPE_SOUTH_WEST_CORNER;
    } else if (has_west && has_north && !has_east && !has_south) {
        return SNAKE_SEGMENT_SHAPE_NORTH_WEST_CORNER;
    }

    // TODO: Handle furled

    return SNAKE_SEGMENT_SHAPE_UNKNOWN;
}

Direction snake_segment_direction_to_head(Snake* snake, S32 segment_index) {
    if (segment_index >= snake->length ||
        segment_index == 0) {
        return DIRECTION_NONE;
    }

    SnakeSegment* curr_segment = snake->segments + segment_index;
    SnakeSegment* prev_segment = curr_segment - 1;

    if (curr_segment->x == prev_segment->x &&
        curr_segment->y == (prev_segment->y - 1)) {
        return DIRECTION_SOUTH;
    }
    if (curr_segment->x == prev_segment->x &&
        curr_segment->y == (prev_segment->y + 1)) {
        return DIRECTION_NORTH;
    }

    if (curr_segment->x == (prev_segment->x + 1) &&
        curr_segment->y == prev_segment->y) {
        return DIRECTION_WEST;
    }

    if (curr_segment->x == (prev_segment->x - 1) &&
        curr_segment->y == prev_segment->y) {
        return DIRECTION_EAST;
    }

    return DIRECTION_NONE;
}
