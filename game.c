//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"

#include <assert.h>
#include <stdio.h> // TODO: remove

typedef struct {
    S16 snake_index;
    S16 segment_index;
} SnakeCollision;

typedef SnakeCollision SnakeSegmentID;

typedef enum {
    SNAKE_KILL_CHECK_UNREACHABLE,
    SNAKE_KILL_CHECK_CELL,
    SNAKE_KILL_CHECK_WALL,
    SNAKE_KILL_CHECK_TACO,
    SNAKE_KILL_CHECK_OTHER_SNAKE,
    SNAKE_KILL_CHECK_SELF,
} SnakeKillCheck;

typedef struct {
    S32 current_x;
    S32 current_y;
    S32 previous_x;
    S32 previous_y;
    S32 next_x;
    S32 next_y;
} SnakeSegmentPosition;

void _track_snake_segment_position(Snake* snake, S32 segment_index, SnakeSegmentPosition* snake_segment_position) {
    memset(snake_segment_position, 0, sizeof(*snake_segment_position));
    if (segment_index < 0 || segment_index >= snake->length) {
        return;
    }

    SnakeSegment* segment = snake->segments + segment_index;
    snake_segment_position->current_x = segment->x;
    snake_segment_position->current_y = segment->y;

    S32 previous_segment_index = segment_index - 1;
    if (previous_segment_index >= 0) {
        SnakeSegment* previous_segment = snake->segments + previous_segment_index;
        snake_segment_position->previous_x = previous_segment->x;
        snake_segment_position->previous_y = previous_segment->y;
    }

    S32 next_segment_index = segment_index + 1;
    if (next_segment_index < snake->length) {
        SnakeSegment* next_segment = snake->segments + next_segment_index;
        snake_segment_position->next_x = next_segment->x;
        snake_segment_position->next_y = next_segment->y;
    }
}

SnakeSegment* _snake_adjacent_segment(Snake* snake, S32 current_segment, S32 delta) {
    assert(delta == -1 || delta == 1);
    if (delta > 0) {
        S32 new_index = current_segment + 1;
        if (new_index < snake->length) {
            return snake->segments + new_index;
        }
    } else if (delta < 0) {
        S32 new_index = current_segment - 1;
        if (new_index >= 0) {
            return snake->segments + new_index;
        }
    }
    return NULL;
}

bool _snake_segment_positions_equal(SnakeSegmentPosition* a, SnakeSegmentPosition* b) {
    return a->current_x == b->current_x &&
           a->current_y == b->current_y &&
           a->previous_x == b->previous_x &&
           a->previous_y == b->previous_y &&
           a->next_x == b->next_x &&
           a->next_y == b->next_y;
}

S32 _snake_next_uncoiled_segment_index(Snake* snake, S32 segment_index) {
    SnakeSegment* original_segment = snake->segments + segment_index;
    for (S32 i = segment_index + 1; i < snake->length; i++) {
        if (snake->segments[i].x != original_segment->x ||
            snake->segments[i].y != original_segment->y) {
            return i;
        }
    }
    return -1;
}

S32 _snake_prev_uncoiled_segment_index(Snake* snake, S32 segment_index) {
    SnakeSegment* original_segment = snake->segments + segment_index;
    for (S32 i = segment_index - 1; i >= 0; i--) {
        if (snake->segments[i].x != original_segment->x ||
            snake->segments[i].y != original_segment->y) {
            return i;
        }
    }
    return -1;
}

void _snake_segment_coiled_index_range(Snake* snake,
                                       S32 segment_index,
                                       S32* first_index,
                                       S32* last_index) {
    *first_index = segment_index;
    *last_index = segment_index;

    SnakeSegment* original_segment = snake->segments + segment_index;
    for (S32 i = segment_index + 1; i < snake->length; i++) {
        if (snake->segments[i].x == original_segment->x &&
            snake->segments[i].y == original_segment->y) {
            *last_index = i;
        } else {
            break;
        }
    }
    for (S32 i = segment_index - 1; i >= 0; i--) {
        if (snake->segments[i].x == original_segment->x &&
            snake->segments[i].y == original_segment->y) {
            *first_index = i;
        } else {
            break;
        }
    }
}

void _snake_move_all_coiled_segments(Snake* snake, S32 segment_index, S32 new_x, S32 new_y) {
    SnakeSegment* original_segment = snake->segments + segment_index;
    // Find segments before the current that match and update.
    for (S32 i = segment_index + 1; i < snake->length; i++) {
        if (snake->segments[i].x == original_segment->x &&
            snake->segments[i].y == original_segment->y) {
            snake->segments[i].x = (S16)(new_x);
            snake->segments[i].y = (S16)(new_y);
        } else {
            break;
        }
    }
    // Find segments after the current that match and update.
    for (S32 i = segment_index - 1; i >= 0; i--) {
        if (snake->segments[i].x == original_segment->x &&
            snake->segments[i].y == original_segment->y) {
            snake->segments[i].x = (S16)(new_x);
            snake->segments[i].y = (S16)(new_y);
        } else {
            break;
        }
    }
    original_segment->x = (S16)(new_x);
    original_segment->y = (S16)(new_y);
}

void _print_game(Game* game) {
    S32 print_height = game->map.height + 1; // For coordinates.
    char** string = malloc(print_height * sizeof(char*));
    S32 string_length = game->map.width + 2; // For coordinates plus null terminator.
    for (S32 i = 0; i < print_height; i++) {
        string[i] = malloc(string_length);
        memset(string[i], 0, string_length);
    }

    string[0][0] = ' ';
    for (S32 x = 0; x < game->items.width; x++) {
        string[0][x + 1] = '0' + (x % 10);
    }

    for (S32 y = 0; y < game->items.height; y++) {
        S32 print_y = y + 1;
        string[print_y][0] = '0' + (y % 10);
        for (S32 x = 0; x < game->items.width; x++) {
            ItemType item = items_get_cell(&game->items, x, y);
            S32 print_x = x + 1;
            switch(item) {
            case ITEM_TYPE_TACO:
                string[print_y][print_x] = 'T';
                break;
            default:
                string[print_y][print_x] = ' ';
                break;
            }
        }
    }

    for (S32 y = 0; y < game->map.height; y++) {
        S32 print_y = y + 1;
        for (S32 x = 0; x < game->map.width; x++) {
            GID tile_gid = GetMapTile(&game->map, x, y, MAP_SOLID_LAYER);
            S32 print_x = x + 1;
            if (tile_gid == 0) {
                string[print_y][print_x] = '.';
            } else {
                string[print_y][print_x] = 'W';
            }
        }
    }

    char base_chars[MAX_SNAKE_COUNT] = {'a', 'A', '0'};
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S32 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            string[segment->y + 1][segment->x + 1] = (char)(base_chars[s] + (e % 26));
        }
    }

    printf("\n");
    for (S32 i = 0; i < print_height; i++) {
        printf("%s\n", string[i]);
    }

    for (S32 i = 0; i < print_height; i++) {
        free(string[i]);
    }
    free(string);
}

void _snake_eat_taco(Game* game, Snake* snake, S32 new_x, S32 new_y) {
    // Grow the snake length by consuming the taco.
    items_set_cell(&game->items, new_x, new_y, ITEM_TYPE_EMPTY);
    snake->length++;
    // Shift all segments one over toward the tail. The new segment is added where the head is.
    for ( int i = snake->length - 1; i >= 1; i-- ) {
        snake->segments[i] = snake->segments[i - 1];
    }
    // The new segment has max health.
    snake->segments[1].health = snake->segments[0].health;
    // The head is moved into the position where the taco was.
    snake->segments[0].x = (S16)(new_x);
    snake->segments[0].y = (S16)(new_y);
    snake->segments[0].health = (S8)(game->settings.segment_health);
}

void _print_snake_segments(Snake* snake) {
    S32 min_x = -1;
    S32 max_x = -1;
    S32 min_y = -1;
    S32 max_y = -1;
    for (S32 i = 0; i < snake->length; i++) {
        printf("%02d: %d, %d\n", i, snake->segments[i].x, snake->segments[i].y);

        if (i == 0) {
            min_x = snake->segments[i].x;
            max_x = snake->segments[i].x;
            min_y = snake->segments[i].y;
            max_y = snake->segments[i].y;
        } else {
            if (snake->segments[i].x < min_x) {
                min_x = snake->segments[i].x;
            }
            if (snake->segments[i].x > max_x) {
                max_x = snake->segments[i].x;
            }

            if (snake->segments[i].y < min_y) {
                min_y = snake->segments[i].y;
            }
            if (snake->segments[i].y > max_y) {
                max_y = snake->segments[i].y;
            }
        }
    }
    printf("\n");

    printf("  ");
    for (S32 x = min_x; x <= max_x; x++) {
        printf(" %02d", x);
    }
    printf("\n");

    for (S32 y = min_y; y <= max_y; y++) {
        printf("%02d", y);
        for (S32 x = min_x; x <= max_x; x++) {
            S32 matched_index = -1;
            for (S32 i = 0; i < snake->length; i++) {
                if (snake->segments[i].x == x && snake->segments[i].y == y) {
                    if (matched_index >= 0) {
                        matched_index = -2;
                    } else if (matched_index != -2) {
                        matched_index = i;
                    }
                }
            }
            if (matched_index == -1) {
                printf("   ");
            } else if (matched_index == -2) {
                // For when multiple segments live on a cell.
                printf(" **");
            } else {
                if (snake->segments[matched_index].clamped) {
                    printf("c%02d", matched_index);
                } else {
                    printf(" %02d", matched_index);
                }
            }
        }
        printf("\n");
    }
}

void _assert_snake_connected(Snake* original_snake, Snake* final_snake) {
    for (S32 i = 0; i < final_snake->length; i++) {
        if (i >= (final_snake->length - 1)) {
            break;
        }

        // Skip over the segment if the next one is identical.
        if (final_snake->segments[i].x == final_snake->segments[i + 1].x &&
            final_snake->segments[i].y == final_snake->segments[i + 1].y) {
            continue;
        }

        Direction check_head = snake_segment_direction_to_head(final_snake, i);
        Direction check_tail = snake_segment_direction_to_tail(final_snake, i);
        if (check_tail == DIRECTION_NONE || check_head == check_tail) {
            printf("Detected disconnect snake\n");
            printf("original snake: %d\n", original_snake->length);
            _print_snake_segments(original_snake);
            printf("final snake: %d\n", final_snake->length);
            _print_snake_segments(final_snake);
            assert(check_tail != DIRECTION_NONE && check_head != check_tail);
        }
    }
}

void _snake_chomp_segment(Game* game, SnakeCollision* snake_collision) {
    // The head is invincible ! Constricting is the only way to kill.
    if (game->settings.head_invincible && snake_collision->segment_index == 0) {
        return;
    }

    Snake* snake = game->snakes + snake_collision->snake_index;

    // A chomp could be on a coiled segment, so find the range of coiled segments to apply the
    // chomp to.
    S32 first_segment_index = 0;
    S32 last_segment_index = 0;
    _snake_segment_coiled_index_range(snake,
                                      snake_collision->segment_index,
                                      &first_segment_index,
                                      &last_segment_index);
    for (S32 i = first_segment_index; i <= last_segment_index; i++) {
        SnakeSegment* chomped_segment = snake->segments + i;
        chomped_segment->health--;
        if (chomped_segment->health <= 0) {
            for (S32 e = snake_collision->segment_index; e < snake->length; e++) {
                SnakeSegment* segment = snake->segments + e;
                items_set_cell(&game->items, segment->x, segment->y, ITEM_TYPE_TACO);
            }
            snake->length = snake_collision->segment_index;
            if (snake->length == 0) {
                snake->life_state = SNAKE_LIFE_STATE_DEAD;
            }
            break;
        }
    }
}

