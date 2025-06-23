//
//  game.c
//  TacoQuest
//
//  Created by Thomas Foster on 11/9/24.
//

#include "game.h"

#include <assert.h>
#include <stdio.h> // TODO: remove

#define MAX_SEGMENT_PATH 5
#define INNER_CORNER_CONSTRICTED_SEGMENTS 1
#define OUTPUT_CORNER_CONSTRICTED_SEGMENTS 5
#define STRAIGHT_CONSTRICTED_SEGMENTS 3

typedef struct {
    S16 snake_index;
    S16 segment_index;
} SnakeCollision;

typedef struct {
    SnakeSegmentShape shape;
    S32 x;
    S32 y;
} SnakeConstrictSegmentInfo;

void _snake_chomp_segment(Game* game, SnakeCollision* snake_collision) {
    Snake* snake = game->snakes + snake_collision->snake_index;
    SnakeSegment* chomped_segment = snake->segments + snake_collision->segment_index;
    chomped_segment->health--;
    if (chomped_segment->health <= 0) {
        for (S32 e = snake_collision->segment_index; e < snake->length; e++) {
            SnakeSegment* segment = snake->segments + e;
            level_set_cell(&game->level, segment->x, segment->y, CELL_TYPE_TACO);
        }
        snake->length = snake_collision->segment_index;
    }
}

void _snake_chomp(Snake* snake, Game* game) {
    S32 new_snake_x = (S32)(snake->segments[0].x);
    S32 new_snake_y = (S32)(snake->segments[0].y);

    adjacent_cell(snake->direction,
                  &new_snake_x,
                  &new_snake_y);

    // Check if collided with other snake
    // TODO: This simplifies when we have an array of snakes.
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
        if (snake_collision.snake_index >= 0 && snake->chomp_cooldown <= 0) {
            _snake_chomp_segment(game, &snake_collision);
            snake->chomp_cooldown = SNAKE_CHOMP_COOLDOWN;
        }
    }
}

void _snake_move(Snake* snake, Game* game) {
    S32 new_snake_x = (S32)(snake->segments[0].x);
    S32 new_snake_y = (S32)(snake->segments[0].y);

    adjacent_cell(snake->direction,
                  &new_snake_x,
                  &new_snake_y);

    CellType cell_type = level_get_cell(&game->level, new_snake_x, new_snake_y);

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

    switch (cell_type) {
    case CELL_TYPE_EMPTY: {
        // Move the snake in the directon it is heading by moving all segments.
        for (int i = snake->length - 1; i >= 1; i--) {
            snake->segments[i].x = snake->segments[i - 1].x;
            snake->segments[i].y = snake->segments[i - 1].y;
        }
        snake->segments[0].x = (S16)(new_snake_x);
        snake->segments[0].y = (S16)(new_snake_y);
        break;
    }
    case CELL_TYPE_TACO: {
        // Grow the snake length by consuming the taco.
        level_set_cell(&game->level, new_snake_x, new_snake_y, CELL_TYPE_EMPTY);
        snake->length++;
        // Shift all segments one over toward the tail. The new segment is added where the head is.
        for ( int i = snake->length - 1; i >= 1; i-- ) {
            snake->segments[i] = snake->segments[i - 1];
        }
        // The new segment has max health.
        snake->segments[1].health = SNAKE_SEGMENT_MAX_HEALTH;
        // The head is moved into the position where the taco was.
        snake->segments[0].x = (S16)(new_snake_x);
        snake->segments[0].y = (S16)(new_snake_y);
        break;
    }
    case CELL_TYPE_WALL:
    default:
        break;
    }
}

bool game_init(Game* game, S32 level_width, S32 level_height, S32 max_taco_count) {
    if (!level_init(&game->level, level_width, level_height)) {
        return false;
    }

    int32_t snake_capacity = level_width * level_height;
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (!snake_init(game->snakes + s, snake_capacity)) {
            return false;
        }
    }

    game->max_taco_count = max_taco_count;
    game->state = GAME_STATE_WAITING;
    return true;
}

