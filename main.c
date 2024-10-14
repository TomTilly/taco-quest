#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "SDL2/SDL.h"

#define LEVEL_WIDTH 16
#define LEVEL_HEIGHT 9
#define CELL_SIZE 32

typedef enum { EMPTY, WALL, SNAKE_HEAD, SNAKE_BODY, TACO } CellType;

typedef struct { 
    CellType* cells;
    int32_t width;
    int32_t height;
} Level;

bool level_init(Level* level, int32_t width, int32_t height) {
    int32_t size = width * height * sizeof(CellType);
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

bool level_set_cell(Level* level, int32_t x, int32_t y, CellType value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->height) {
        return false;
    }

    int32_t index = y * level->width + x;
    level->cells[index] = value;
    return true;
}

bool level_get_cell(Level* level, int32_t x, int32_t y, CellType* value) {
    if (x < 0 || x >= level->width) {
        return false;
    }

    if (y < 0 || y >= level->width) {
        return false;
    }
    
    int32_t index = y * level->width + x;
    *value = level->cells[index];
    return true;
}

int32_t main () {
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
    level_set_cell(&level, 7, 3, SNAKE_HEAD);
    level_set_cell(&level, 6, 3, SNAKE_BODY);
    level_set_cell(&level, 9, 5, TACO);
    for (int32_t y = 0; y < level.height; y++) {
        for (int32_t x = 0; x < level.width; x++) {
            if (x == 0 || x == level.width - 1) {
                level_set_cell(&level, x, y, WALL);
            }

            if (y == 0 || y == level.height - 1) {
                level_set_cell(&level, x, y, WALL);
            }
        }
    }

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
                break;
            }
        }

        // Clear window
        SDL_FillRect(window_surface, NULL, SDL_MapRGB(window_surface->format, 0x00, 0x00, 0x00));

        // Draw level
        for(int32_t y = 0; y < level.height; y++) {
            for(int32_t x = 0; x < level.width; x++) {
                CellType cell_type;
                // TODO: Asserts
                bool status = level_get_cell(&level, x, y, &cell_type);

                if (!status) {
                    printf("Coordinates outside of cell bounds\n");
                    continue;
                }

                if (cell_type == EMPTY) {
                    continue;
                }

                uint32_t rgb;

                switch (cell_type) {
                    case EMPTY: {
                        break;
                    }
                    case WALL: {
                        rgb = SDL_MapRGB(window_surface->format, 0xAD, 0x62, 0x00);
                        break;
                    }
                    case SNAKE_HEAD: {
                        rgb = SDL_MapRGB(window_surface->format, 0x54, 0xFF, 0x00);
                        break;
                    }
                    case SNAKE_BODY: {
                        rgb = SDL_MapRGB(window_surface->format, 0x86, 0xFF, 0x49);
                        break;
                    }
                    case TACO: {
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