void _snake_chomp(Snake* snake, Game* game) {
    assert(snake->chomp_state == SNAKE_CHOMP_STATE_NONE);
    //
    // xxx
    //  a
    //  b
    //  c
    //

    S32 chomp_check_x[CHOMP_POINT_CHECK_COUNT];
    S32 chomp_check_y[CHOMP_POINT_CHECK_COUNT];

    for (S32 i = 0; i < CHOMP_POINT_CHECK_COUNT; i++) {
        chomp_check_x[i] = (S32)(snake->segments[0].x);
        chomp_check_y[i] = (S32)(snake->segments[0].y);

        adjacent_cell(snake->direction,
                      chomp_check_x + i,
                      chomp_check_y + i);
    }

    adjacent_cell(rotate_clockwise(snake->direction),
                  chomp_check_x + 0,
                  chomp_check_y + 0);

    adjacent_cell(rotate_counter_clockwise(snake->direction),
                  chomp_check_x + 2,
                  chomp_check_y + 2);

    bool did_chomp = false;
    for (S32 i = 0; i < CHOMP_POINT_CHECK_COUNT; i++) {
        // Check if collided with other snake
        // TODO: This simplifies when we have an array of snakes.
        SnakeCollision snake_collision = {
            .snake_index = -1,
            .segment_index = -1
        };

        for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
            for (S16 e = 0; e < game->snakes[s].length; e++) {
                SnakeSegment* segment = game->snakes[s].segments + e;
                if (segment->x == chomp_check_x[i] &&
                    segment->y == chomp_check_y[i]) {
                    snake_collision.snake_index = s;
                    snake_collision.segment_index = e;
                    break;
                }
            }
        }

        if (snake_collision.snake_index >= 0 && snake_collision.snake_index >= 0) {
            // The middle segment gets clamped.
            _snake_chomp_segment(game, &snake_collision);
            did_chomp = true;
        }
    }

    if (did_chomp) {
        // The middle element is the one in front of the snake, if that one dies, we don't need
        // to clamp, we can just do the bite and move on.
        QueriedObject queried_object = game_query(game, chomp_check_x[1], chomp_check_y[1]);
        if (queried_object.type == QUERIED_OBJECT_TYPE_SNAKE) {
            snake->chomp_state = SNAKE_CHOMP_STATE_CLAMPING;
        } else {
            snake->chomp_state = SNAKE_CHOMP_STATE_BITE;
        }
    }
}

bool _snake_uncoil_clamped(Snake* snake,
                           S16 origin_index,
                           S16 first_clamped_segment_index,
                           S32 next_head_x,
                           S32 next_head_y) {
    assert(origin_index <= first_clamped_segment_index);

    // If any segments are on top of each other, unravel them.
    // TODO: This should be able to be consolidated with the normal snake move code.
    for (S32 i = origin_index; i < first_clamped_segment_index; i++) {
        if (snake->segments[i].x == snake->segments[i + 1].x &&
            snake->segments[i].y == snake->segments[i + 1].y) {

            // TODO: consolidate logic with below
            for (S32 j = i; j > origin_index; j--) {
                snake->segments[j].x = snake->segments[j - 1].x;
                snake->segments[j].y = snake->segments[j - 1].y;
            }
            snake->segments[origin_index].x = (S16)(next_head_x);
            snake->segments[origin_index].y = (S16)(next_head_y);
            return true;
        }
    }

    return false;
}

bool _snake_unravel_clamped(Game* game,
                            Snake* snake,
                            S16 origin_index,
                            S16 first_clamped_segment_after_origin_index) {
    assert(origin_index < first_clamped_segment_after_origin_index);

    // An example unravel
    //
    // [5][4][3][2]
    // [6]      [1][>] -> [6][5][4][3/2/1][>]
    //
    S32 last_corner_index = -1;
    Direction last_corner_direction_to_head = DIRECTION_NONE;
    Direction last_corner_direction_to_tail = DIRECTION_NONE;

    S32 uncoiled_after_origin_index = _snake_next_uncoiled_segment_index(snake, origin_index);
    for (S32 i = uncoiled_after_origin_index; i < first_clamped_segment_after_origin_index; i++) {
        // TODO: Evaluate if snake_segment_direction_to_head() should do this logic.
        Direction direction_to_head = (i == 0) ? snake->direction : snake_segment_direction_to_head(snake, i);
        Direction direction_to_tail = snake_segment_direction_to_tail(snake, i);

        // Check if we are a corner
        DirectionRelationship segment_direction_relationship =
            direction_relationship(direction_to_head, direction_to_tail);
        if (segment_direction_relationship == DIRECTION_RELATIONSHIP_CLOCKWISE ||
            segment_direction_relationship == DIRECTION_RELATIONSHIP_COUNTER_CLOCKWISE) {

            // Did we find a corner that pairs with the last corner for an unravel ?
            if (direction_relationship(last_corner_direction_to_tail, direction_to_head)
                    == DIRECTION_RELATIONSHIP_OPPOSITE &&
                last_corner_direction_to_head == direction_to_tail) {

                // If the final corner has coils, find the last corner by continuing to iterate so
                // that when we unravel, the range consists of the index first coiled segment of the
                // first corner to the last coiled index of the last corner.
                SnakeSegment* next_segment = _snake_adjacent_segment(snake, i, 1);
                if (next_segment &&
                    snake->segments[i].x == next_segment->x &&
                    snake->segments[i].y == next_segment->y) {
                    continue;
                }

                S32 uncoiled_after_last_corner_index =
                    _snake_next_uncoiled_segment_index(snake, last_corner_index);
                if (uncoiled_after_last_corner_index < 0) {
                    return false;
                }
                S32 after_uncoiled_last_corner_index =
                    _snake_next_uncoiled_segment_index(snake, uncoiled_after_last_corner_index);
                if (after_uncoiled_last_corner_index < 0) {
                    return false;
                }

                // Collapse 2 segments starting from the first corner into the origin segment.
                // [5][4][3][2]       [5][4]
                // [6]      [1][>] -> [6]      [3/2/1][>]
                S32 collapse_index = last_corner_index - 1;
                _snake_move_all_coiled_segments(snake,
                                                last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);
                _snake_move_all_coiled_segments(snake,
                                                uncoiled_after_last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);

                // Unravel the rest of the segments, by moving them according to the current corner.
                // [5][4]
                // [6]      [3/2/1][>] -> [6][5][4][3/2/1][>]
                for (S32 j = after_uncoiled_last_corner_index; j <= i; j++) {
                    S32 new_x = snake->segments[j].x;
                    S32 new_y = snake->segments[j].y;
                    adjacent_cell(direction_to_head, &new_x, &new_y);
                    adjacent_cell(direction_to_tail, &new_x, &new_y);
                    snake->segments[j].x = (S16)(new_x);
                    snake->segments[j].y = (S16)(new_y);
                }
                return true;
            } else {
                // If this segment is the same as the last segment, skip it so that we always
                // store the first corner we find.
                SnakeSegment* prev_segment = _snake_adjacent_segment(snake, i, -1);
                if (prev_segment &&
                    snake->segments[i].x == prev_segment->x &&
                    snake->segments[i].y == prev_segment->y) {
                    continue;
                }

                // Store this corner to see if it matches the next.
                last_corner_index = i;
                last_corner_direction_to_head = direction_to_head;
                last_corner_direction_to_tail = direction_to_tail;
            }
        } else if (segment_direction_relationship == DIRECTION_RELATIONSHIP_OPPOSITE) {
            // If this segment is straight and our last corner is populated, check if the adjacent
            // square is empty.
            if (last_corner_direction_to_head != DIRECTION_NONE) {
                S32 check_x = snake->segments[i].x;
                S32 check_y = snake->segments[i].y;
                adjacent_cell(last_corner_direction_to_head, &check_x, &check_y);
                QueriedObject queried_object = game_query(game, check_x, check_y);
                if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
                    // This means something is in the way and we wouldn't be able to use this corner
                    // to unravel.
                    last_corner_index = -1;
                    last_corner_direction_to_head = DIRECTION_NONE;
                    last_corner_direction_to_tail = DIRECTION_NONE;
                }
            }
        } else {
            assert(!"The snake is broken !");
            last_corner_index = -1;
            last_corner_direction_to_head = DIRECTION_NONE;
            last_corner_direction_to_tail = DIRECTION_NONE;
        }
    }

    return false;
}

bool _snake_reverse_unravel_clamped(Game* game,
                                    Snake* snake,
                                    S16 origin_index,
                                    S16 first_clamped_segment_before_origin_index) {
    assert(origin_index > first_clamped_segment_before_origin_index);

    // An example unravel
    //
    // [5][4][3][2]
    // [6]      [1][>] -> [6][5][4][3/2/1][>]
    //
    S32 last_corner_index = -1;
    Direction last_corner_direction_to_head = DIRECTION_NONE;
    Direction last_corner_direction_to_tail = DIRECTION_NONE;

    S32 uncoiled_before_origin_index = _snake_prev_uncoiled_segment_index(snake, origin_index);
    for (S32 i = uncoiled_before_origin_index; i > first_clamped_segment_before_origin_index; i--) {
        // TODO: Evaluate if snake_segment_direction_to_head() should do this logic.
        Direction direction_to_head = (i == 0) ? snake->direction : snake_segment_direction_to_head(snake, i);
        Direction direction_to_tail = snake_segment_direction_to_tail(snake, i);

        // Check if we are a corner
        DirectionRelationship segment_direction_relationship =
            direction_relationship(direction_to_head, direction_to_tail);
        if (segment_direction_relationship == DIRECTION_RELATIONSHIP_CLOCKWISE ||
            segment_direction_relationship == DIRECTION_RELATIONSHIP_COUNTER_CLOCKWISE) {

            // Did we find a corner that pairs with the last corner for an unravel ?
            if (direction_relationship(last_corner_direction_to_tail, direction_to_head)
                    == DIRECTION_RELATIONSHIP_OPPOSITE &&
                last_corner_direction_to_head == direction_to_tail) {

                // If the final corner has coils, find the last corner by continuing to iterate so
                // that when we unravel, the range consists of the index first coiled segment of the
                // first corner to the last coiled index of the last corner.
                SnakeSegment* prev_segment = _snake_adjacent_segment(snake, i, -1);
                if (prev_segment &&
                    snake->segments[i].x == prev_segment->x &&
                    snake->segments[i].y == prev_segment->y) {
                    continue;
                }

                S32 uncoiled_before_last_corner_index =
                    _snake_prev_uncoiled_segment_index(snake, last_corner_index);
                if (uncoiled_before_last_corner_index < 0) {
                    return false;
                }
                S32 before_uncoiled_last_corner_index =
                    _snake_prev_uncoiled_segment_index(snake, uncoiled_before_last_corner_index);
                if (before_uncoiled_last_corner_index < 0) {
                    return false;
                }

                // Collapse 2 segments starting from the first corner into the origin segment.
                // [5][4][3][2]       [5][4]
                // [6]      [1][>] -> [6]      [3/2/1][>]
                S32 collapse_index = last_corner_index - 1;
                _snake_move_all_coiled_segments(snake,
                                                last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);
                _snake_move_all_coiled_segments(snake,
                                                uncoiled_before_last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);

                // Unravel the rest of the segments, by moving them according to the current corner.
                // [5][4]
                // [6]      [3/2/1][>] -> [6][5][4][3/2/1][>]
                for (S32 j = before_uncoiled_last_corner_index; j >= i; j--) {
                    S32 new_x = snake->segments[j].x;
                    S32 new_y = snake->segments[j].y;
                    adjacent_cell(direction_to_head, &new_x, &new_y);
                    adjacent_cell(direction_to_tail, &new_x, &new_y);
                    snake->segments[j].x = (S16)(new_x);
                    snake->segments[j].y = (S16)(new_y);
                }
                return true;
            } else {
                // If this segment is the same as the last segment, skip it so that we always
                // store the first corner we find.
                SnakeSegment* next_segment = _snake_adjacent_segment(snake, i, 1);
                if (next_segment &&
                    snake->segments[i].x == next_segment->x &&
                    snake->segments[i].y == next_segment->y) {
                    continue;
                }

                // Store this corner to see if it matches the next.
                last_corner_index = i;
                last_corner_direction_to_head = direction_to_head;
                last_corner_direction_to_tail = direction_to_tail;
            }
        } else if (segment_direction_relationship == DIRECTION_RELATIONSHIP_OPPOSITE) {
            // If this segment is straight and our last corner is populated, check if the adjacent
            // square is empty.
            if (last_corner_direction_to_head != DIRECTION_NONE) {
                S32 check_x = snake->segments[i].x;
                S32 check_y = snake->segments[i].y;
                adjacent_cell(last_corner_direction_to_head, &check_x, &check_y);
                QueriedObject queried_object = game_query(game, check_x, check_y);
                if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
                    // This means something is in the way and we wouldn't be able to use this corner
                    // to unravel.
                    last_corner_index = -1;
                    last_corner_direction_to_head = DIRECTION_NONE;
                    last_corner_direction_to_tail = DIRECTION_NONE;
                }
            }
        } else {
            assert(!"The snake is broken !");
            last_corner_index = -1;
            last_corner_direction_to_head = DIRECTION_NONE;
            last_corner_direction_to_tail = DIRECTION_NONE;
        }
    }

    return false;
}

