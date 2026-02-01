#include "snake.h"

#include <assert.h>
#include <stdio.h>

#include <SDL3/SDL_scancode.h>

bool snake_init(Snake* snake, int32_t capacity) {
    snake->segments = calloc(capacity, sizeof(snake->segments[0]));
    if (snake->segments == NULL) {
        return false;
    }
    snake->capacity = capacity;
    return true;
}

void snake_clone(Snake* new_snake, Snake* to_be_cloned) {
    snake_destroy(new_snake);
    snake_init(new_snake, to_be_cloned->capacity);
    new_snake->length = to_be_cloned->length;
    for (S32 i = 0; i < to_be_cloned->length; i++) {
        memcpy(new_snake->segments + i,
               to_be_cloned->segments + i,
               sizeof(new_snake->segments[i]));
    }
    new_snake->direction = to_be_cloned->direction;
    new_snake->chomp_state = to_be_cloned->chomp_state;
    new_snake->chomp_cooldown = to_be_cloned->chomp_cooldown;
    new_snake->kill_damage_cooldown = to_be_cloned->kill_damage_cooldown;
    new_snake->life_state = to_be_cloned->life_state;
    new_snake->constrict_state = to_be_cloned->constrict_state;
    new_snake->color = to_be_cloned->color;
    // This check should help us detect if more fields need to be added here.
    S32 snake_size = sizeof(Snake);
    assert(snake_size == 40);
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
    snake->chomp_state = SNAKE_CHOMP_STATE_NONE;
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

void _set_snake_texture_color_mod(Snake* snake, SDL_Texture* texture) {
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
}

float _calculate_snake_health_source_y_offset(Snake* snake, S32 segment_index, S32 max_segment_health, float frame_height) {
    const S32 MAX_HEALTH_FRAME = 2;
    S32 health_frame = 0;
    if (snake->segments[segment_index].health < max_segment_health) {
        health_frame = MAX_HEALTH_FRAME -
            (S32)(((float)(snake->segments[segment_index].health) /
                  (float)(max_segment_health)) * 2.0);
    }

    return (float)(MAX_HEALTH_FRAME * health_frame * frame_height);
}

void snake_draw_head(SDL_Renderer* renderer,
                     SDL_Texture* texture,
                     Snake* snake,
                     S32 cell_size,
                     S32 camera_offset_x,
                     S32 camera_offset_y,
                     S32 max_segment_health) {
    SDL_FRect dest_rect = {
        .x = (float)(camera_offset_x + snake->segments[0].x * cell_size),
        .y = (float)(camera_offset_y + snake->segments[0].y * cell_size),
        .w = (float)(cell_size),
        .h = (float)(cell_size)
    };

    double angle = 0.0;

    SDL_FRect source_rect = {0};

    source_rect.w = 16.0f;
    source_rect.h = 16.0f;

    source_rect.x = 80.0f;
    source_rect.y = 0.0f;
    angle = 90.0 * snake->direction;

    source_rect.y += _calculate_snake_health_source_y_offset(snake, 0, max_segment_health, source_rect.h);

    SDL_FPoint texture_center = {0};
    texture_center.x = (float)(cell_size) / 2;
    texture_center.y = (float)(cell_size) / 2;

    if (snake->chomp_state != SNAKE_CHOMP_STATE_NONE) {
        source_rect.x = 96.0f;
        source_rect.h = 32.0f;
        dest_rect.h = cell_size * 2.0f;
        dest_rect.y -= cell_size;
        texture_center.y = (float)(cell_size) * 1.5f;
    }

    _set_snake_texture_color_mod(snake, texture);

    bool result = SDL_RenderTextureRotated(renderer,
                                           texture,
                                           &source_rect,
                                           &dest_rect,
                                           angle,
                                           &texture_center,
                                           SDL_FLIP_NONE);
    if (!result) {
        fprintf(stderr, "Tom F was wrong: %s\n", SDL_GetError());
        return;
    }
}

void snake_draw_body(SDL_Renderer* renderer,
                     SDL_Texture* texture,
                     Snake* snake,
                     S32 cell_size,
                     S32 camera_offset_x,
                     S32 camera_offset_y,
                     S32 max_segment_health) {
    int tail_index = snake->length - 1;
    for (int i = 1; i < snake->length; i++) {
        SDL_FRect dest_rect = {
            .x = (float)(camera_offset_x + snake->segments[i].x * cell_size),
            .y = (float)(camera_offset_y + snake->segments[i].y * cell_size),
            .w = (float)(cell_size),
            .h = (float)(cell_size)
        };

        double angle = 0.0;

        SDL_FRect source_rect = {0};

        source_rect.w = 16;
        source_rect.h = 16;

        // detect straight vs corner vs tail
        if (i == tail_index) {
            // tail
            source_rect.x = 0.0f;
            source_rect.y = 0.0f;

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

            source_rect.y = shape.flipped ? 0.0f : 16.0f;

            switch(shape.type) {
            case SNAKE_SEGMENT_SHAPE_TYPE_VERTICAL:
                source_rect.x = 32.0f;
                angle = 90.0;
                break;
            case SNAKE_SEGMENT_SHAPE_TYPE_HORIZONTAL:
                source_rect.x = 32.0f;
                break;
            case SNAKE_SEGMENT_SHAPE_TYPE_NORTH_EAST_CORNER:
                source_rect.x = 16.0f;
                break;
            case SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_EAST_CORNER:
                source_rect.x = 16.0f;
                angle = 90.0;
                break;
            case SNAKE_SEGMENT_SHAPE_TYPE_SOUTH_WEST_CORNER:
                source_rect.x = 16.0f;
                angle = 180.0;
                break;
            case SNAKE_SEGMENT_SHAPE_TYPE_NORTH_WEST_CORNER:
                // corner top left
                source_rect.x = 16.0f;
                angle = 270.0;
                break;
            default:
                break;
            }

            if (snake->segments[i].x == snake->segments[i - 1].x &&
                snake->segments[i].y == snake->segments[i - 1].y) {
                source_rect.x += 32.0f;
            }
        }

        source_rect.y += _calculate_snake_health_source_y_offset(snake, i, max_segment_health, source_rect.h);

        _set_snake_texture_color_mod(snake, texture);

        bool result = SDL_RenderTextureRotated(renderer,
                                          texture,
                                          &source_rect,
                                          &dest_rect,
                                          angle,
                                          NULL,
                                          SDL_FLIP_NONE);
        if (!result) {
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
    total_size += sizeof(SnakeChompState); // chomp state
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

    *ptr = snake->chomp_state;
    ptr += sizeof(snake->chomp_state);

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

    out->chomp_state = *ptr;
    ptr += sizeof(out->chomp_state);

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

    // filter out previous movements in the buffer.
    SnakeAction prev_movement_actions = {0};
    for (S32 i = 0; i < buf->count; i++) {
        prev_movement_actions |= (buf->actions[i] & ALL_SNAKE_ACTION_MOVEMENTS);
    }
    SnakeAction filtered_actions = actions & (~prev_movement_actions);

    if (filtered_actions == 0) {
        return; // Tried to press the same direction again, ignore
    }

    // Separate movement to queue after the current action.
    SnakeAction movement_actions = (filtered_actions & ALL_SNAKE_ACTION_MOVEMENTS);
    buf->actions[buf->count] = movement_actions;
    buf->count++;

    // Other actions are defined as non-movement actions:
    // - chomping, lunging, constricting
    // The highest priority other action overwrites the current other action while preserving the
    // current movement.
    SnakeAction prioritized_other_actions = snake_other_action_highest_priority(
        (filtered_actions & ALL_SNAKE_OTHER_ACTIONS) | (buf->actions[0] & ALL_SNAKE_OTHER_ACTIONS));
    buf->actions[0] = prioritized_other_actions | (buf->actions[0] & ALL_SNAKE_ACTION_MOVEMENTS);
}

SnakeAction snake_other_action_highest_priority(SnakeAction actions) {
    if (actions & SNAKE_ACTION_LUNGE) {
        // Lunging can only occur on its own.
        return SNAKE_ACTION_LUNGE;
    }
    // Including both constrict actions, cancels out.
    if ((actions & SNAKE_CONSTRCT_ACTIONS) == SNAKE_CONSTRCT_ACTIONS) {
        actions &= ~SNAKE_CONSTRCT_ACTIONS;
    }
    // Chomping and a single constrict are compatible, as we want people who are clamping to be
    // able to constrict.
    return actions & ALL_SNAKE_OTHER_ACTIONS;
}

void snake_action_handle_keystate(const bool* keyboard_state,
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
    current_snake_actions_key_state.lunge = keyboard_state[SDL_SCANCODE_LCTRL];

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

    if (!prev_snake_actions_key_state->lunge && current_snake_actions_key_state.lunge) {
        *snake_actions |= SNAKE_ACTION_LUNGE;
    }

    // Constricting and chomping act different, where you can hold it down.
    if (current_snake_actions_key_state.chomp) {
        *snake_actions |= SNAKE_ACTION_CHOMP;
    }

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
        buf->actions[buf->count] = 0;
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

    // HACK: To handle segments that overlap, we look at even the previous segment
    while ((prev_segment - snake->segments) > 0 &&
           curr_segment->x == prev_segment->x && curr_segment->y == prev_segment->y) {
        prev_segment--;
    }

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

    S32 prev_segment_index = segment_index - 1;
    SnakeSegment* curr_segment = snake->segments + segment_index;
    SnakeSegment* prev_segment = snake->segments + prev_segment_index;

    while (curr_segment->x == prev_segment->x &&
           curr_segment->y == prev_segment->y) {
        prev_segment_index--;
        if (prev_segment_index < 0) {
            return opposite_direction(snake_segment_direction_to_tail(snake, 0));
        }
        prev_segment = snake->segments + prev_segment_index;
    }

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

    S32 next_segment_index = segment_index + 1;
    SnakeSegment* next_segment = snake->segments + next_segment_index;
    while (curr_segment->x == next_segment->x &&
           curr_segment->y == next_segment->y) {
        next_segment_index++;
        if (next_segment_index >= snake->length) {
            return snake_segment_direction_to_tail(snake, snake->length - 1);
        }
        next_segment = snake->segments + next_segment_index;
    }
    return _direction_between_segments(curr_segment, next_segment);
}
