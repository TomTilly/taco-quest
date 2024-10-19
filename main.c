#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "SDL2/SDL.h"

#define LEVEL_WIDTH 20
#define LEVEL_HEIGHT 15
#define CELL_SIZE 32
#define GAME_SIMULATE_TIME_INTERVAL_US 200000

typedef enum {
    CELL_TYPE_EMPTY,
    CELL_TYPE_WALL,
    CELL_TYPE_SNAKE_HEAD,
    CELL_TYPE_SNAKE_BODY,
    CELL_TYPE_SNAKE_TAIL,
    CELL_TYPE_TACO
} CellType;

typedef enum {
    DIRECTION_NONE,
    DIRECTION_NORTH,
    DIRECTION_EAST,
    DIRECTION_SOUTH,
    DIRECTION_WEST
} Direction;

typedef struct {
    Direction connected_to;
    Direction facing;
} CellSnakeHead;

typedef struct {
    Direction connected_to;
} CellSnakeBody;

typedef union {
    CellSnakeHead snake_head;
    CellSnakeBody snake_body;
} CellData;

typedef struct {
    CellType type;
    CellData data;
} Cell;

typedef struct {
    Cell* cells;
    int32_t width;
    int32_t height;
} Level;

typedef struct {
    Cell cell;
    int32_t x;
    int32_t y;
} FoundCell;

void adjacent_cell(Direction direction, int32_t* x, int32_t* y) {
    switch(direction) {
    case DIRECTION_NONE:
        break;
    case DIRECTION_NORTH:
        (*y)++;
        break;
    case DIRECTION_EAST:
        (*x)++;
        break;
    case DIRECTION_SOUTH:
        (*y)--;
        break;
    case DIRECTION_WEST:
        (*x)--;
        break;
    }
}

Direction opposite_direction(Direction direction) {
    switch(direction) {
    case DIRECTION_NONE:
        break;
    case DIRECTION_NORTH:
        return DIRECTION_SOUTH;
    case DIRECTION_EAST:
        return DIRECTION_WEST;
    case DIRECTION_SOUTH:
        return DIRECTION_NORTH;
    case DIRECTION_WEST:
        return DIRECTION_EAST;
    }

    return DIRECTION_NONE;
}

bool level_init(Level* level, int32_t width, int32_t height) {
    int32_t size = width * height * sizeof(level->cells[0]);
    level->cells = malloc(size);

    if (level->cells == NULL) {
        return false;
    }

    level->width = width;
    level->height = height;
    memset(level->cells, 0, size);

    return true;
}

void level_destroy(Level* level) {
    free(level->cells);
    memset(level, 0, sizeof(*level));
}

int32_t level_get_cell_index(Level* level, int32_t x, int32_t y) {
    return y * level->width + x;
}

bool level_set_cell(Level* level, int32_t x, int32_t y, Cell* value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->height) {
        return false;
    }

    int32_t index = level_get_cell_index(level, x, y);
    level->cells[index] = *value;
    return true;
}

bool level_get_cell(Level* level, int32_t x, int32_t y, Cell* value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->width) {
        return false;
    }

    int32_t index = level_get_cell_index(level, x, y);
    *value = level->cells[index];
    return true;
}

FoundCell find_cell(Level* level, CellType cell_type) {
    FoundCell result = {};
    for (int32_t y = 0; y < level->height; y++) {
        for (int32_t x = 0; x < level->width; x++) {
            Cell cell = {};
            if (level_get_cell(level, x, y, &cell) &&
                cell.type == cell_type) {
                result.cell = cell;
                result.x = x;
                result.y = y;
                return result;
            }
        }
    }
    return result;
}

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