void _snake_move_clamped(Snake* snake, Game* game, S16 first_clamped_segment_index) {
    // Check if the head can move forward
    S32 next_head_x = snake->segments[0].x;
    S32 next_head_y = snake->segments[0].y;
    adjacent_cell(snake->direction, &next_head_x, &next_head_y);
    QueriedObject queried_object = game_query(game, next_head_x, next_head_y);
    if (queried_object.type == QUERIED_OBJECT_TYPE_ITEM &&
        queried_object.item == ITEM_TYPE_TACO) {
        _snake_eat_taco(game, snake, next_head_x, next_head_y);
        return;
    }

    if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
        return;
    }

    Snake original_snake = {0};
    snake_clone(&original_snake, snake);

    if (_snake_uncoil_clamped(snake, 0, first_clamped_segment_index, next_head_x, next_head_y)) {
        _assert_snake_connected(&original_snake, snake);
        snake_destroy(&original_snake);
        return;
    }

    //
    // [>]
    // [1][2]
    //
    S32 next_uncoiled_segment_index = _snake_next_uncoiled_segment_index(snake, 0);
    if (next_uncoiled_segment_index > 0 &&
        snake_segment_direction_to_tail(snake, next_uncoiled_segment_index) == snake->direction) {
        _snake_move_all_coiled_segments(snake, 0, next_head_x, next_head_y);
        _snake_move_all_coiled_segments(snake, next_uncoiled_segment_index, next_head_x, next_head_y);
        _assert_snake_connected(&original_snake, snake);
        snake_destroy(&original_snake);
        return;
    // Check for general unravelling.
    } else if (_snake_unravel_clamped(game, snake, 0, first_clamped_segment_index)) {
        _assert_snake_connected(&original_snake, snake);
        _snake_uncoil_clamped(snake, 0, first_clamped_segment_index, next_head_x, next_head_y);
        snake_destroy(&original_snake);
        return;
    }

    snake_destroy(&original_snake);

    //
    // [ ][ ] -> [ ][ ][>]
    //    [>]
    //
    // or
    //
    // [ ]     [ ]
    // [ ]  -> [ ][>]
    // [>]
    //

    S32 final_cell_x = next_head_x;
    S32 final_cell_y = next_head_y;
    adjacent_cell(direction_to_tail, &final_cell_x, &final_cell_y);

    queried_object = game_query(game, final_cell_x, final_cell_y);
    if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
        return;
    }

    snake->segments[0].x = (S16)(final_cell_x);
    snake->segments[0].y = (S16)(final_cell_y);
}

void _snake_move(Snake* snake, Game* game) {
    if (snake->length <= 0) {
        return;
    }

    for (S16 i = 0; i < snake->length; i++) {
        SnakeSegment* segment = snake->segments + i;
        if (segment->clamped) {
            _snake_move_clamped(snake, game, i);
            return;
        }
    }

    S32 new_snake_x = (S32)(snake->segments[0].x);
    S32 new_snake_y = (S32)(snake->segments[0].y);

    adjacent_cell(snake->direction, &new_snake_x, &new_snake_y);

    ItemType item_type = items_get_cell(&game->items, new_snake_x, new_snake_y);

    // TODO: Duplication with _snake_chomp() to figure out.
    SnakeCollision snake_collision = {
        .snake_index = -1,
        .segment_index = -1
    };
    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 i = 0; i < game->snakes[s].length; i++) {
            SnakeSegment* segment = game->snakes[s].segments + i;
            if (segment->x == new_snake_x &&
                segment->y == new_snake_y) {
                snake_collision.snake_index = s;
                snake_collision.segment_index = i;
                break;
            }
        }
    }

    if (snake_collision.snake_index >= 0) {
        return;
    }

    GID tile_gid = GetMapTile(&game->map, new_snake_x, new_snake_y, MAP_SOLID_LAYER);

    if (tile_gid == 0) {
        if (item_type == ITEM_TYPE_TACO) {
            _snake_eat_taco(game, snake, new_snake_x, new_snake_y);
        } else {
            // Move the snake in the directon it is heading by moving all segments.
            S32 first_index_to_unravel = (snake->length - 1);
            for (int i = 0; i <= first_index_to_unravel; i++) {
                if (snake->segments[i].x == snake->segments[i + 1].x &&
                    snake->segments[i].y == snake->segments[i + 1].y) {
                    first_index_to_unravel = i;
                    break;
                }
            }
            for (int i = first_index_to_unravel; i >= 1; i--) {
                snake->segments[i].x = snake->segments[i - 1].x;
                snake->segments[i].y = snake->segments[i - 1].y;
            }
            snake->segments[0].x = (S16)(new_snake_x);
            snake->segments[0].y = (S16)(new_snake_y);
        }
    }
}

bool _snake_lunge(Snake* snake, Game* game) {
    bool lunged = false;

    // TODO: Consolidate with unraveling code.
    S32 last_corner_index = -1;
    Direction last_corner_direction_to_head = DIRECTION_NONE;
    Direction last_corner_direction_to_tail = DIRECTION_NONE;

    for (S32 i = 0; i < snake->length; i++) {
        if (snake->segments[i].clamped) {
            break;
        }

        // TODO: Evaluate if snake_segment_direction_to_head() should do this logic.
        Direction direction_to_head = (i == 0) ? snake->direction : snake_segment_direction_to_head(snake, i);
        Direction direction_to_tail = snake_segment_direction_to_tail(snake, i);

        DirectionRelationship segment_direction_relationship =
            direction_relationship(direction_to_head, direction_to_tail);

        if (segment_direction_relationship == DIRECTION_RELATIONSHIP_CLOCKWISE ||
            segment_direction_relationship == DIRECTION_RELATIONSHIP_COUNTER_CLOCKWISE) {

            // Did we find a corner that pairs with the last corner for an unravel ?
            if (direction_relationship(last_corner_direction_to_tail, direction_to_head)
                    == DIRECTION_RELATIONSHIP_OPPOSITE &&
                last_corner_direction_to_head == direction_to_tail) {

                // If the final corner has coils, find the last corner by continuing to iterate so
                // that when we unravel, the range consists of the index first coiled segment of the
                // first corner to the last coiled index of the last corner.
                SnakeSegment* next_segment = _snake_adjacent_segment(snake, i, 1);
                if (next_segment &&
                    snake->segments[i].x == next_segment->x &&
                    snake->segments[i].y == next_segment->y) {
                    continue;
                }

                S32 uncoiled_after_last_corner_index =
                    _snake_next_uncoiled_segment_index(snake, last_corner_index);
                if (uncoiled_after_last_corner_index < 0) {
                    break;
                }
                S32 after_uncoiled_last_corner_index =
                    _snake_next_uncoiled_segment_index(snake, uncoiled_after_last_corner_index);
                if (after_uncoiled_last_corner_index < 0) {
                    break;
                }

                // Collapse 2 segments starting from the first corner into the origin segment.
                // [5][4][3][2]       [5][4]
                // [6]      [1][>] -> [6]      [3/2/1][>]
                S32 collapse_index = last_corner_index - 1;
                _snake_move_all_coiled_segments(snake,
                                                last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);
                _snake_move_all_coiled_segments(snake,
                                                uncoiled_after_last_corner_index,
                                                snake->segments[collapse_index].x,
                                                snake->segments[collapse_index].y);

                // Unravel the rest of the segments, by moving them according to the current corner.
                // [5][4]
                // [6]      [3/2/1][>] -> [6][5][4][3/2/1][>]
                for (S32 j = after_uncoiled_last_corner_index; j <= i; j++) {
                    S32 new_x = snake->segments[j].x;
                    S32 new_y = snake->segments[j].y;
                    adjacent_cell(direction_to_head, &new_x, &new_y);
                    adjacent_cell(direction_to_tail, &new_x, &new_y);
                    snake->segments[j].x = (S16)(new_x);
                    snake->segments[j].y = (S16)(new_y);
                }

                // As long as there is space ahead of the snake, uncoil.
                bool hit_object = false;
                for (S32 k = 0; k < 2; k++) {
                    S32 next_head_x = snake->segments[0].x;
                    S32 next_head_y = snake->segments[0].y;
                    adjacent_cell(snake->direction, &next_head_x, &next_head_y);

                    QueriedObject queried_object = game_query(game, next_head_x, next_head_y);
                    if (queried_object.type == QUERIED_OBJECT_TYPE_ITEM &&
                        queried_object.item == ITEM_TYPE_TACO) {
                        _snake_eat_taco(game, snake, next_head_x, next_head_y);
                    } else if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
                        hit_object = true;
                        break;
                    }

                    _snake_uncoil_clamped(snake, 0, (S16)(i), next_head_x, next_head_y);
                }

                lunged = true;

                if (hit_object) {
                    break;
                }

                // Reset the loop to before the original corner to see if we created another corner
                // at the previous element
                //
                // > [4][3]
                //   [5][2]    ->  > [5][4]
                //[7][6][1][>]    [7][6][3][2][1][>]
                //
                i = last_corner_index - 2;
                if (i < 0) {
                    i = -1;
                }

                last_corner_index = -1;
                last_corner_direction_to_head = DIRECTION_NONE;
                last_corner_direction_to_tail = DIRECTION_NONE;
            } else if (direction_to_tail == opposite_direction(snake->direction)) {
                // If this segment is the same as the last segment, skip it so that we always
                // store the first corner we find.
                SnakeSegment* prev_segment = _snake_adjacent_segment(snake, i, -1);
                if (prev_segment &&
                    snake->segments[i].x == prev_segment->x &&
                    snake->segments[i].y == prev_segment->y) {
                    continue;
                }

                // Store this corner to see if it matches the next.
                last_corner_index = i;
                last_corner_direction_to_head = direction_to_head;
                last_corner_direction_to_tail = direction_to_tail;
            }
        } else if(segment_direction_relationship == DIRECTION_RELATIONSHIP_OPPOSITE) {
            // If this segment is straight and our last corner is populated, check if the adjacent
            // square is empty.
            if (last_corner_direction_to_head != DIRECTION_NONE) {
                S32 check_x = snake->segments[i].x;
                S32 check_y = snake->segments[i].y;
                adjacent_cell(last_corner_direction_to_head, &check_x, &check_y);
                QueriedObject queried_object = game_query(game, check_x, check_y);
                if (queried_object.type != QUERIED_OBJECT_TYPE_NONE) {
                    // This means something is in the way and we wouldn't be able to use this corner
                    // to unravel.
                    last_corner_index = -1;
                    last_corner_direction_to_head = DIRECTION_NONE;
                    last_corner_direction_to_tail = DIRECTION_NONE;
                }
            }
        } else {
            assert(!"The snake is broken !");
            last_corner_index = -1;
            last_corner_direction_to_head = DIRECTION_NONE;
            last_corner_direction_to_tail = DIRECTION_NONE;
        }
    }

    return lunged;
}

