#include "snake.h"

#include <assert.h>

bool snake_init(Snake* snake, int32_t capacity) {
    snake->segments = calloc(capacity, sizeof(snake->segments[0]));
    if (snake->segments == NULL) {
        return false;
    }
    snake->capacity = capacity;
    return true;
}

void snake_spawn(Snake* snake, int x, int y, Direction direction) {
    snake->length = INITIAL_SNAKE_LEN;
    snake->direction = direction;

    for (int i = 0; i < snake->length; i++) {
        snake->segments[i].x = x;
        snake->segments[i].y = y;
    }
}

void snake_grow(Snake* snake) {
    assert(snake->length < snake->capacity);

    int last_segment_index = snake->length - 1;
    int new_segment_index = snake->length;
    snake->segments[new_segment_index].x = snake->segments[last_segment_index].x;
    snake->segments[new_segment_index].y = snake->segments[last_segment_index].y;
    snake->length++;
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

void snake_draw(SDL_Renderer* renderer, Snake* snake, int32_t cell_size) {
    for (int i = 0; i < snake->length; i++) {
        SDL_SetRenderDrawColor(renderer, 0, (i == 0) ? 176 : 255, 0, 255);

        SDL_Rect r = {
            .x = snake->segments[i].x * cell_size,
            .y = snake->segments[i].y * cell_size,
            .w = cell_size,
            .h = cell_size
        };

        SDL_RenderFillRect(renderer, &r);
    }
}

void snake_destroy(Snake* snake) {
    free(snake->segments);
    memset(snake, 0, sizeof(*snake));
}

size_t snake_serialize(const Snake* snake, void* buffer, size_t buffer_size) {
    // Segments plus the direction as a single byte.
    size_t segments_size = (snake->length * sizeof(*snake->segments));
    size_t total_size = sizeof(snake->length) + segments_size + 1;
    assert(total_size <= buffer_size && "buffer too small!");

    U8 * ptr = buffer;

    memcpy(ptr, &snake->length, sizeof(snake->length));
    ptr += sizeof(snake->length);

    memcpy(ptr, snake->segments, segments_size);
    ptr += segments_size;

    *ptr = (U8)snake->direction;

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

    return ptr - (U8 *)buffer;
}