void _snake_turn(Game* game, SnakeAction snake_action, S32 snake_index) {
    Direction direction = DIRECTION_NONE;
    // TODO: snake_index check
    Snake* snake = game->snakes + snake_index;
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

void _snake_drag(Snake* snake, S32 segment_index, S32 new_x, S32 new_y) {
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

SnakeSegment _adjacent_snake_segment(SnakeSegment original_segment, Direction direction) {
    S32 x = original_segment.x;
    S32 y = original_segment.y;
    adjacent_cell(direction, &x, &y);
    SnakeSegment result = {
        .x = (S16)(x),
        .y = (S16)(y),
    };
    return result;
}

SnakeSegment _diagonal_snake_segment(SnakeSegment original_segment, Direction horizontal_direction, Direction vertical_direction) {
    S32 x = original_segment.x;
    S32 y = original_segment.y;
    adjacent_cell(horizontal_direction, &x, &y);
    adjacent_cell(vertical_direction, &x, &y);
    SnakeSegment result = {
        .x = (S16)(x),
        .y = (S16)(y),
    };
    return result;
}

void _segment_path_add(SnakeSegment new_segment, SnakeSegment* segment_path, S32* segment_path_count) {
    assert(*segment_path_count < MAX_SEGMENT_PATH);
    segment_path[*segment_path_count] = new_segment;
    (*segment_path_count)++;
}

typedef enum {
    SNAKE_SEGMENT_PUSH_TYPE_UNKNOWN,
    SNAKE_SEGMENT_PUSH_TYPE_STRAIGHT,
    SNAKE_SEGMENT_PUSH_TYPE_INNER_CORNER,
    SNAKE_SEGMENT_PUSH_TYPE_OUTER_CORNER,
} SnakeSegmentPushType;

// At the segment, from head to tail, for example, to the north is the head, and its a corner that
// goes west.
SnakeSegmentPushType _corner_push_type(FullDirection push_direction,
                                       FullDirection corner_direction) {
    // TODO: Inner and outer corners seem broken again.
    FullDirection opposite_corner_direction = opposite_full_direction(corner_direction);
    if (push_direction == corner_direction) {
        return SNAKE_SEGMENT_PUSH_TYPE_INNER_CORNER;
    } else if (push_direction == opposite_corner_direction) {
        return SNAKE_SEGMENT_PUSH_TYPE_OUTER_CORNER;
    }
    return SNAKE_SEGMENT_PUSH_TYPE_UNKNOWN;
}

typedef struct {
    S32 x;
    S32 y;
    bool drag;
} Cell;

// based on the outer corner push
#define MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT 5

typedef struct {
    bool can_be_pushed;
    SnakeSegmentPushType push_type;
    SnakeSegmentShape segment_shape;
    Direction direction_to_head;
    Direction direction_to_tail;
    // Cells that we can push into.
    Cell cells[MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT];
    S32 cell_count;
} SnakeSegmentCanBePushedResult;

bool _add_cell(SnakeSegmentCanBePushedResult* result, Cell cell) {
    if (result->cell_count >= MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT) {
        return false;
    }
    result->cells[result->cell_count] = cell;
    result->cell_count++;
    return true;
}

void _remove_cell(SnakeSegmentCanBePushedResult* result, S32 index) {
    if (index < 0 || index >= result->cell_count) {
        return;
    }

    // Shift each of the cells closer to 0 by overwriting the cell we want to remove.
    S32 iterate_to = result->cell_count - 1;
    for (S32 i = index; i < iterate_to; i++) {
        result->cells[i].x = result->cells[i+1].x;
        result->cells[i].y = result->cells[i+1].y;
    }
    result->cell_count--;
}

SnakeSegmentCanBePushedResult _snake_segment_can_be_pushed(Game* game, Snake* snake, S32 segment_index, bool left);

bool _game_object_can_be_pushed(Game* game, S32 x, S32 y, FullDirection full_direction) {
    QueriedObject queried_object = game_query(game, x, y);
    switch (queried_object.type) {
    case QUERIED_OBJECT_TYPE_NONE:
        return true;
    case QUERIED_OBJECT_TYPE_CELL:
        switch(queried_object.cell) {
        case CELL_TYPE_EMPTY:
            return true;
        case CELL_TYPE_TACO:
            // TODO: When we can actually move snakes, _game_object_can_be_pushed(game, x, y, full_direction)
            return false;
        default:
            break;
        }
        return false;
    case QUERIED_OBJECT_TYPE_SNAKE: {
        SnakeSegmentCanBePushedResult result =
            _snake_segment_can_be_pushed(game,
                                         game->snakes + queried_object.snake.index,
                                         queried_object.snake.segment_index,
                                         full_direction);
        (void)(result);
        // TODO: When we can actually move snakes, return result.can_be_pushed.
        return false;
    }
    default:
        break;
    }

    return false;
}

// TODO: change push to constrict
SnakeSegmentCanBePushedResult _snake_segment_can_be_pushed(Game* game,
                                                           Snake* snake,
                                                           S32 segment_index,
                                                           FullDirection direction) {
    SnakeSegmentCanBePushedResult result = {0};

    // Figure out the push type
    SnakeSegmentPushType push_type = SNAKE_SEGMENT_PUSH_TYPE_UNKNOWN;

    // TODO: Consolidate with below
    // TODO: Make a single function that returns all of these, since there is some duplicate work here.
    SnakeSegmentShape segment_shape = snake_segment_shape(snake, segment_index);
    FullDirection direction_to_head = snake_segment_full_direction_to_head(snake, segment_index);
    FullDirection direction_to_tail = snake_segment_full_direction_to_tail(snake, segment_index);

    switch(segment_shape){
    case SNAKE_SEGMENT_SHAPE_VERTICAL:
    case SNAKE_SEGMENT_SHAPE_HORIZONTAL:
        if (!directions_are_perpendicular(direction_to_tail, direction)) {
            break;
        }
        push_type = SNAKE_SEGMENT_PUSH_TYPE_STRAIGHT;
        break;
    case SNAKE_SEGMENT_SHAPE_NORTH_WEST_CORNER:
        push_type = _corner_push_type(direction, FULL_DIRECTION_NORTH_WEST);
        break;
    case SNAKE_SEGMENT_SHAPE_NORTH_EAST_CORNER:
        push_type = _corner_push_type(direction, FULL_DIRECTION_NORTH_EAST);
        break;
    case SNAKE_SEGMENT_SHAPE_SOUTH_WEST_CORNER:
        push_type = _corner_push_type(direction, FULL_DIRECTION_SOUTH_WEST);
        break;
    case SNAKE_SEGMENT_SHAPE_SOUTH_EAST_CORNER:
        push_type = _corner_push_type(direction, FULL_DIRECTION_SOUTH_EAST);
        break;
    case SNAKE_SEGMENT_SHAPE_HEAD:
    case SNAKE_SEGMENT_SHAPE_UNKNOWN:
    default:
        return result;
    }

    // Calculate cells the segments would push into.
    switch(push_type) {
    case SNAKE_SEGMENT_PUSH_TYPE_STRAIGHT: {
        // In this example, we're pushing cell 2 left with respect to the head.
        // .....
        // ..h..
        // .X1..
        // .X2..
        // ..3..
        // ..4..
        Cell first_cell = {
            .x = snake->segments[segment_index].x,
            .y = snake->segments[segment_index].y,
            .drag = true
        };
        adjacent_full_direction_cell(direction, &first_cell.x, &first_cell.y);

        Cell second_cell = first_cell;
        adjacent_full_direction_cell(direction_to_head, &second_cell.x, &second_cell.y);

        result.can_be_pushed =
            _game_object_can_be_pushed(game, first_cell.x, first_cell.y, direction) &&
            _game_object_can_be_pushed(game, second_cell.x, second_cell.y, direction);

        if (result.can_be_pushed) {
            _add_cell(&result, first_cell);
            _add_cell(&result, second_cell);
        }
        break;
    }
    case SNAKE_SEGMENT_PUSH_TYPE_INNER_CORNER: {
        // In this example, we're pushing cell 2 left with respect to the head.
        // ......
        // ..h...
        // .X1...
        // 432...
        Cell cell = {
            .x = snake->segments[segment_index].x,
            .y = snake->segments[segment_index].y,
            .drag = false
        };

        adjacent_full_direction_cell(direction_to_head, &cell.x, &cell.y);
        adjacent_full_direction_cell(direction_to_tail, &cell.x, &cell.y);
        FullDirection in_between = full_direction_in_between(direction_to_head, direction_to_tail);
        result.can_be_pushed = _game_object_can_be_pushed(game, cell.x, cell.y, in_between);
        if (result.can_be_pushed) {
            _add_cell(&result, cell);
        }
        break;
    }
    case SNAKE_SEGMENT_PUSH_TYPE_OUTER_CORNER: {
        // In this example, we're pushing cell 2 left with respect to the head.
        // ......
        // ...h..
        // ...1X.
        // .432X.
        // ..XXX.
        // ......

        Cell cells[MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT] = {0};
        for (S32 i = 0; i < MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT; i++) {
            cells[i].x = snake->segments[segment_index].x;
            cells[i].y = snake->segments[segment_index].y;
            cells[i].drag = true;
        }

        adjacent_full_direction_cell(direction_to_head, &cells[0].x, &cells[0].y);
        adjacent_full_direction_cell(opposite_full_direction(direction_to_tail), &cells[0].x, &cells[0].y);

        adjacent_full_direction_cell(opposite_full_direction(direction_to_tail), &cells[1].x, &cells[1].y);

        adjacent_full_direction_cell(opposite_full_direction(direction_to_head), &cells[2].x, &cells[2].y);
        adjacent_full_direction_cell(opposite_full_direction(direction_to_tail), &cells[2].x, &cells[2].y);

        adjacent_full_direction_cell(opposite_full_direction(direction_to_head), &cells[3].x, &cells[3].y);

        adjacent_full_direction_cell(direction_to_tail, &cells[4].x, &cells[4].y);
        adjacent_full_direction_cell(opposite_full_direction(direction_to_head), &cells[4].x, &cells[4].y);

        FullDirection directions[MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT] = {0};
        directions[0] = full_direction_in_between(direction_to_head, opposite_full_direction(direction_to_tail));
        directions[1] = opposite_full_direction(direction_to_tail);
        directions[2] = full_direction_in_between(opposite_full_direction(direction_to_head), opposite_full_direction(direction_to_tail));
        directions[3] = opposite_full_direction(direction_to_head);
        directions[4] = full_direction_in_between(opposite_full_direction(direction_to_head), direction_to_tail);

        bool can_be_pushed[MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT];
        bool cell_added[MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT] = {0};
        for (S32 i = 0; i < MAX_SNAKE_SEGMENT_PUSH_CELL_COUNT; i++) {
            can_be_pushed[i] = _game_object_can_be_pushed(game, cells[i].x, cells[i].y, directions[i]);
        }

        // ......
        // ...h..
        // ...1W.
        // .432W.
        // ..WWW.
        // ......

        if (can_be_pushed[3] && can_be_pushed[4]) {
            result.can_be_pushed = true;
            if (result.cell_count == 0) {
                cells[4].drag = false;
            }
            cell_added[4] = _add_cell(&result, cells[4]);
            cell_added[3] = _add_cell(&result, cells[3]);
        }

        if (can_be_pushed[1] && can_be_pushed[2] && can_be_pushed[3]) {
            result.can_be_pushed = true;
            // deal with overlap
            if (!cell_added[3]) {
                cell_added[3] = _add_cell(&result, cells[3]);
            }
            cell_added[2] = _add_cell(&result, cells[2]);
            cell_added[1] = _add_cell(&result, cells[1]);
        }

        if (can_be_pushed[0] && can_be_pushed[1]) {
            result.can_be_pushed = true;
            if (!cell_added[1]) {
                cell_added[1] = _add_cell(&result, cells[1]);
            }
            cell_added[0] = _add_cell(&result, cells[0]);
        }

        // If the walls are setup such that the original cell would be disconnected, add it in,
        // at the very end, here is an example.
        // ...... ...... ....... .........
        // ...h.. ...h.. ...h... ...h.....
        // ...1W. ...1W. ...1W.. ...1W....
        // 5432W. 543.W. .54.W.. ..52W....
        // ....W. ..2.W. ..32W.. ..43W....
        // ...... ...... ....... .........

        // The important base configurations are:
        // ..... ...... .......
        // ...h. ...h.. ...h...
        // ...1. ...1.. ...1W..
        // 5432. 5432W. 5432...
        // ....W ...... .......
        // ..... ...... .......
        if (!can_be_pushed[0] || !can_be_pushed[1] || !can_be_pushed[2]) {
            Cell cell = {
                .x = snake->segments[segment_index].x,
                .y = snake->segments[segment_index].y,
                .drag = true
            };
            _add_cell(&result, cell);
        }
        break;
    }
    default:
    case SNAKE_SEGMENT_PUSH_TYPE_UNKNOWN:
        break;
    }

    return result;
}

bool _snake_segment_push(Game* game, Snake* snake, S32 segment_index, FullDirection direction) {
    SnakeSegmentCanBePushedResult can_be_pushed_result =
        _snake_segment_can_be_pushed(game, snake, segment_index, direction);
    if (!can_be_pushed_result.can_be_pushed) {
        return false;
    }
    for (S32 i = 0; i < can_be_pushed_result.cell_count; i++) {
        if (can_be_pushed_result.cells[i].drag) {
            _snake_drag(snake,
                        segment_index,
                        can_be_pushed_result.cells[i].x,
                        can_be_pushed_result.cells[i].y);
        } else {
            snake->segments[segment_index].x = (S16)(can_be_pushed_result.cells[i].x);
            snake->segments[segment_index].y = (S16)(can_be_pushed_result.cells[i].y);
        }
    }
    return true;
}

FullDirection _corner_push_direction_from_head(FullDirection direction_to_head,
                                               bool left,
                                               FullDirection vertical_direction,
                                               FullDirection horizontal_direction) {
    FullDirection result = {0};
    if (left) {
        if (direction_to_head == vertical_direction) {
            result = full_direction_in_between(vertical_direction, horizontal_direction);
        } else if (direction_to_head == horizontal_direction) {
            result = opposite_full_direction(full_direction_in_between(vertical_direction,
                                                                       horizontal_direction));
        }
    } else {
        if (direction_to_head == vertical_direction) {
            result = opposite_full_direction(full_direction_in_between(vertical_direction,
                                                                       horizontal_direction));
        } else if (direction_to_head == horizontal_direction) {
            result = full_direction_in_between(vertical_direction, horizontal_direction);
        }
    }
    return result;
}

void _snake_constrict(Snake* snake, Game* game) {
    // First pass finds the shapes to operate on. This is so we don't operate on corners that we
    // create during this pass and only operate on the existing ones.
    S32 last_segment_index = snake->length - 1;

    // How many elements were impacted by a constrict pass on this tick. If any, advance passed them
    // otherwise we will see the chain constrict effect.
    S32 constricted_elements = 0;
    for (S32 e = snake->constrict_state.index; e < last_segment_index; e++) {
        SnakeSegmentShape segment_shape = snake_segment_shape(snake, e);
        FullDirection direction_to_head = snake_segment_full_direction_to_head(snake, e);
        FullDirection push_direction = FULL_DIRECTION_UNKNOWN;

        switch(segment_shape){
        case SNAKE_SEGMENT_SHAPE_VERTICAL:
        case SNAKE_SEGMENT_SHAPE_HORIZONTAL:
            if (e == 1) {
                // Game design decision, do not constrict a straight segment when it is attached to
                // the head.
                break;
            }
            if (snake->constrict_state.left) {
                push_direction = rotate_counter_clockwise_full_direction_for(direction_to_head, 2);
            } else {
                push_direction = rotate_clockwise_full_direction_for(direction_to_head, 2);
            }
            break;
        case SNAKE_SEGMENT_SHAPE_NORTH_WEST_CORNER:
            push_direction = _corner_push_direction_from_head(direction_to_head,
                                                              snake->constrict_state.left,
                                                              FULL_DIRECTION_NORTH,
                                                              FULL_DIRECTION_WEST);
            break;
        case SNAKE_SEGMENT_SHAPE_NORTH_EAST_CORNER:
            push_direction = _corner_push_direction_from_head(direction_to_head,
                                                              snake->constrict_state.left,
                                                              FULL_DIRECTION_NORTH,
                                                              FULL_DIRECTION_EAST);
            break;
        case SNAKE_SEGMENT_SHAPE_SOUTH_WEST_CORNER:
            push_direction = _corner_push_direction_from_head(direction_to_head,
                                                              snake->constrict_state.left,
                                                              FULL_DIRECTION_SOUTH,
                                                              FULL_DIRECTION_WEST);
            break;
        case SNAKE_SEGMENT_SHAPE_SOUTH_EAST_CORNER:
            push_direction = _corner_push_direction_from_head(direction_to_head,
                                                              snake->constrict_state.left,
                                                              FULL_DIRECTION_SOUTH,
                                                              FULL_DIRECTION_EAST);
            break;
        default:
            break;
        }

        if (_snake_segment_push(game, snake, e, push_direction)) {
            // TODO: Change index based on the type of constriction.
            constricted_elements = OUTPUT_CORNER_CONSTRICTED_SEGMENTS;
            snake->constrict_state.index = e + constricted_elements;
            break;
        }
    }

    if (constricted_elements == 0) {
        snake->constrict_state.index = -1;
    }
}

QueriedObject game_query(Game* game, S32 x, S32 y) {
    QueriedObject result = {0};

    CellType cell_type = level_get_cell(&game->level, x, y);
    if (cell_type != CELL_TYPE_EMPTY) {
        result.type = QUERIED_OBJECT_TYPE_CELL;
        result.cell = cell_type;
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
        SnakeAction snake_action = snake_actions[s];
        _snake_turn(game, snake_action, s);
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        if (game->snakes[s].chomp_cooldown > 0) {
            game->snakes[s].chomp_cooldown--;
        }
        SnakeAction snake_action = snake_actions[s];
        if (snake_action & SNAKE_ACTION_CHOMP) {
            _snake_chomp(game->snakes + s, game);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        SnakeAction snake_action = snake_actions[s];
        if (game->snakes[s].constrict_state.index >= 0) {
            if (((snake_action & SNAKE_ACTION_CONSTRICT_LEFT_END) &&
                game->snakes[s].constrict_state.left) ||
                ((snake_action & SNAKE_ACTION_CONSTRICT_RIGHT_END) &&
                !game->snakes[s].constrict_state.left)) {
                game->snakes[s].constrict_state.index = -1;
            } else {
                _snake_constrict(game->snakes + s, game);
            }
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_LEFT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = true;
            _snake_constrict(game->snakes + s, game);
        } else if (snake_action & SNAKE_ACTION_CONSTRICT_RIGHT) {
            game->snakes[s].constrict_state.index = 0;
            game->snakes[s].constrict_state.left = false;
            _snake_constrict(game->snakes + s, game);
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        // Only allow movement if we aren't constricting.
        if (game->snakes[s].constrict_state.index == -1) {
            _snake_move(game->snakes + s, game);
        }
        if (game->snakes[s].length != 0) {
            snakes_alive++;
        }
    }

    S32 taco_count = game_count_tacos(game);
    for (size_t i = taco_count; i < game->max_taco_count; i++) {
        game_spawn_taco(game);
    }

    if (snakes_alive == 1) {
        game->state = GAME_STATE_GAME_OVER;
    }
}

void game_destroy(Game* game) {
    level_destroy(&game->level);
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        snake_destroy(game->snakes + s);
    }
}

void game_spawn_taco(Game* game) {
    // If there are no tacos on the map, generate one in an empty cell.
    int32_t attempts = 0;
    while (attempts < 10) {
        int taco_x = rand() % game->level.width;
        int taco_y = rand() % game->level.height;

        CellType cell_type = level_get_cell(&game->level, taco_x, taco_y);
        if (cell_type != CELL_TYPE_EMPTY) {
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

        level_set_cell(&game->level, taco_x, taco_y, CELL_TYPE_TACO);
        break;
    }
}

S32 game_count_tacos(Game* game) {
    S32 taco_count = 0;
    for(S32 y = 0; y < game->level.height; y++) {
        for(S32 x = 0; x < game->level.width; x++) {
            CellType cell_type = level_get_cell(&game->level, x, y);
            if (cell_type == CELL_TYPE_TACO) {
                taco_count++;
            }
        }
    }
    return taco_count;
}

size_t game_serialize(const Game* game, void* buffer, size_t buffer_size)
{
    U8 * byte_buffer = buffer;

    size_t msg_size = level_serialize(&game->level, byte_buffer, buffer_size);
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

    size_t msg_size = level_deserialize(byte_buffer, size, &out->level);
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