bool game_init(Game* game, const char* map_filepath) {
    if (!LoadMap(&game->map, map_filepath)) {
        fprintf(stderr, "Failed to load map.\n");
        return false;
    }

    if (!items_init(&game->items, game->map.width, game->map.height)) {
        return false;
    }

    int32_t snake_capacity = game->map.width * game->map.height;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (!snake_init(game->snakes + s, snake_capacity)) {
            return false;
        }
    }

    game->state = GAME_STATE_WAITING;
    return true;
}

void game_clone(Game* input, Game* output) {
    if (input->items.width != output->items.width ||
        input->items.height != output->items.height) {
        items_destroy(&output->items);
        items_init(&output->items, input->items.width, input->items.height);
    }

    for (S32 x = 0; x < input->items.width; x++) {
        for (S32 y = 0; y < input->items.height; y++) {
            items_set_cell(&output->items, x, y, items_get_cell(&input->items, x, y));
        }
    }

    // TODO: actually deep clone ?
    output->map = input->map;

    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (output->snakes[i].capacity != input->snakes[i].capacity) {
            snake_destroy(output->snakes + i);
            snake_init(output->snakes + i, input->snakes[i].capacity);
        }

        // Copy the snakes, but save the segments since that is a pointer that must be owned by the
        // output snake.
        SnakeSegment* output_segments = output->snakes[i].segments;
        output->snakes[i] = input->snakes[i];
        output->snakes[i].segments = output_segments;
        for (S32 l = 0; l < input->snakes[i].length; l++) {
            output->snakes[i].segments[l] = input->snakes[i].segments[l];
        }
    }

    output->state = input->state;
    output->settings = input->settings;
}

void _snake_turn(Game* game, SnakeAction snake_action, S32 snake_index) {
    Direction direction = DIRECTION_NONE;
    // TODO: snake_index check
    Snake* snake = game->snakes + snake_index;
    // skip turning if currently chomping
    if (snake->chomp_state != SNAKE_CHOMP_STATE_NONE) {
        return;
    }
    if (snake_action & SNAKE_ACTION_FACE_NORTH) {
        direction = DIRECTION_NORTH;
    }
    if (snake_action & SNAKE_ACTION_FACE_EAST) {
        direction = DIRECTION_EAST;
    }
    if (snake_action & SNAKE_ACTION_FACE_SOUTH) {
        direction = DIRECTION_SOUTH;
    }
    if (snake_action & SNAKE_ACTION_FACE_WEST) {
        direction = DIRECTION_WEST;
    }
    // TODO: Should native snake_turn() take the SnakeAction instead of having to convert to a direction here ?
    snake_turn(snake, direction);
}

void _snake_drag_segment_range(Snake* snake,
                               S32 first_segment_index,
                               S32 last_segment_index,
                               S32 new_x,
                               S32 new_y) {
    // TODO: Consolidate with below.
    S32 iter = (first_segment_index < last_segment_index) ? -1 : 1;
    for (S32 e = last_segment_index; e != first_segment_index; e += iter) {
        SnakeSegment* curr_segment = snake->segments + e;
        SnakeSegment* prev_segment = snake->segments + e + iter;
        curr_segment->x = prev_segment->x;
        curr_segment->y = prev_segment->y;
    }
    // Finally move the specified segment to the new location.
    snake->segments[first_segment_index].x = (S16)(new_x);
    snake->segments[first_segment_index].y = (S16)(new_y);
}

void _snake_drag_segments(Snake* snake, S32 segment_index, S32 new_x, S32 new_y) {
    // From the tail to the specified segment, move each segment closer to the head by replacing
    // the segment's position before it.
    for (S32 e = (snake->length - 1); e > segment_index; e--) {
        SnakeSegment* curr_segment = snake->segments + e;
        SnakeSegment* prev_segment = curr_segment - 1;
        curr_segment->x = prev_segment->x;
        curr_segment->y = prev_segment->y;
    }
    // Finally move the specified segment to the new location.
    snake->segments[segment_index].x = (S16)(new_x);
    snake->segments[segment_index].y = (S16)(new_y);
}

typedef struct {
    S32 x;
    S32 y;
} CellMove;

void init_push_state(Game* game, PushState* push_state) {
    S32 size = (game->map.height * game->map.width);
    push_state->cells = malloc(size);
    memset(push_state->cells, 0, size);
}

bool has_been_pushed(PushState* push_state, Game* game, S32 x, S32 y, Direction direction) {
    assert(direction < DIRECTION_COUNT);
    S32 index = (y * game->map.width) + x;
    assert(index < (game->map.width * game->map.height));
    return push_state->cells[index] & (1 << direction);
}

void mark_pushed(PushState* push_state, Game* game, S32 x, S32 y, Direction direction) {
    assert(direction < DIRECTION_COUNT);
    S32 index = (y * game->map.width) + x;
    assert(index < (game->map.width * game->map.height));
    push_state->cells[index] |= (1 << direction);
}

MoveResult _game_object_push_impl(Game* game, PushState* push_state, S32 x, S32 y, Direction direction);

MoveResult _taco_push(Game* game, PushState* push_state, S32 x, S32 y, Direction direction) {
    S32 adjacent_x = x;
    S32 adjacent_y = y;
    adjacent_cell(direction, &adjacent_x, &adjacent_y);
    MoveResult move_result = _game_object_push_impl(game, push_state, adjacent_x, adjacent_y, direction);
    if (move_result == MOVE_OBJECT_SUCCESS || move_result == MOVE_OBJECT_EMPTY) {
        if (game_empty_at(game, adjacent_x, adjacent_y)) {
            items_set_cell(&game->items, x, y, ITEM_TYPE_EMPTY);
            items_set_cell(&game->items, adjacent_x, adjacent_y, ITEM_TYPE_TACO);
        } else {
            return MOVE_OBJECT_PROGRESS;
        }
    }
    return move_result;
}

bool game_empty_at(Game* game, S32 x, S32 y) {
    QueriedObject queried_object = game_query(game, x, y);
    return queried_object.type == QUERIED_OBJECT_TYPE_NONE;
}

S32 game_query_for_snake_at(Game* game, S32 x, S32 y) {
    QueriedObject queried_object = game_query(game, x, y);
    if (queried_object.type == QUERIED_OBJECT_TYPE_SNAKE) {
        return queried_object.snake.index;
    }
    return -1;
}

MoveResult game_empty_push_result(Game* game, S32 x, S32 y) {
    bool check = game_empty_at(game, x, y);
    if (check) {
        return MOVE_OBJECT_FAIL;
    }
    return MOVE_OBJECT_SUCCESS;
}

MoveResult _game_object_push_impl(Game* game, PushState* push_state, S32 x, S32 y, Direction direction) {
    if (has_been_pushed(push_state, game, x, y, direction)) {
        return MOVE_OBJECT_FAIL;
    }
    mark_pushed(push_state, game, x, y, direction);

    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        return MOVE_OBJECT_EMPTY;
    case QUERIED_OBJECT_TYPE_ITEM:
        if (queried_object.item == ITEM_TYPE_TACO) {
            return _taco_push(game, push_state, x, y, direction);
        }
        break;
    case QUERIED_OBJECT_TYPE_SNAKE:
        if (push_state->original_snake_index == queried_object.snake.index) {
            return MOVE_OBJECT_FAIL;
        }
        return snake_segment_push(game,
                                  push_state,
                                  queried_object.snake.index,
                                  queried_object.snake.segment_index,
                                  direction);
    case QUERIED_OBJECT_TYPE_WALL:
        return MOVE_OBJECT_FAIL;
    default:
        break;
    }

    return MOVE_OBJECT_EMPTY;
}

bool _snake_segment_can_expand(Game* game, S32 x, S32 y, Direction preferred_direction,
                               S32* result_x, S32* result_y) {
    // Prefer expanding straight ahead.
    S32 check_x = x;
    S32 check_y = y;
    adjacent_cell(preferred_direction, &check_x, &check_y);

    // TODO: We may need to push things out the way.
    if (game_empty_at(game, check_x, check_y)) {
        *result_x = check_x;
        *result_y = check_y;
        return true;
    }

    for (S8 d = 0; d < DIRECTION_COUNT; d++) {
        Direction direction = (Direction)(d);
        if (direction == preferred_direction) {
            continue;
        }

        check_x = x;
        check_y = y;
        adjacent_cell(direction, &check_x, &check_y);

        if (game_empty_at(game, check_x, check_y)) {
            *result_x = check_x;
            *result_y = check_y;
            return true;
        }
    }

    return false;
}

void _snake_segment_expand(Snake* snake, S32 starting_segment, S32 x, S32 y) {
    if (snake->length <= 0) {
        return;
    }

    S32 tail_index = snake->length - 1;
    for (S32 i = starting_segment; i < tail_index; i++) {
        snake->segments[i].x = snake->segments[i + 1].x;
        snake->segments[i].y = snake->segments[i + 1].y;
    }
    snake->segments[tail_index].x = (S16)(x);
    snake->segments[tail_index].y = (S16)(y);
}

MoveResult _game_if_cell_not_empty_try_push_impl(Game* game,
                                                 PushState* push_state,
                                                 S32 target_cell_x,
                                                 S32 target_cell_y,
                                                 Direction first_direction,
                                                 Direction second_direction) {

    MoveResult result = MOVE_OBJECT_EMPTY;
    if (!game_empty_at(game, target_cell_x, target_cell_y)) {
        // If not empty at the target, attempt to push in the specified direction.
        result = _game_object_push_impl(game, push_state, target_cell_x, target_cell_y, first_direction);

        if (result == MOVE_OBJECT_FAIL) {
            // If the first push failed, try again but in the second specified direction.
            result = _game_object_push_impl(game, push_state, target_cell_x, target_cell_y, second_direction);
        }
    }

    return result;
}

MoveResult _game_if_cell_not_empty_try_push(Game* game,
                                            S32 original_snake_index,
                                            S32 target_cell_x,
                                            S32 target_cell_y,
                                            Direction first_direction,
                                            Direction second_direction) {
    PushState push_state = {0};
    init_push_state(game, &push_state);
    // TODO: Maybe init_push_state() should take this as a param.
    push_state.original_snake_index = original_snake_index;

    MoveResult result = _game_if_cell_not_empty_try_push_impl(game,
                                                              &push_state,
                                                              target_cell_x,
                                                              target_cell_y,
                                                              first_direction,
                                                              second_direction);

    free(push_state.cells);
    return result;
}

MoveResult _pass_along_game_object_push(Game* game,
                                        PushState* push_state,
                                        S32 cell_x,
                                        S32 cell_y,
                                        Direction direction) {
    S32 adjacent_cell_to_push_x = cell_x;
    S32 adjacent_cell_to_push_y = cell_y;
    adjacent_cell(direction, &adjacent_cell_to_push_x, &adjacent_cell_to_push_y);
    MoveResult result = _game_object_push_impl(game,
                                               push_state,
                                               adjacent_cell_to_push_x,
                                               adjacent_cell_to_push_y,
                                               direction);
    if (result == MOVE_OBJECT_SUCCESS) {
        // Do we check if the game is empty and forward on the success if so ?
        return MOVE_OBJECT_PROGRESS;
    }
    if (result == MOVE_OBJECT_EMPTY) {
        // If the push is a pass through that doesn't move the current object and there is nothing
        // to push, then we didn't push the original object.
        return MOVE_OBJECT_FAIL;
    }
    return result;
}