int32_t main (int argc, char** argv) {
    (void)(argc);
    (void)(argv);

    printf("hello taco\n");

    int rc = SDL_Init(SDL_INIT_EVERYTHING);
    if (rc < 0) {
        printf("SDL_Init failed %s\n", SDL_GetError());
        return 1;
    }

    int32_t window_width = 1280;
    int32_t window_height = 1024;
    const char* title = "Taco Quest";
    SDL_Window* window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, 0);
    if (window == NULL) {
        printf("SDL_CreateWindow failed %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface* window_surface = SDL_GetWindowSurface(window);

    Level level = {};
    level_init(&level, LEVEL_WIDTH, LEVEL_HEIGHT);

    {
        Cell snake_head = {
            .type = CELL_TYPE_SNAKE_HEAD,
            .data = {
                .snake_head = {
                    .connected_to = DIRECTION_WEST,
                    .facing = DIRECTION_EAST
                }
            }
        };
        level_set_cell(&level, 7, 3, &snake_head);

        Cell snake_body = {
            .type = CELL_TYPE_SNAKE_BODY,
            .data = {
                .snake_body = {
                    .connected_to = DIRECTION_WEST
                }
            }
        };
        level_set_cell(&level, 6, 3, &snake_body);
        level_set_cell(&level, 5, 3, &snake_body);

        Cell snake_tail = {
            .type = CELL_TYPE_SNAKE_TAIL,
        };
        level_set_cell(&level, 4, 3, &snake_tail);

        Cell taco = {
            .type = CELL_TYPE_TACO,
            .data = {}
        };
        level_set_cell(&level, 9, 5, &taco);
        level_set_cell(&level, 14, 3, &taco);
        level_set_cell(&level, 15, 9, &taco);
        level_set_cell(&level, 2, 2, &taco);
        level_set_cell(&level, 8, 10, &taco);
    }

    for (int32_t y = 0; y < level.height; y++) {
        for (int32_t x = 0; x < level.width; x++) {
            if (x == 0 || x == level.width - 1) {
                Cell wall = {
                    .type = CELL_TYPE_WALL,
                    .data = {}
                };
                level_set_cell(&level, x, y, &wall);
            }

            if (y == 0 || y == level.height - 1) {
                Cell wall = {
                    .type = CELL_TYPE_WALL,
                    .data = {}
                };
                level_set_cell(&level, x, y, &wall);
            }
        }
    }

    struct timespec last_frame_timestamp = {};
    timespec_get(&last_frame_timestamp, TIME_UTC);

    // Seed random with time.
    srand((uint32_t)(time(NULL)));

    int64_t time_since_update_us = 0;

    bool input_move_up = false;
    bool input_move_left = false;
    bool input_move_down = false;
    bool input_move_right = false;

    bool quit = false;
    while (!quit) {
        // Calculate how much time has elapsed (in microseconds).
        struct timespec current_frame_timestamp = {};
        timespec_get(&current_frame_timestamp, TIME_UTC);
        time_since_update_us += microseconds_between_timestamps(&last_frame_timestamp, &current_frame_timestamp);
        last_frame_timestamp = current_frame_timestamp;

        // Handle events, such as input or window changes.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_w:
                    input_move_up = true;
                    break;
                case SDLK_a:
                    input_move_left = true;
                    break;
                case SDLK_s:
                    input_move_down = true;
                    break;
                case SDLK_d:
                    input_move_right = true;
                    break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                case SDLK_w:
                    input_move_up = false;
                    break;
                case SDLK_a:
                    input_move_left = false;
                    break;
                case SDLK_s:
                    input_move_down = false;
                    break;
                case SDLK_d:
                    input_move_right = false;
                    break;
                default:
                    break;
                }
                break;
            }
        }

        if (input_move_up) {
            FoundCell found_snake_head = find_cell(&level, CELL_TYPE_SNAKE_HEAD);
            if (found_snake_head.cell.type == CELL_TYPE_SNAKE_HEAD &&
                found_snake_head.cell.data.snake_head.connected_to !=
                DIRECTION_NORTH) {
                found_snake_head.cell.data.snake_head.facing = DIRECTION_NORTH;
                level_set_cell(&level, found_snake_head.x, found_snake_head.y, &found_snake_head.cell);
            }
        }

        if (input_move_left) {
            FoundCell found_snake_head = find_cell(&level, CELL_TYPE_SNAKE_HEAD);
            if (found_snake_head.cell.type == CELL_TYPE_SNAKE_HEAD &&
                found_snake_head.cell.data.snake_head.connected_to !=
                DIRECTION_WEST) {
                found_snake_head.cell.data.snake_head.facing = DIRECTION_WEST;
                level_set_cell(&level, found_snake_head.x, found_snake_head.y, &found_snake_head.cell);
            }
        }

        if (input_move_right) {
            FoundCell found_snake_head = find_cell(&level, CELL_TYPE_SNAKE_HEAD);
            if (found_snake_head.cell.type == CELL_TYPE_SNAKE_HEAD &&
                found_snake_head.cell.data.snake_head.connected_to !=
                DIRECTION_EAST) {
                found_snake_head.cell.data.snake_head.facing = DIRECTION_EAST;
                level_set_cell(&level, found_snake_head.x, found_snake_head.y, &found_snake_head.cell);
            }
        }

        if (input_move_down) {
            FoundCell found_snake_head = find_cell(&level, CELL_TYPE_SNAKE_HEAD);
            if (found_snake_head.cell.type == CELL_TYPE_SNAKE_HEAD &&
                found_snake_head.cell.data.snake_head.connected_to !=
                DIRECTION_SOUTH) {
                found_snake_head.cell.data.snake_head.facing = DIRECTION_SOUTH;
                level_set_cell(&level, found_snake_head.x, found_snake_head.y, &found_snake_head.cell);
            }
        }

        // Update the game state if a tick has passed.
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;
            FoundCell found_snake_head = find_cell(&level, CELL_TYPE_SNAKE_HEAD);

            // If we found the snake head, move it in the direction it is facing.
            if (found_snake_head.cell.type == CELL_TYPE_SNAKE_HEAD) {
                int32_t new_cell_x = found_snake_head.x;
                int32_t new_cell_y = found_snake_head.y;
                adjacent_cell(found_snake_head.cell.data.snake_head.facing, &new_cell_x, &new_cell_y);

                Cell new_cell = {};
                level_get_cell(&level, new_cell_x, new_cell_y, &new_cell);

                // TODO: Check for walls and the snake's own body.
                if (new_cell_x >= 0 && new_cell_x < level.width &&
                    new_cell_y >= 0 && new_cell_y < level.height &&
                    (new_cell.type == CELL_TYPE_EMPTY ||
                     new_cell.type == CELL_TYPE_TACO)) {
                    int32_t old_cell_x = found_snake_head.x;
                    int32_t old_cell_y = found_snake_head.y;
                    Cell old_cell = found_snake_head.cell;

                    // Save connected to to update other snake body parts.
                    Direction connected_to = found_snake_head.cell.data.snake_head.connected_to;

                    // Update the snake's connected to based on the direction we've moved.
                    old_cell.data.snake_head.connected_to = opposite_direction(found_snake_head.cell.data.snake_head.facing);

                    while (true) {
                        // Write the new cell with the old cell's data.
                        level_set_cell(&level, new_cell_x, new_cell_y, &old_cell);

                        // Iterate over the snake by updating the new cell location to the next snake's body part.
                        new_cell_x = old_cell_x;
                        new_cell_y = old_cell_y;

                        // Update the old cell location to the next snake's body part.
                        if (old_cell.type == CELL_TYPE_SNAKE_HEAD ||
                            old_cell.type == CELL_TYPE_SNAKE_BODY) {
                            adjacent_cell(connected_to, &old_cell_x, &old_cell_y);
                        } else {
                            Cell empty_cell = {};
                            level_set_cell(&level, old_cell_x, old_cell_y, &empty_cell);
                            break;
                        }

                        // Get the old cell's data.
                        level_get_cell(&level, old_cell_x, old_cell_y, &old_cell);

                        // Save the direction we are coming from, and update the old cell's
                        // connected_to based on the saved direction from this iteration.
                        if (old_cell.type == CELL_TYPE_SNAKE_HEAD) {
                            Direction tmp = connected_to;
                            connected_to = old_cell.data.snake_head.connected_to;
                            old_cell.data.snake_head.connected_to = tmp;
                        } else if (old_cell.type == CELL_TYPE_SNAKE_BODY) {
                            Direction tmp = connected_to;
                            connected_to = old_cell.data.snake_body.connected_to;
                            old_cell.data.snake_body.connected_to = tmp;
                        }

                        // If we're at the tail, and we ate a taco
                        if (old_cell.type == CELL_TYPE_SNAKE_TAIL &&
                            new_cell.type == CELL_TYPE_TACO) {
                            old_cell.type = CELL_TYPE_SNAKE_BODY;
                            old_cell.data.snake_body.connected_to = connected_to;
                            level_set_cell(&level, new_cell_x, new_cell_y, &old_cell);
                            break;
                        }
                    }
                }
            }

            // If there are no tacos on the map, generate one in an empty cell.
            FoundCell found_taco = find_cell(&level, CELL_TYPE_TACO);
            if (found_taco.cell.type == CELL_TYPE_EMPTY) {
                int32_t attempts = 0;
                while (attempts < 10) {
                    int taco_x = rand() % level.width;
                    int taco_y = rand() % level.height;

                    Cell current_cell = {};
                    level_get_cell(&level, taco_x, taco_y, &current_cell);
                    if (current_cell.type == CELL_TYPE_EMPTY) {
                        Cell new_taco_cell = {
                            .type = CELL_TYPE_TACO,
                            .data = {}
                        };
                        level_set_cell(&level, taco_x, taco_y, &new_taco_cell);
                        break;
                    }
                    attempts++;
                }
            }
        }

        // Clear window
        SDL_FillRect(window_surface, NULL, SDL_MapRGB(window_surface->format, 0x00, 0x00, 0x00));

        // Draw level
        for(int32_t y = 0; y < level.height; y++) {
            for(int32_t x = 0; x < level.width; x++) {
                Cell cell;
                // TODO: Asserts
                bool status = level_get_cell(&level, x, y, &cell);

                if (!status) {
                    printf("Coordinates outside of cell bounds\n");
                    continue;
                }

                if (cell.type == CELL_TYPE_EMPTY) {
                    continue;
                }

                uint32_t rgb = 0;

                switch (cell.type) {
                    case CELL_TYPE_EMPTY: {
                        break;
                    }
                    case CELL_TYPE_WALL: {
                        rgb = SDL_MapRGB(window_surface->format, 0xAD, 0x62, 0x00);
                        break;
                    }
                    case CELL_TYPE_SNAKE_HEAD: {
                        rgb = SDL_MapRGB(window_surface->format, 0x54, 0xFF, 0x00);
                        break;
                    }
                    case CELL_TYPE_SNAKE_BODY: {
                        rgb = SDL_MapRGB(window_surface->format, 0x4e, 0x90, 0x2d);
                        break;
                    }
                    case CELL_TYPE_SNAKE_TAIL: {
                        rgb = SDL_MapRGB(window_surface->format, 0x7c, 0xb2, 0x1e);
                        break;
                    }
                    case CELL_TYPE_TACO: {
                        rgb = SDL_MapRGB(window_surface->format, 0xEE, 0xFF, 0x00);
                        break;
                    }
                }

                SDL_Rect cell_rect = {};
                cell_rect.x = x * CELL_SIZE;
                cell_rect.y = (window_height - CELL_SIZE) - y * CELL_SIZE;
                cell_rect.w = CELL_SIZE;
                cell_rect.h = CELL_SIZE;

                SDL_FillRect(window_surface, &cell_rect, rgb);
            }
        }

        // Render updates
        SDL_UpdateWindowSurface(window);

        // Allow process to go to sleep so we don't use 100% of CPU
        SDL_Delay(0);
    }

    level_destroy(&level);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
