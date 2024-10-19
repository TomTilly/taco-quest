#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "SDL2/SDL.h"

#define LEVEL_WIDTH 20
#define LEVEL_HEIGHT 15
#define CELL_SIZE 32
#define GAME_SIMULATE_TIME_INTERVAL_US 1000000

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

typedef struct {
    int32_t time_to_live_ms;
} CellTaco;

typedef union {
    CellSnakeHead snake_head;
    CellSnakeBody snake_body;
    CellTaco taco;
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

    const char* title = "Taco Quest";
    SDL_Window* window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
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

        Cell snake_tail = {
            .type = CELL_TYPE_SNAKE_TAIL,
        };
        level_set_cell(&level, 5, 3, &snake_tail);

        Cell taco = {
            .type = CELL_TYPE_TACO,
            .data = { .taco = { .time_to_live_ms = 10000 } }
        };
        level_set_cell(&level, 9, 5, &taco);
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

    int64_t time_since_update_us = 0;

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
            if (event.type == SDL_QUIT) {
                quit = true;
                break;
            }
        }

        // Update the game state if a tick has passed.
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            // Find the snake body.
            int32_t snake_head_x = 0;
            int32_t snake_head_y = 0;
            Cell snake_head = {};
            for (int32_t y = 0; y < level.height; y++) {
                for (int32_t x = 0; x < level.width; x++) {
                    Cell cell = {};
                    if (level_get_cell(&level, x, y, &cell) &&
                        cell.type == CELL_TYPE_SNAKE_HEAD) {
                        snake_head = cell;
                        snake_head_x = x;
                        snake_head_y = y;
                        break;
                    }
                }
                if (snake_head.type != CELL_TYPE_EMPTY) {
                    break;
                }
            }

            // If we found the snake head, move it in the direction it is facing.
            if (snake_head.type != CELL_TYPE_EMPTY) {
                int32_t next_snake_head_x = snake_head_x;
                int32_t next_snake_head_y = snake_head_y;
                adjacent_cell(snake_head.data.snake_head.facing, &next_snake_head_x, &next_snake_head_y);

                if (next_snake_head_x >= 0 && next_snake_head_x < level.width &&
                    next_snake_head_y >= 0 && next_snake_head_y < level.height) {
                    // TODO: check for walls.
                    level_set_cell(&level, next_snake_head_x, next_snake_head_y, &snake_head);

                    int32_t cell_x = snake_head_x;
                    int32_t cell_y = snake_head_y;
                    Cell cell = {};
                    do {
                        int32_t prev_cell_x = cell_x;
                        int32_t prev_cell_y = cell_y;
                        adjacent_cell(snake_head.data.snake_head.connected_to, &cell_x, &cell_y);
                        level_get_cell(&level, cell_x, cell_y, &cell);
                        level_set_cell(&level, prev_cell_x, prev_cell_y, &cell);
                    } while (cell.type != CELL_TYPE_SNAKE_TAIL);
                    // TODO: Unless we've eaten a taco, we will move the tail.
                    Cell empty_cell = {};
                    level_set_cell(&level, cell_x, cell_y, &empty_cell);
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
                cell_rect.y = y * CELL_SIZE;
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