// TODO: Better name ? lol
MoveResult _snake_segment_slink(Game* game, S32 snake_index, S32 segment_index, bool towards_head) {
    // Case 1:
    //
    // ..1..    ..1..
    // ..2.. -> .32..
    // ..3..    .4...
    // ..4..    .....
    // ..^..    .....
    //

    //
    // Case 2a
    //
    // .21..    ..1..
    // >34..    ..23.
    // .65.. -> .654.
    // .....    .....
    //

    //
    // Case 2b
    //
    // ..21..    .321..
    // ..34<.    .45...
    // ..65.. -> ..6...
    // ......    ......
    //

    Snake* snake = game->snakes + snake_index;

    // For case 2a and 2b, we can only drag if the snake would remain connected.
    if (towards_head) {
        if (segment_index < (snake->length - 1)) {
            Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);
            Direction next_direction_to_tail = snake_segment_direction_to_tail(snake, segment_index + 1);
            if (next_direction_to_tail != direction_to_head) {
                return MOVE_OBJECT_FAIL;
            }
        }
    } else {
        if (segment_index >= 2) {
            Direction direction_to_tail = snake_segment_direction_to_tail(snake, segment_index);
            Direction prev_direction_to_head = snake_segment_direction_to_head(snake, segment_index - 1);
            if (prev_direction_to_head != direction_to_tail) {
                return MOVE_OBJECT_FAIL;
            }
        }
    }

    // Support iterating forward or backward depending on the direction.
    S32 iter = towards_head ? -1 : 1;
    S32 current_index = segment_index + iter;
    // We intentionally use 0 so that the head does not get slinked since we do not want the head
    // being pushed around.
    S32 past_last_index = towards_head ? 0 : snake->length;
    while (current_index != past_last_index) {
        SnakeSegment* current_segment = snake->segments + current_index;
        SnakeSegment* next_segment = snake->segments + current_index + iter;

        Direction direction_to_next = DIRECTION_NONE;
        if (towards_head) {
            direction_to_next = snake_segment_direction_to_head(snake, current_index);
        } else {
            direction_to_next = snake_segment_direction_to_tail(snake, current_index);
        }
        Direction right_direction = rotate_clockwise(direction_to_next);

        S32 adjacent_current_x = current_segment->x;
        S32 adjacent_current_y = current_segment->y;
        adjacent_cell(right_direction, &adjacent_current_x, &adjacent_current_y);

        S32 next_adjacent_current_x = next_segment->x;
        S32 next_adjacent_current_y = next_segment->y;
        adjacent_cell(right_direction, &next_adjacent_current_x, &next_adjacent_current_y);

        if (!game_empty_at(game, adjacent_current_x, adjacent_current_y) ||
            !game_empty_at(game, next_adjacent_current_x, next_adjacent_current_y)) {
            // If the right side isn't empty, try the left side.
            Direction left_direction = rotate_counter_clockwise(direction_to_next);
            adjacent_current_x = current_segment->x;
            adjacent_current_y = current_segment->y;
            adjacent_cell(left_direction, &adjacent_current_x, &adjacent_current_y);

            next_adjacent_current_x = next_segment->x;
            next_adjacent_current_y = next_segment->y;
            adjacent_cell(left_direction, &next_adjacent_current_x, &next_adjacent_current_y);

            // If neither side works, move on.
            if (!game_empty_at(game, adjacent_current_x, adjacent_current_y) ||
                !game_empty_at(game, next_adjacent_current_x, next_adjacent_current_y)) {
                current_index += iter;
                continue;
            }
        }

        // Drag the element before the original segment index, clamping to 0 or length.
        S32 first_segment_to_drag = segment_index - iter;
        if (first_segment_to_drag <= 0) {
            // We do not drag the head !
            break;
        }
        if (first_segment_to_drag >= snake->length) {
            first_segment_to_drag = snake->length - 1;
        }

        _snake_drag_segment_range(snake,
                                  current_index,
                                  first_segment_to_drag,
                                  adjacent_current_x,
                                  adjacent_current_y);
        _snake_drag_segment_range(snake,
                                  current_index,
                                  first_segment_to_drag,
                                  next_adjacent_current_x,
                                  next_adjacent_current_y);
        return MOVE_OBJECT_SUCCESS;
    }

    // We've made it through all segments without finding open space.
    return MOVE_OBJECT_FAIL;
}

// Push guarantees that if it returns true, the segment that was pushed moved and there is no
// segment at that cell.
MoveResult snake_segment_push(Game* game, PushState* push_state, S32 snake_index, S32 segment_index, Direction direction) {
    // The snake head is not pushable, also if the snake is constricting towards the push, then the
    // push has no effect.
    if (segment_index == 0 ||
        snake_segment_is_constricting_towards(game, snake_index, segment_index, direction)) {
        return MOVE_OBJECT_FAIL;
    }

    Snake* snake = game->snakes + snake_index;
    SnakeSegment* segment_to_move = snake->segments + segment_index;

    SnakeSegmentPosition original_segment_pos = {0};
    _track_snake_segment_position(snake, segment_index, &original_segment_pos);

    Direction direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction direction_to_tail = snake_segment_direction_to_tail(snake, segment_index);

    // The head is pushed.
    //
    // Case 1a
    //
    // .>1..    .....
    // ..2.. -> ..21.
    // ..3..    ..3..
    //
    // Case 1b
    //
    // .....    ..1..
    // .12.. -> ..2..
    // .^3..    ..3..
    //
    if (segment_index == 0) {
        if (direction == direction_to_head) {
            return MOVE_OBJECT_FAIL;
        }
        if (direction == direction_to_tail) {
            return _snake_segment_slink(game, snake_index, segment_index, false);
        }

        if (!directions_are_perpendicular(direction, direction_to_tail)) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       first_cell_to_check_x,
                                                                       first_cell_to_check_y,
                                                                       direction,
                                                                       direction_to_tail);

        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_tail, &final_cell_move_x, &final_cell_move_y);

        push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                            push_state,
                                                            final_cell_move_x,
                                                            final_cell_move_y,
                                                            direction,
                                                            direction_to_tail);

        // Since the first push succeeded, even if we failed, return that we made progress.
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return MOVE_OBJECT_PROGRESS;
        }

        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            return MOVE_OBJECT_PROGRESS;
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
    }

    // Case 2 The body be pushed.
    //
    // ..1..    ..12.
    // .>23. -> ...3.
    // .....    .....
    //
    bool segment_is_corner = directions_are_perpendicular(direction_to_head, direction_to_tail);
    if (segment_is_corner) {
        if (direction == opposite_direction(direction_to_head) ||
            direction == opposite_direction(direction_to_tail)) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction_to_tail, &first_cell_to_check_x, &first_cell_to_check_y);

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       final_cell_move_x,
                                                                       final_cell_move_y,
                                                                       direction_to_tail,
                                                                       direction_to_head);

        if (push_result == MOVE_OBJECT_FAIL) {
            return _snake_segment_slink(game,
                                        snake_index,
                                        segment_index,
                                        direction == direction_to_head);
        } else if (push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        if ((segment_index + 1) < snake->length) {
            SnakeSegment* next_segment = segment_to_move + 1;
            if (next_segment->x == first_cell_to_check_x &&
                next_segment->y == first_cell_to_check_y) {
                if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
                    return MOVE_OBJECT_PROGRESS;
                }

                segment_to_move->x = (S16)(final_cell_move_x);
                segment_to_move->y = (S16)(final_cell_move_y);
                return MOVE_OBJECT_SUCCESS;
            }
        }
    }

    // Case 3
    //
    // ..1..    ..1..
    // ..2.. -> ..23.
    // .>3..    .....
    // .....    .....
    //
    if (segment_index == (snake->length - 1)) {
        if (direction == direction_to_tail) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }

        if (direction == direction_to_head) {
            return _snake_segment_slink(game, snake_index, segment_index, true);
        }

        S32 first_cell_to_check_x = segment_to_move->x;
        S32 first_cell_to_check_y = segment_to_move->y;
        adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

        MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                       push_state,
                                                                       first_cell_to_check_x,
                                                                       first_cell_to_check_y,
                                                                       direction,
                                                                       direction_to_head);
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return push_result;
        }

        SnakeSegmentPosition updated_segment_pos = {0};
        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        S32 final_cell_move_x = first_cell_to_check_x;
        S32 final_cell_move_y = first_cell_to_check_y;
        adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

        push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                            push_state,
                                                            final_cell_move_x,
                                                            final_cell_move_y,
                                                            direction, direction_to_head);

        // Even if this push failed, since the first push succeed, return that we made progress.
        if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
            return MOVE_OBJECT_PROGRESS;
        }

        _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
        if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
            return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
        }

        if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            return MOVE_OBJECT_PROGRESS;
        }

        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
    }

    // Case 4
    //
    // ..1..    ..12.
    // .>2.. -> ...3.
    // ..3..    ...4.
    // ..4..    .....
    //
    if (!segment_is_corner) {
        // Handling exiting early for case 3.
        if (direction == direction_to_head || direction == direction_to_tail) {
            return _pass_along_game_object_push(game,
                                                push_state,
                                                segment_to_move->x,
                                                segment_to_move->y,
                                                direction);
        }
    }

    S32 first_cell_to_check_x = segment_to_move->x;
    S32 first_cell_to_check_y = segment_to_move->y;
    adjacent_cell(direction, &first_cell_to_check_x, &first_cell_to_check_y);

    MoveResult push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                                   push_state,
                                                                   first_cell_to_check_x,
                                                                   first_cell_to_check_y,
                                                                   direction,
                                                                   direction_to_head);
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return push_result;
    }

    SnakeSegmentPosition updated_segment_pos = {0};
    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    S32 second_cell_to_check_x = first_cell_to_check_x;
    S32 second_cell_to_check_y = first_cell_to_check_y;
    adjacent_cell(opposite_direction(direction_to_head), &second_cell_to_check_x, &second_cell_to_check_y);

    push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                        push_state,
                                                        second_cell_to_check_x,
                                                        second_cell_to_check_y,
                                                        direction,
                                                        direction_to_head);

    // Even if this push failed, since the first push succeed, return that we made progress.
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return MOVE_OBJECT_PROGRESS;
    }

    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    S32 final_cell_move_x = first_cell_to_check_x;
    S32 final_cell_move_y = first_cell_to_check_y;
    adjacent_cell(direction_to_head, &final_cell_move_x, &final_cell_move_y);

    push_result = _game_if_cell_not_empty_try_push_impl(game,
                                                        push_state,
                                                        final_cell_move_x,
                                                        final_cell_move_y,
                                                        direction,
                                                        direction_to_head);

    // Even if this push failed, since the first push succeed, return that we made progress.
    if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
        return MOVE_OBJECT_PROGRESS;
    }

    _track_snake_segment_position(snake, segment_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return game_empty_push_result(game, original_segment_pos.current_x, original_segment_pos.current_y);
    }

    if (!game_empty_at(game, first_cell_to_check_x, first_cell_to_check_y) ||
        !game_empty_at(game, second_cell_to_check_x, second_cell_to_check_y) ||
        !game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
        return MOVE_OBJECT_PROGRESS;
    }

    segment_to_move->x = (S16)(second_cell_to_check_x);
    segment_to_move->y = (S16)(second_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, first_cell_to_check_x, first_cell_to_check_y);
    _snake_drag_segments(snake, segment_index, final_cell_move_x, final_cell_move_y);
    return MOVE_OBJECT_SUCCESS;
}

