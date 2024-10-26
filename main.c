#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL.h>

// TODO: redefine int32_t etc. to u32 or U32

#define LEVEL_WIDTH 24
#define LEVEL_HEIGHT 20
#define GAME_SIMULATE_TIME_INTERVAL_US 150000
#define INITIAL_SNAKE_LEN 15

typedef enum {
    CELL_TYPE_INVALID = -1,
    CELL_TYPE_EMPTY,
    CELL_TYPE_WALL,
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
    int x;
    int y;
} SnakeSegment;

typedef struct {
    SnakeSegment* segments;
    int length;
    Direction direction;
} Snake;

typedef struct {
    CellType* cells;
    int32_t width;
    int32_t height;
    Snake snake;
} Level;

void adjacent_cell(Direction direction, int32_t* x, int32_t* y) {
    switch(direction) {
    case DIRECTION_NONE:
        break;
    case DIRECTION_NORTH:
        (*y)--;
        break;
    case DIRECTION_EAST:
        (*x)++;
        break;
    case DIRECTION_SOUTH:
        (*y)++;
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
    size_t size = width * height * sizeof(level->cells[0]);
    level->cells = malloc(size);

    if (level->cells == NULL) {
        return false;
    }

    memset(level->cells, 0, size);

    level->width = width;
    level->height = height;

    level->snake.segments = calloc(width * height, sizeof(level->snake.segments[0]));
    if (level->snake.segments == NULL) {
        return false;
    }

    return true;
}

void spawn_snake(Snake* snake, int x, int y, Direction direction) {
    snake->length = INITIAL_SNAKE_LEN;
    snake->direction = direction;

    for (int i = 0; i < snake->length; i++) {
        snake->segments[i].x = x;
        snake->segments[i].y = y;
    }
}

void level_destroy(Level* level) {
    free(level->cells);
    memset(level, 0, sizeof(*level));
}

int32_t level_get_cell_index(Level* level, int32_t x, int32_t y) {
    return y * level->width + x;
}

bool level_set_cell(Level* level, int32_t x, int32_t y, CellType value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->height) {
        return false;
    }

    int32_t index = level_get_cell_index(level, x, y);
    level->cells[index] = value;
    return true;
}

CellType level_get_cell(Level* level, int32_t x, int32_t y) {
    if (x < 0 || x >= level->width) {
        return CELL_TYPE_INVALID;
    }

    if (y < 0 || y >= level->width) {
        return CELL_TYPE_INVALID;
    }

    int32_t index = level_get_cell_index(level, x, y);
    return level->cells[index];
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

    int32_t cell_size = 48;
    int32_t window_width = LEVEL_WIDTH * cell_size;
    int32_t window_height = LEVEL_HEIGHT * cell_size;
    const char* title = "Taco Quest";
    SDL_Window* window = SDL_CreateWindow(title,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          window_width,
                                          window_height,
                                          0);
    if (window == NULL) {
        printf("SDL_CreateWindow failed %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    if ( renderer == NULL ) {
        printf("SDL_CreateRenderer failed %s\n", SDL_GetError());
        return 1;
    }

    Level level = {0};
    level_init(&level, LEVEL_WIDTH, LEVEL_HEIGHT);

    level_set_cell(&level, 9, 5, CELL_TYPE_TACO);
    level_set_cell(&level, 14, 3, CELL_TYPE_TACO);
    level_set_cell(&level, 15, 9, CELL_TYPE_TACO);
    level_set_cell(&level, 2, 2, CELL_TYPE_TACO);
    level_set_cell(&level, 8, 10, CELL_TYPE_TACO);

    spawn_snake(&level.snake,
                level.width / 2,
                level.height / 2,
                DIRECTION_EAST);

    for (int32_t y = 0; y < level.height; y++) {
        for (int32_t x = 0; x < level.width; x++) {
            if (x == 0 || x == level.width - 1) {
                level_set_cell(&level, x, y, CELL_TYPE_WALL);
            }

            if (y == 0 || y == level.height - 1) {
                level_set_cell(&level, x, y, CELL_TYPE_WALL);
            }
        }
    }

    struct timespec last_frame_timestamp = {0};
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
        struct timespec current_frame_timestamp = {0};
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

        // TODO: move snake
        if (input_move_up) {
            level.snake.direction = DIRECTION_NORTH;
        }

        if (input_move_left) {
            level.snake.direction = DIRECTION_WEST;
        }

        if (input_move_right) {
            level.snake.direction = DIRECTION_EAST;
        }

        if (input_move_down) {
            level.snake.direction = DIRECTION_SOUTH;
        }

        // Update the game state if a tick has passed.
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            for (int i = level.snake.length; i >= 1; i--) {
                level.snake.segments[i].x = level.snake.segments[i - 1].x;
                level.snake.segments[i].y = level.snake.segments[i - 1].y;
            }

            adjacent_cell(level.snake.direction,
                          &level.snake.segments[0].x,
                          &level.snake.segments[0].y);
        }

        // Clear window
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        // Draw level
        for(int32_t y = 0; y < level.height; y++) {
            for(int32_t x = 0; x < level.width; x++) {
                // TODO: Asserts
                CellType cell_type = level_get_cell(&level, x, y);

                if (cell_type == CELL_TYPE_EMPTY) {
                    continue;
                }

                switch (cell_type) {
                    case CELL_TYPE_EMPTY: {
                        break;
                    }
                    case CELL_TYPE_WALL: {
                        SDL_SetRenderDrawColor(renderer, 0xAD, 0x62, 0x00, 0xFF);
                        break;
                    }
                    case CELL_TYPE_TACO: {
                        SDL_SetRenderDrawColor(renderer, 0xEE, 0xFF, 0x00, 0xFF);
                        break;
                    }
                    default:
                        break;
                }

                SDL_Rect cell_rect = {
                    .x = x * cell_size,
                    .y = y * cell_size,
                    .w = cell_size,
                    .h = cell_size
                };

                SDL_RenderFillRect(renderer, &cell_rect);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

        for (int i = 0; i < level.snake.length; i++) {
            SDL_Rect r = {
                .x = level.snake.segments[i].x * cell_size,
                .y = level.snake.segments[i].y * cell_size,
                .w = cell_size,
                .h = cell_size
            };

            // *ptr -> the contents of ptr
            // &var -> the address of var

            SDL_RenderFillRect(renderer, &r);
        }

        // Render updates
        SDL_RenderPresent(renderer);

        // Allow process to go to sleep so we don't use 100% of CPU
        SDL_Delay(0);
    }

    level_destroy(&level);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
