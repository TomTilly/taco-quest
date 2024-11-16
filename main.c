#include "game.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#define LEVEL_WIDTH 24
#define LEVEL_HEIGHT 20
#define GAME_SIMULATE_TIME_INTERVAL_US 150000
#define SERVER_ACCEPT_QUEUE_LIMIT 5

typedef enum {
    SESSION_TYPE_SINGLE_PLAYER,
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

S32 main (S32 argc, char** argv) {
    puts("hello taco");

    const char* port = NULL;
    const char* ip = NULL;
    const char* window_title;

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

    int server_socket_fd = -1;
    int client_socket_fd = -1;
    
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC, // don't care IPv4 or IPv6
        .ai_socktype = SOCK_STREAM // TCP stream sockets
    };
    
    struct addrinfo *server_info;  // will point to the results

    switch(session_type) {
    case SESSION_TYPE_SINGLE_PLAYER:
        puts("Starting single player session.\n");
        
        window_title = "Taco Quest";
        
        break;
    case SESSION_TYPE_CLIENT: {
        printf("Starting client session, with address: %s:%s\n", ip, port);
        
        window_title = "Taco Quest (Client)";

        // get ready to connect
        int rc = getaddrinfo(ip, port, &hints, &server_info);
        if (rc != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
            return EXIT_FAILURE;
        }

        // Create client socket file descriptor.
        client_socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
        if (client_socket_fd < 0) {
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        
        // TODO: Set to non-blocking?
        
        rc = connect(client_socket_fd, server_info->ai_addr, server_info->ai_addrlen);
        if (rc == -1) {
            fprintf(stderr, "connect error: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        
        freeaddrinfo(server_info);
        
        printf("Connected to %s:%s\n", ip, port);
        break;
    }
    case SESSION_TYPE_SERVER: {
        printf("Starting server session, with port: %s\n", port);
        
        window_title = "Taco Quest (Server)";

        hints.ai_flags = AI_PASSIVE;

        // See beej's network guide for more details.
        // Lookup network info for server type socket.
        int rc = getaddrinfo(NULL, port, &hints, &server_info);
        if (rc != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
            return EXIT_FAILURE;
        }

        // Create server socket file descriptor.
        server_socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
        if (server_socket_fd < 0) {
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // Set socket file descriptor to non blocking, so we can call accept() without blocking.
        int server_socket_flags = fcntl(server_socket_fd, F_GETFL);
        if (server_socket_flags == -1){
            fprintf(stderr, "fcntl(F_GETFL) failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        server_socket_flags |= O_NONBLOCK;
        rc = fcntl(server_socket_fd, F_SETFL, server_socket_flags);
        if (rc != 0){
            fprintf(stderr, "fcntl(F_SETFL) failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // Bind to a specific port.
        rc = bind(server_socket_fd, server_info->ai_addr, server_info->ai_addrlen);
        if (rc != 0) {
            fprintf(stderr, "bind() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // Listen on the socket for incoming connections.
        rc = listen(server_socket_fd, SERVER_ACCEPT_QUEUE_LIMIT);
        if (rc != 0) {
            fprintf(stderr, "listen() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        freeaddrinfo(server_info);
        break;
    }
    }

    int rc = SDL_Init(SDL_INIT_EVERYTHING);
    if (rc < 0) {
        printf("SDL_Init failed %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    S32 cell_size = 40;
    S32 window_width = LEVEL_WIDTH * cell_size;
    S32 window_height = LEVEL_HEIGHT * cell_size;
    SDL_Window* window = SDL_CreateWindow(window_title,
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
                // TODO: Send command to server
                break;
            case SESSION_TYPE_SERVER: {
                struct sockaddr_storage client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int rc = accept(server_socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (rc != 0) {
                    if (errno != EWOULDBLOCK && errno != EAGAIN){
                        fprintf(stderr, "accept() failed: %s\n", strerror(errno));
                    }
                } else {
                    fprintf(stdout, "received incoming client connection!\n");
                    // TODO: Handle incoming client connection.
                }

                // TODO: check for command from client
                game_update(&game, snake_action, other_snake_action);
                // TODO: Send state to client.
                break;
            }
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

    switch(session_type) {
    case SESSION_TYPE_CLIENT:
        // TODO: close client socket.
        break;
    case SESSION_TYPE_SERVER: {
        close(server_socket_fd);
        // TODO: close any client sockets.
        break;
    }
    case SESSION_TYPE_SINGLE_PLAYER:
        break;
    }

    game_destroy(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