MoveResult snake_segment_constrict(Game* game, S32 snake_index, S32 segment_index, bool left) {
    Snake* snake = game->snakes + snake_index;

    S32 segment_to_move_index = segment_index + 1;
    if (snake->length <= segment_to_move_index) {
        return MOVE_OBJECT_FAIL;
    }

    SnakeSegment* segment_to_move = snake->segments + segment_to_move_index;

    if (segment_to_move->clamped) {
        return MOVE_OBJECT_FAIL;
    }

    Direction current_direction_to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction next_direction_to_head = snake_segment_direction_to_head(snake, segment_to_move_index);

    assert(current_direction_to_head < DIRECTION_COUNT);
    assert(next_direction_to_head < DIRECTION_COUNT);

    Direction rotation_direction = (left) ?
        rotate_counter_clockwise(next_direction_to_head) :
        rotate_clockwise(next_direction_to_head);

    // Limit so we don't rotate in from of the current segment.
    if (rotation_direction == current_direction_to_head ||
        (segment_index == 0 && rotation_direction == snake->direction)) {
        return MOVE_OBJECT_FAIL;
    }

    SnakeSegmentPosition original_segment_pos = {0};
    _track_snake_segment_position(snake, segment_to_move_index, &original_segment_pos);

    // Figure out adjacent cells we plan to move through.
    S32 initial_cell_move_x = segment_to_move->x;
    S32 initial_cell_move_y = segment_to_move->y;
    adjacent_cell(rotation_direction, &initial_cell_move_x, &initial_cell_move_y);

    // Case 1
    //
    // ..1..    ..12.
    // ..23. -> ...3.
    // .....    .....
    //

    bool segment_is_corner = false;
    S32 after_segment_to_move_index = segment_to_move_index + 1;
    if (after_segment_to_move_index < snake->length) {
        SnakeSegment* next_segment = snake->segments + after_segment_to_move_index;
        if (next_segment->x == initial_cell_move_x &&
            next_segment->y == initial_cell_move_y) {
            segment_is_corner = true;
        }
    }

    S32 final_cell_move_x = initial_cell_move_x;
    S32 final_cell_move_y = initial_cell_move_y;
    adjacent_cell(next_direction_to_head, &final_cell_move_x, &final_cell_move_y);

    // In the cases below, the we are operating on segment 1

    if (segment_is_corner) {
        // Case 2
        //
        // ..12.    ..1..
        // ..43. -> ..2..
        // .....    ..3..
        // .....    ..4..
        //

        if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
            S32 segment_to_check_index = after_segment_to_move_index + 1;
            if (segment_to_check_index < snake->length) {
                SnakeSegment* check_segment = snake->segments + segment_to_check_index;
                if (check_segment->x == final_cell_move_x &&
                    check_segment->y == final_cell_move_y) {

                    // If any segment is clamped between us and the tail we cannot move.
                    for (S32 i = segment_to_move_index; i < snake->length; i++) {
                        if (snake->segments[i].clamped) {
                            return MOVE_OBJECT_FAIL;
                        }
                    }

                    S32 tail_index = (snake->length - 1);
                    Direction tail_direction_to_expand = snake_segment_direction_to_tail(snake, tail_index);
                    S32 first_expanded_x = 0;
                    S32 first_expanded_y = 0;
                    S32 second_expanded_x = 0;
                    S32 second_expanded_y = 0;
                    SnakeSegment* tail = snake->segments + (snake->length - 1);
                    if (_snake_segment_can_expand(game, tail->x, tail->y,
                                                  tail_direction_to_expand, &first_expanded_x, &first_expanded_y) &&
                        _snake_segment_can_expand(game, first_expanded_x, first_expanded_y,
                                                  tail_direction_to_expand, &second_expanded_x, &second_expanded_y)) {
                        _snake_segment_expand(snake, segment_to_move_index, first_expanded_x, first_expanded_y);
                        _snake_segment_expand(snake, segment_to_move_index, second_expanded_x, second_expanded_y);
                        return MOVE_OBJECT_SUCCESS;
                    }
                    return MOVE_OBJECT_FAIL;
                }
            }

            MoveResult push_result = _game_if_cell_not_empty_try_push(game,
                                                                      snake_index,
                                                                      final_cell_move_x,
                                                                      final_cell_move_y,
                                                                      rotation_direction,
                                                                      next_direction_to_head);

            if (push_result == MOVE_OBJECT_FAIL || push_result == MOVE_OBJECT_PROGRESS) {
                return push_result;
            }

            SnakeSegmentPosition updated_segment_pos = {0};
            _track_snake_segment_position(snake, segment_to_move_index, &updated_segment_pos);
            if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
                return MOVE_OBJECT_FAIL;
            }

            // Pushes can cause a lot of chaos, even if it succeeds, double check that the space is open.
            if (!game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
                return MOVE_OBJECT_PROGRESS;
            }
        }

        // If the next segment creates a corner with the previous segment, just move towards the diagonal.
        segment_to_move->x = (S16)(final_cell_move_x);
        segment_to_move->y = (S16)(final_cell_move_y);
        return MOVE_OBJECT_SUCCESS;
    }

    // Case 3
    //
    // ..1..    ..12.
    // ..2.. -> ..43.
    // ..3..    .....
    // ..4..    .....
    //

    // Clone the game before making the push.
    Game first_cloned_game = {0};
    game_clone(game, &first_cloned_game);

    MoveResult first_push_first_result =
        _game_if_cell_not_empty_try_push(&first_cloned_game,
                                          snake_index,
                                          initial_cell_move_x,
                                          initial_cell_move_y,
                                          rotation_direction,
                                          next_direction_to_head);

    MoveResult first_push_second_result =
        _game_if_cell_not_empty_try_push(&first_cloned_game,
                                         snake_index,
                                         final_cell_move_x,
                                         final_cell_move_y,
                                         next_direction_to_head,
                                         rotation_direction);

    if (first_push_first_result == MOVE_OBJECT_SUCCESS && first_push_second_result == MOVE_OBJECT_SUCCESS) {
        game_clone(&first_cloned_game, game);
        game_destroy(&first_cloned_game);
    } else {
        // If either of the first attempts fail, try reversing the order of the pushes. We found
        // scenarios where this retry works and keeps our rules simple.
        Game second_cloned_game = {0};
        game_clone(game, &second_cloned_game);

        MoveResult second_push_first_result =
            _game_if_cell_not_empty_try_push(&second_cloned_game,
                                             snake_index,
                                             final_cell_move_x,
                                             final_cell_move_y,
                                             next_direction_to_head,
                                             rotation_direction);

        MoveResult second_push_second_result =
            _game_if_cell_not_empty_try_push(&second_cloned_game,
                                             snake_index,
                                             initial_cell_move_x,
                                             initial_cell_move_y,
                                             rotation_direction,
                                             next_direction_to_head);

        if (second_push_first_result == MOVE_OBJECT_SUCCESS && second_push_second_result == MOVE_OBJECT_SUCCESS) {
            // If the second succeeded in both use that, since we know the first did not succeed in both.
            game_clone(&second_cloned_game, game);
            game_destroy(&first_cloned_game);
            game_destroy(&second_cloned_game);
        } else {
            bool first_pair_has_fail =
                (first_push_first_result == MOVE_OBJECT_FAIL || first_push_second_result == MOVE_OBJECT_FAIL);
            bool second_pair_has_fail =
                (second_push_first_result == MOVE_OBJECT_FAIL || second_push_second_result == MOVE_OBJECT_FAIL);

            // If we find a fail in both push pairs, exit out.
            if (first_pair_has_fail) {
                if (second_pair_has_fail) {
                    // TODO: These return code paths that need to destroy are rough !
                    game_destroy(&first_cloned_game);
                    game_destroy(&second_cloned_game);
                    return MOVE_OBJECT_FAIL;
                } else {
                    game_clone(&second_cloned_game, game);

                    if (second_push_first_result == MOVE_OBJECT_PROGRESS ||
                        second_push_second_result == MOVE_OBJECT_PROGRESS) {
                        game_destroy(&first_cloned_game);
                        game_destroy(&second_cloned_game);
                        return MOVE_OBJECT_PROGRESS;
                    }
                }
            } else {
                game_clone(&first_cloned_game, game);

                if (first_push_first_result == MOVE_OBJECT_PROGRESS ||
                    first_push_second_result == MOVE_OBJECT_PROGRESS) {
                    game_destroy(&first_cloned_game);
                    game_destroy(&second_cloned_game);
                    return MOVE_OBJECT_PROGRESS;
                }
            }

            game_destroy(&first_cloned_game);
            game_destroy(&second_cloned_game);
        }
    }

    // Update snake pointer after game pointer changed.
    snake = game->snakes + snake_index;

    // Check that both cells are empty after our successful pushes, because the first could have
    // succeed, then the second one succeeded and pushed something into the first.
    if (!game_empty_at(game, initial_cell_move_x, initial_cell_move_y) ||
        !game_empty_at(game, final_cell_move_x, final_cell_move_y)) {
        return MOVE_OBJECT_PROGRESS;
    }

    SnakeSegmentPosition updated_segment_pos = {0};
    _track_snake_segment_position(snake, segment_to_move_index, &updated_segment_pos);
    if (!_snake_segment_positions_equal(&original_segment_pos, &updated_segment_pos)) {
        return MOVE_OBJECT_PROGRESS;
    }

    Snake original_snake = {0};
    snake_clone(&original_snake, snake);

    // If any segments after the current segment to move are clamped, then we need to try to
    // unravel up until the clamped segment.
    S32 clamped_segment_before_index = -1;
    for (S32 i = 0; i < segment_to_move_index; i++) {
        SnakeSegment* check_segment = snake->segments + i;
        if (check_segment->clamped) {
            clamped_segment_before_index = i;
        }
    }

    S32 clamped_segment_before_segment_to_move = -1;
    for (S32 i = segment_to_move_index - 1; i >= 0; i--) {
        SnakeSegment* check_segment = snake->segments + i;
        if (check_segment->clamped) {
            clamped_segment_before_segment_to_move = i;
            break;
        }
    }

    for (S32 i = segment_to_move_index; i < snake->length; i++) {
        SnakeSegment* check_segment = snake->segments + i;
        if (check_segment->clamped) {
            //
            // Detect and address this special case
            //
            // [2][1][>] ->       [>]
            // [3]          [3][2][1]
            // [4]          [4]
            //
            S32 next_uncoiled_segment_index = _snake_next_uncoiled_segment_index(snake, segment_to_move_index);
            if (next_uncoiled_segment_index > segment_to_move_index &&
                next_uncoiled_segment_index < i &&
                snake_segment_direction_to_tail(snake, segment_to_move_index) == opposite_direction(next_direction_to_head) &&
                snake_segment_direction_to_tail(snake, next_uncoiled_segment_index) == rotation_direction) {
                _snake_move_all_coiled_segments(snake, next_uncoiled_segment_index, initial_cell_move_x, initial_cell_move_y);
                _snake_move_all_coiled_segments(snake, segment_to_move_index, final_cell_move_x, final_cell_move_y);
                _assert_snake_connected(&original_snake, snake);
                snake_destroy(&original_snake);
                return MOVE_OBJECT_SUCCESS;
            } else {
                // Otherwise, run the normal unravel logic.

                // TODO: May need to clone game to see if this action works, as the first could cause a
                // change while the second doesn't. However, the second should since we've verified
                // above that both spots are empty.
                if (_snake_unravel_clamped(game,
                                           snake,
                                           (S16)(segment_to_move_index),
                                           (S16)(i))) {
                    _snake_uncoil_clamped(snake,
                                          (S16)(segment_to_move_index),
                                          (S16)(i),
                                          initial_cell_move_x,
                                          initial_cell_move_y);
                    _snake_uncoil_clamped(snake,
                                          (S16)(segment_to_move_index),
                                          (S16)(i),
                                          final_cell_move_x,
                                          final_cell_move_y);
                    _assert_snake_connected(&original_snake, snake);
                    snake_destroy(&original_snake);
                    return MOVE_OBJECT_SUCCESS;
                } else if (clamped_segment_before_segment_to_move >= 0 &&
                           _snake_reverse_unravel_clamped(game,
                                                          snake,
                                                          (S16)(segment_to_move_index),
                                                          (S16)(clamped_segment_before_segment_to_move))) {
                   // TODO: Consolidate with above.
                    _snake_uncoil_clamped(snake,
                                          (S16)(segment_to_move_index),
                                          (S16)(i),
                                          initial_cell_move_x,
                                          initial_cell_move_y);
                    _snake_uncoil_clamped(snake,
                                          (S16)(segment_to_move_index),
                                          (S16)(i),
                                          final_cell_move_x,
                                          final_cell_move_y);
                    _assert_snake_connected(&original_snake, snake);
                    snake_destroy(&original_snake);
                    return MOVE_OBJECT_SUCCESS;
                }
            }

            _assert_snake_connected(&original_snake, snake);
            snake_destroy(&original_snake);
            return MOVE_OBJECT_FAIL;
        }
    }
    snake_destroy(&original_snake);

    // As long as the adjacent squares are empty, we can drag the snake's body through it.
    // TODO: If we can push things out of the way, that works too.
    _snake_drag_segments(snake, segment_to_move_index, initial_cell_move_x, initial_cell_move_y);
    _snake_drag_segments(snake, segment_to_move_index, final_cell_move_x, final_cell_move_y);
    return MOVE_OBJECT_SUCCESS;
}

