#include "game.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <SDL2/SDL.h>

#define LEVEL_WIDTH 24
#define LEVEL_HEIGHT 20
#define GAME_SIMULATE_TIME_INTERVAL_US 150000

typedef enum {
    SESSION_TYPE_SINGLE_PLAYER,
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

S32 main (int argc, char** argv) {
    puts("hello taco");

    const char* port = NULL;
    const char* ip = NULL;

    SessionType session_type = SESSION_TYPE_SINGLE_PLAYER;
    if(argc > 1) {
        if (strcmp(argv[1], "-s") == 0) {
            session_type = SESSION_TYPE_SERVER;

            if (argc != 3) {
                puts("Expected port argument for server mode");
                return EXIT_FAILURE;
            }

            port = argv[2];
        } else if (strcmp(argv[1], "-c") == 0) {
            session_type = SESSION_TYPE_CLIENT;

            if (argc != 4) {
                puts("Expected ip and port arguments for client mode");
                return EXIT_FAILURE;
            }

            ip = argv[2];
            port = argv[3];
        } else {
            puts("Unexpected argument passed");
            return EXIT_FAILURE;
        }
    }

    switch(session_type) {
    case SESSION_TYPE_SINGLE_PLAYER:
        puts("Starting single player session.\n");
        break;
    case SESSION_TYPE_CLIENT:
        printf("Starting client session, with address: %s:%s\n", ip, port);
        break;
    case SESSION_TYPE_SERVER:
        printf("Starting server session, with port: %s\n", port);
        break;
    }

    int rc = SDL_Init(SDL_INIT_EVERYTHING);
    if (rc < 0) {
        printf("SDL_Init failed %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    S32 cell_size = 40;
    S32 window_width = LEVEL_WIDTH * cell_size;
    S32 window_height = LEVEL_HEIGHT * cell_size;
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

    Game game = {0};
    game_init(&game, LEVEL_WIDTH, LEVEL_HEIGHT);

    Level* level = &game.level;

    level_set_cell(level, 9, 5, CELL_TYPE_TACO);
    level_set_cell(level, 14, 3, CELL_TYPE_TACO);
    level_set_cell(level, 15, 9, CELL_TYPE_TACO);
    level_set_cell(level, 2, 2, CELL_TYPE_TACO);
    level_set_cell(level, 8, 10, CELL_TYPE_TACO);

    snake_spawn(game.snakes + 0,
                level->width / 3,
                level->height / 2,
                DIRECTION_EAST);

    snake_spawn(game.snakes + 1,
                (level->width / 3) + (level->width / 3),
                level->height / 2,
                DIRECTION_EAST);

    for (S32 y = 0; y < level->height; y++) {
        for (S32 x = 0; x < level->width; x++) {
            if (x == 0 || x == level->width - 1) {
                level_set_cell(level, x, y, CELL_TYPE_WALL);
            }

            if (y == 0 || y == level->height - 1) {
                level_set_cell(level, x, y, CELL_TYPE_WALL);
            }
        }
    }

    struct timespec last_frame_timestamp = {0};
    timespec_get(&last_frame_timestamp, TIME_UTC);

    // Seed random with time.
    srand((U32)(time(NULL)));

    int64_t time_since_update_us = 0;

    SnakeAction snake_action = SNAKE_ACTION_NONE;
    SnakeAction other_snake_action = SNAKE_ACTION_NONE;

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
                    snake_action |= SNAKE_ACTION_FACE_NORTH;
                    break;
                case SDLK_a:
                    snake_action |= SNAKE_ACTION_FACE_WEST;
                    break;
                case SDLK_s:
                    snake_action |= SNAKE_ACTION_FACE_SOUTH;
                    break;
                case SDLK_d:
                    snake_action |= SNAKE_ACTION_FACE_EAST;
                    break;
                case SDLK_UP:
                    other_snake_action |= SNAKE_ACTION_FACE_NORTH;
                    break;
                case SDLK_LEFT:
                    other_snake_action |= SNAKE_ACTION_FACE_WEST;
                    break;
                case SDLK_DOWN:
                    other_snake_action |= SNAKE_ACTION_FACE_SOUTH;
                    break;
                case SDLK_RIGHT:
                    other_snake_action |= SNAKE_ACTION_FACE_EAST;
                    break;
                }
                break;
            }
        }

        // Update the game state if a tick has passed.
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            switch(session_type) {
            case SESSION_TYPE_CLIENT:
                // TODO: check for updated state from server.
                break;
            case SESSION_TYPE_SERVER:
                // TODO: check for command from client
                game_update(&game, snake_action, other_snake_action);
                // TODO: Send state to client.
                break;
            case SESSION_TYPE_SINGLE_PLAYER:
                game_update(&game, snake_action, other_snake_action);
                break;
            }

            // Clear actions.
            snake_action = 0;
            other_snake_action = 0;
        }

        // Clear window
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        // Draw level
        for(S32 y = 0; y < level->height; y++) {
            for(S32 x = 0; x < level->width; x++) {
                // TODO: Asserts
                CellType cell_type = level_get_cell(level, x, y);

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

        for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
            snake_draw(renderer, game.snakes + s, cell_size);
        }

        // Render updates
        SDL_RenderPresent(renderer);

        // Allow process to go to sleep so we don't use 100% of CPU
        SDL_Delay(0);
    }

    game_destroy(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