bool snake_segment_is_constricting_towards(Game* game, S32 snake_index, S32 segment_index, Direction from) {
    if (snake_index < 0 || snake_index >= MAX_SNAKE_COUNT) {
        return true;
    }

    Snake* snake = game->snakes + snake_index;

    if (segment_index < 0 || segment_index >= snake->length) {
        return true;
    }

    //
    // <XXX
    //   ^

    Direction to_head = snake_segment_direction_to_head(snake, segment_index);
    Direction clockwise_to_head = rotate_clockwise(to_head);

    // does this really belong here ?
    Direction to_tail = snake_segment_direction_to_tail(snake, segment_index);
    if (snake->constrict_state != SNAKE_CONSTRICT_STATE_NONE &&
        to_head == opposite_direction(to_tail) &&
        (from == to_head || from == to_tail)) {
        return true;
    }

    if (clockwise_to_head == from && snake->constrict_state == SNAKE_CONSTRICT_STATE_LEFT) {
        return true;
    }

    Direction counter_clockwise_to_head = rotate_counter_clockwise(to_head);
    if (counter_clockwise_to_head == from && snake->constrict_state == SNAKE_CONSTRICT_STATE_RIGHT) {
        return true;
    }


    if (from == to_head) {
        //
        // <XX <
        //   X
        //
        Direction clockwise_to_tail = rotate_clockwise(to_tail);
        if (clockwise_to_tail == from && snake->constrict_state == SNAKE_CONSTRICT_STATE_RIGHT) {
            return true;
        }

        //
        //   X
        // <XX <
        //
        Direction counter_clockwise_to_tail = rotate_counter_clockwise(to_tail);
        if (counter_clockwise_to_tail == from && snake->constrict_state == SNAKE_CONSTRICT_STATE_LEFT) {
            return true;
        }
    }

    return false;
}

SnakeKillCheck* kill_check_entry(Game* game, SnakeKillCheck* kill_checks, S32 x, S32 y) {
    return kill_checks + (y * game->map.width) + x;
}

void _flood_fill_kill_checks(Game* game, SnakeKillCheck* kill_checks, S32 snake_index, S32 x, S32 y) {
    SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);

    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        *entry = SNAKE_KILL_CHECK_CELL;
        break;
    case QUERIED_OBJECT_TYPE_SNAKE:
        if (queried_object.snake.index == snake_index) {
            *entry = SNAKE_KILL_CHECK_SELF;
        } else {
            *entry = SNAKE_KILL_CHECK_OTHER_SNAKE;
        }
        break;
    case QUERIED_OBJECT_TYPE_ITEM:
        if (queried_object.item == ITEM_TYPE_TACO) {
            *entry = SNAKE_KILL_CHECK_TACO;
        }
        break;
    case QUERIED_OBJECT_TYPE_WALL:
        *entry = SNAKE_KILL_CHECK_WALL;
        // Creates a boundary that we do not pass.
        return;
    }

    for (S32 d = 0; d < DIRECTION_COUNT; d++) {
        S32 next_x = x;
        S32 next_y = y;
        adjacent_cell(d, &next_x, &next_y);
        if (next_x < 0 || next_x >= game->map.width || next_y < 0 || next_y >= game->map.height) {
            continue;
        }

        SnakeKillCheck* next_entry = kill_check_entry(game, kill_checks, next_x, next_y);
        if (*next_entry == SNAKE_KILL_CHECK_UNREACHABLE) {
            _flood_fill_kill_checks(game, kill_checks, snake_index, next_x, next_y);
        }
    }
}

bool _kill_checks_has_adjacent_empty(Game* game,
                                     SnakeKillCheck* kill_checks,
                                     bool* adjacent_checked,
                                     S32 x,
                                     S32 y,
                                     S32 snake_index) {
    // TODO: clean this up.
    S32 adjacent_check_index = y * game->map.width + x;
    if (adjacent_checked[adjacent_check_index]) {
        return false;
    }
    adjacent_checked[adjacent_check_index] = true;

    SnakeKillCheck* current_entry = kill_check_entry(game, kill_checks, x, y);

    for (S8 d = 0; d < DIRECTION_COUNT; d++) {
        S32 adjacent_x = x;
        S32 adjacent_y = y;
        adjacent_cell(d, &adjacent_x, &adjacent_y);

        SnakeKillCheck* adjacent_entry = kill_check_entry(game, kill_checks, adjacent_x, adjacent_y);
        if (*current_entry == SNAKE_KILL_CHECK_CELL && *adjacent_entry == SNAKE_KILL_CHECK_CELL) {
            return true;
        }

        bool check_adjacent = (*adjacent_entry == SNAKE_KILL_CHECK_CELL ||
                               *adjacent_entry == SNAKE_KILL_CHECK_TACO);

        // If the adjacent cell has a snake, only check it if it is our snake !
        if (*adjacent_entry == SNAKE_KILL_CHECK_OTHER_SNAKE) {
            QueriedObject query = game_query(game, adjacent_x, adjacent_y);
            check_adjacent = (query.type == QUERIED_OBJECT_TYPE_SNAKE &&
                              query.snake.index == snake_index);
        }

        if (check_adjacent) {
            bool check = _kill_checks_has_adjacent_empty(game,
                                                         kill_checks,
                                                         adjacent_checked,
                                                         adjacent_x,
                                                         adjacent_y,
                                                         snake_index);
            if (check) {
                return true;
            }
        }
    }

    return false;
}

void snake_constrict(Game* game, S32 snake_index) {
    Snake* snake = game->snakes + snake_index;
    assert(snake->constrict_state != SNAKE_CONSTRICT_STATE_NONE);

    // Allocate an array of the size of the level wherel each cell is flood filled inside where
    // the snake is constricting.
    S32 cell_count = game->map.width * game->map.height;
    SnakeKillCheck* kill_checks = malloc(cell_count * sizeof(*kill_checks));
    bool* adjacent_checks = malloc(cell_count);

    bool snake_should_attempt_to_kill = true;

    if (snake->kill_damage_cooldown > 0) {
        snake->kill_damage_cooldown--;
        snake_should_attempt_to_kill = false;
    }

    // TODO: Evaluate if we want to go back to doing a single segment constrict per tick.

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    for (S32 segment_index = 0; segment_index < snake->length; segment_index++) {
        bool left = (snake->constrict_state == SNAKE_CONSTRICT_STATE_LEFT);

        // if we haven't unfurled yet, skip.
        if (segment_index > 0 &&
            snake->segments[segment_index].x == snake->segments[segment_index - 1].x &&
            snake->segments[segment_index].y == snake->segments[segment_index - 1].y) {
            continue;
        }

        if (segment_index < (snake->length - 1) &&
            snake->segments[segment_index].x == snake->segments[segment_index + 1].x &&
            snake->segments[segment_index].y == snake->segments[segment_index + 1].y) {
            continue;
        }

        S32 original_segment_index = segment_index;
        bool constrict_success = snake_segment_constrict(game, snake_index, segment_index, left) == MOVE_OBJECT_SUCCESS;
        if (constrict_success) {
            segment_index++;
        }

        if (!snake_should_attempt_to_kill) {
            continue;
        }

        {
            // Check for a kill !

            // Reset the board.
            for (S32 i = 0; i < cell_count; i++) {
                kill_checks[i] = SNAKE_KILL_CHECK_UNREACHABLE;
            }

            // Populate the snake segments.
            for (S32 i = 0; i < snake->length; i++) {
                SnakeSegment* segment = snake->segments + i;

                SnakeKillCheck* entry = kill_check_entry(game, kill_checks, segment->x, segment->y);
                *entry = SNAKE_KILL_CHECK_SELF;
            }

            // If a segment failed to constrict, check if we killed another snake !
            SnakeSegment* segment = snake->segments + original_segment_index;

            Direction direction_to_head = snake_segment_direction_to_head(snake, original_segment_index);
            Direction direction_to_tail = snake_segment_direction_to_tail(snake, original_segment_index);

            if (opposite_direction(direction_to_head) != direction_to_tail) {
                continue;
            }

            S32 cell_to_fill_x = segment->x;
            S32 cell_to_fill_y = segment->y;
            Direction direction_to_cell = DIRECTION_NONE;
            if (left) {
                direction_to_cell = rotate_counter_clockwise(direction_to_head);
            } else {
                direction_to_cell = rotate_clockwise(direction_to_head);
            }

            adjacent_cell(direction_to_cell, &cell_to_fill_x, &cell_to_fill_y);

            // Flood fill inside the snake's constriction.
            _flood_fill_kill_checks(game, kill_checks, snake_index, cell_to_fill_x, cell_to_fill_y);

            // Debug printing.
            // printf("\n");
            // for (S32 y = 0; y < game->map.height; y++) {
            //     for (S32 x = 0; x < game->map.width; x++) {
            //         SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);

            //         // point out the current segment.
            //         if (x == segment->x && y == segment->y) {
            //             printf("c");
            //             continue;
            //         }

            //         switch(*entry){
            //         case SNAKE_KILL_CHECK_UNREACHABLE:
            //             printf(" ");
            //             break;
            //         case SNAKE_KILL_CHECK_WALL:
            //             printf("w");
            //             break;
            //         case SNAKE_KILL_CHECK_TACO:
            //             printf("T");
            //             break;
            //         case SNAKE_KILL_CHECK_CELL:
            //             printf(".");
            //             break;
            //         case SNAKE_KILL_CHECK_OTHER_SNAKE:
            //             printf("s");
            //             break;
            //         case SNAKE_KILL_CHECK_SELF:
            //             printf("m");
            //             break;
            //         }
            //     }
            //     printf("\n");
            // }


            for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
                if (s == snake_index) {
                    continue;
                }

                Snake* check_snake = game->snakes + s;
                if (check_snake->length == 0) {
                    continue;
                }

                SnakeSegment* check_segment = check_snake->segments + 0;
                memset(adjacent_checks, 0, cell_count);

                // There are configurations where a single empty cell is unavoidable, but as long as
                // there are not multiple empty cells together, then we kill the any snake inside.
                if (_kill_checks_has_adjacent_empty(game,
                                                    kill_checks,
                                                    adjacent_checks,
                                                    check_segment->x,
                                                    check_segment->y,
                                                    s)) {
                    continue;
                }

                // Track how many snake segments are inside the constriction and number of adjacent cells.
                S32 snake_segment_count = 0;
                for (S32 y = 0; y < game->map.height; y++) {
                    for (S32 x = 0; x < game->map.width; x++) {
                        SnakeKillCheck* entry = kill_check_entry(game, kill_checks, x, y);
                        if (*entry == SNAKE_KILL_CHECK_OTHER_SNAKE) {
                            QueriedObject queried_object = game_query(game, x, y);
                            if (queried_object.type == QUERIED_OBJECT_TYPE_SNAKE &&
                                queried_object.snake.index == s) {
                                snake_segment_count++;
                            }
                        }
                    }
                }

                // If all of the segments are found within the constriction, kill the snake and replace with tacos.
                if (snake_segment_count == check_snake->length) {
                    for (S32 e = 1; e < check_snake->length; e++) {
                        SnakeCollision snake_collision = {
                            .snake_index = (S16)(s),
                            .segment_index = (S16)(e)
                        };

                        _snake_chomp_segment(game, &snake_collision);
                    }

                    check_snake->segments[0].health--;

                    if (check_snake->segments[0].health == 0) {
                        items_set_cell(&game->items,
                                       check_snake->segments[0].x,
                                       check_snake->segments[0].y,
                                       ITEM_TYPE_TACO);
                        check_snake->length = 0;
                        check_snake->life_state = SNAKE_LIFE_STATE_DEAD;
                    }

                    snake->kill_damage_cooldown = SNAKE_KILL_DAMAGE_COOLDOWN;
                    snake_should_attempt_to_kill = false;
                }
            }
        }
    }

    free(adjacent_checks);
    free(kill_checks);
}

QueriedObject game_query(Game* game, S32 x, S32 y) {
    QueriedObject result = {0};

    GID tile_gid = GetMapTile(&game->map, x, y, MAP_SOLID_LAYER);
    if (tile_gid != 0) {
        result.type = QUERIED_OBJECT_TYPE_WALL;
        return result;
    }

    ItemType item_type = items_get_cell(&game->items, x, y);
    if (item_type != ITEM_TYPE_EMPTY) {
        result.type = QUERIED_OBJECT_TYPE_ITEM;
        result.item = item_type;
        return result;
    }

    for (S16 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S16 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            if (segment->x == x && segment->y == y) {
                result.type = QUERIED_OBJECT_TYPE_SNAKE;
                result.snake.index = s;
                result.snake.segment_index = e;
                return result;
            }
        }
    }

    return result;
}

void game_update(Game* game, SnakeAction* snake_actions) {
    S32 snakes_alive = 0;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        if (snake->life_state != SNAKE_LIFE_STATE_DEAD) {
            snakes_alive++;
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        SnakeAction snake_action = snake_actions[s];
        _snake_turn(game, snake_action, s);
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        if (snake->chomp_cooldown > 0) {
            snake->chomp_cooldown--;
        }

        SnakeAction snake_action = snake_actions[s];
        if (snake->chomp_state == SNAKE_CHOMP_STATE_NONE &&
            snake->chomp_cooldown == 0) {
            if (snake_action & SNAKE_ACTION_CHOMP) {
                _snake_chomp(snake, game);
            }
        } else if (snake->chomp_state == SNAKE_CHOMP_STATE_BITE) {
            snake->chomp_state = SNAKE_CHOMP_STATE_NONE;
            snake->chomp_cooldown = (S8)(game->settings.chomp_ticks);
        } else if (snake->chomp_state == SNAKE_CHOMP_STATE_CLAMPING) {
            if ((snake_action & SNAKE_ACTION_CHOMP) == 0) {
                snake->chomp_state = SNAKE_CHOMP_STATE_NONE;
                snake->chomp_cooldown = (S8)(game->settings.chomp_ticks);
            }
        }
    }

    // Calculate which segments are being clamped, it is important that this is done before any
    // constricting, lunging or moving, because this impacts those within the current tick.
    SnakeSegmentID clamped_segment_ids[MAX_SNAKE_COUNT];
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        if (snake->chomp_state == SNAKE_CHOMP_STATE_NONE) {
            clamped_segment_ids[s].snake_index = -1;
            continue;
        }

        S32 clamped_cell_x = (S16)(snake->segments[0].x);
        S32 clamped_cell_y = (S16)(snake->segments[0].y);

        adjacent_cell(snake->direction, &clamped_cell_x, &clamped_cell_y);

        QueriedObject queried_object = game_query(game, clamped_cell_x, clamped_cell_y);
        if (queried_object.type == QUERIED_OBJECT_TYPE_SNAKE) {
            clamped_segment_ids[s].snake_index = (S16)(queried_object.snake.index);
            clamped_segment_ids[s].segment_index = (S16)(queried_object.snake.segment_index);
        } else {
            clamped_segment_ids[s].snake_index = -1;

            // If no segment is in front of the snake, then it is no longer chomping.
            snake->chomp_state = SNAKE_CHOMP_STATE_NONE;
            snake->chomp_cooldown = (S8)(game->settings.chomp_ticks);
        }
    }

    // Clear all segments being clamped
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        for (S32 e = 0; e < snake->length; e++) {
            SnakeSegment* segment = snake->segments + e;
            segment->clamped = false;
        }
    }

    // Mark segments that are clamped
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (clamped_segment_ids[s].snake_index < 0) {
            continue;
        }

        Snake* snake = game->snakes + clamped_segment_ids[s].snake_index;

        // This segment could be coiled with others, so find and apply clamping to all coiled
        // segments.
        S32 first_segment_index = -1;
        S32 last_segment_index = -1;
        _snake_segment_coiled_index_range(snake,
                                          clamped_segment_ids[s].segment_index,
                                          &first_segment_index,
                                          &last_segment_index);
        for (S32 e = first_segment_index; e <= last_segment_index; e++) {
            SnakeSegment* segment = snake->segments + e;
            segment->clamped = true;
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        snake->constrict_state = SNAKE_CONSTRICT_STATE_NONE;
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;

        if (snake_actions[s] & SNAKE_ACTION_CONSTRICT_LEFT) {
            // constricting both directions cancels out
            if (snake_actions[s] & SNAKE_ACTION_CONSTRICT_RIGHT) {
                continue;
            }
            snake->constrict_state = SNAKE_CONSTRICT_STATE_LEFT;
        }

        if (snake_actions[s] & SNAKE_ACTION_CONSTRICT_RIGHT) {
            snake->constrict_state = SNAKE_CONSTRICT_STATE_RIGHT;
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        if (snake->constrict_state != SNAKE_CONSTRICT_STATE_NONE) {
            snake_constrict(game, s);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* snake = game->snakes + s;
        // Only allow movement if we aren't doing other actions.
        if (snake->constrict_state != SNAKE_CONSTRICT_STATE_NONE ||
            snake->chomp_state != SNAKE_CHOMP_STATE_NONE) {
            continue;
        }

        bool lunged = false;
        if (snake_actions[s] & SNAKE_ACTION_LUNGE) {
            lunged = _snake_lunge(game->snakes + s, game);
        }

        // Move if we weren't trying to or didn't successfully lunge.
        if (!lunged) {
            _snake_move(game->snakes + s, game);
        }
    }

    S32 taco_count = game_count_tacos(game);
    if ((game->settings.zero_tacos_respawn && taco_count == 0) || !game->settings.zero_tacos_respawn) {
        for (size_t i = taco_count; i < (size_t)game->settings.taco_count; i++) {
            game_spawn_taco(game);
        }
    }

    if (snakes_alive == 1) {
        game->state = GAME_STATE_GAME_OVER;
    }
}

void game_destroy(Game* game) {
    items_destroy(&game->items);
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        snake_destroy(game->snakes + s);
    }
}

void game_spawn_taco(Game* game) {
    // If there are no tacos on the map, generate one in an empty cell.
    int32_t attempts = 0;
    while (attempts < 10) {
        int taco_x = (int)(rand() % game->map.width);
        int taco_y = (int)(rand() % game->map.height);

        ItemType item_type = items_get_cell(&game->items, taco_x, taco_y);
        if (item_type == ITEM_TYPE_TACO) {
            attempts++;
            continue;
        }

        GID ground_tile_gid = GetMapTile(&game->map, taco_x, taco_y, MAP_GROUND_LAYER);
        if (ground_tile_gid == 0) {
            attempts++;
            continue;
        }

        GID solid_tile_gid = GetMapTile(&game->map, taco_x, taco_y, MAP_SOLID_LAYER);
        if (solid_tile_gid != 0) {
            attempts++;
            continue;
        }

        bool spawned_on_snake = false;
        for (S32 s = 0; s < MAX_SNAKE_COUNT && !spawned_on_snake; s++) {
            for (S32 e = 0; e < game->snakes[s].length; e++) {
                if (game->snakes[s].segments[e].x == taco_x &&
                    game->snakes[s].segments[e].y == taco_y) {
                    spawned_on_snake = true;
                    break;
                }
            }
        }
        if (spawned_on_snake) {
            attempts++;
            continue;
        }

        items_set_cell(&game->items, taco_x, taco_y, ITEM_TYPE_TACO);
        break;
    }
}

S32 game_count_tacos(Game* game) {
    S32 taco_count = 0;
    for(S32 y = 0; y < game->map.height; y++) {
        for(S32 x = 0; x < game->map.width; x++) {
            ItemType item_type = items_get_cell(&game->items, x, y);
            if (item_type == ITEM_TYPE_TACO) {
                taco_count++;
            }
        }
    }
    return taco_count;
}

size_t game_serialize(const Game* game, void* buffer, size_t buffer_size)
{
    U8 * byte_buffer = buffer;

    size_t msg_size = sizeof(game->state);
    memcpy(byte_buffer, &game->state, msg_size);
    byte_buffer += msg_size;

    msg_size = sizeof(game->settings.wait_to_start_ms);
    memcpy(byte_buffer, &game->settings.wait_to_start_ms, msg_size);
    byte_buffer += msg_size;

    msg_size = items_serialize(&game->items, byte_buffer, buffer_size);
    byte_buffer += msg_size;

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        msg_size = snake_serialize(game->snakes + s,
                                   byte_buffer,
                                   buffer_size - (byte_buffer - (U8*)buffer));
        byte_buffer += msg_size;
    }

    *byte_buffer = (U8)game->state;
    byte_buffer += sizeof(U8);

    return byte_buffer - (U8*)buffer;
}

size_t game_deserialize(void * buffer, size_t size, Game * out)
{
    U8 * byte_buffer = buffer;

    size_t msg_size = sizeof(out->state);
    memcpy(&out->state, byte_buffer, msg_size);
    byte_buffer += msg_size;

    msg_size = sizeof(out->settings.wait_to_start_ms);
    memcpy(&out->settings.wait_to_start_ms, byte_buffer, msg_size);
    byte_buffer += msg_size;

    msg_size = items_deserialize(byte_buffer, size, &out->items);
    byte_buffer += msg_size;

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        msg_size = snake_deserialize(byte_buffer,
                                     size - (byte_buffer - (U8*)buffer),
                                     &out->snakes[s]);
        byte_buffer += msg_size;
    }

    size_t size_of_serialized_game_state = sizeof(U8);
    out->state = *byte_buffer;
    byte_buffer += size_of_serialized_game_state;

    return byte_buffer - (U8*)buffer;
}
