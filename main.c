#include "game.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#if defined(PLATFORM_WINDOWS)
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#define INVALID_SOCKET -1
#endif

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
    const char* window_title = NULL;

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

#if defined(PLATFORM_WINDOWS)
    SOCKET server_socket_fd = INVALID_SOCKET;
    SOCKET client_socket_fd = INVALID_SOCKET;
    SOCKET server_client_socket_fd = INVALID_SOCKET;

    {
        WORD wsa_version_requested = MAKEWORD(2, 2);
        WSADATA wsa_data = {0};
        int rc = WSAStartup(wsa_version_requested, &wsa_data);
        if (rc != 0) {
            fprintf(stderr, "WSAStartup() failed with %d\n", rc);
            return EXIT_FAILURE;
        }
    }
#else
    int server_socket_fd = INVALID_SOCKET;
    int client_socket_fd = INVALID_SOCKET;
    int server_client_socket_fd = INVALID_SOCKET; // TODO: will be an array to handle multiple connections
#endif

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
        client_socket_fd = socket(server_info->ai_family, server_info->ai_socktype, (int)server_info->ai_protocol);
        if (client_socket_fd < 0) {
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // TODO: Set to non-blocking?

        rc = connect(client_socket_fd, server_info->ai_addr, (int)server_info->ai_addrlen);
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
        int rc = getaddrinfo("0.0.0.0", port, &hints, &server_info);
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
#if defined(PLATFORM_WINDOWS)
        u_long server_socket_flags = 1;
        rc = ioctlsocket(server_socket_fd, FIONBIO, &server_socket_flags);
        if (rc != NO_ERROR) {
            fprintf(stderr, "ioctlsocket() failed with error: %ld\n", rc);
            return EXIT_FAILURE;
        }
#else
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
#endif

        // Bind to a specific port.
        rc = bind(server_socket_fd, server_info->ai_addr, (int)server_info->ai_addrlen);
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

    SDL_Rect display_size;
    SDL_GetDisplayBounds(0, &display_size);

    int window_x = SDL_WINDOWPOS_CENTERED;
    int window_y = display_size.h / 4;
    S32 cell_size = 40;
    S32 window_width = LEVEL_WIDTH * cell_size;
    S32 window_height = LEVEL_HEIGHT * cell_size;

    if ( session_type == SESSION_TYPE_SERVER ) {
        window_x = display_size.w / 2 - window_width;
    } else if ( session_type == SESSION_TYPE_CLIENT ) {
        window_x = display_size.w / 2;
    } else {
        window_y = SDL_WINDOWPOS_CENTERED;
    }

    SDL_Window* window = SDL_CreateWindow(window_title,
                                          window_x,
                                          window_y,
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

    size_t net_msg_buffer_size = 1024 * 1024;
    char* net_msg_buffer = (char*)malloc(net_msg_buffer_size);

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
                }
                break;
            }
        }

        // Update the game state if a tick has passed.
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            switch(session_type) {
            case SESSION_TYPE_CLIENT: {
                // Send our action to the server if there is one.
                if (snake_action != SNAKE_ACTION_NONE) {
#if defined(PLATFORM_WINDOWS)
                    int size_sent = send(client_socket_fd, (char*)&snake_action, (int)sizeof(snake_action), 0);
                    if ( size_sent == -1 ) {
                        fprintf(stderr, "error sending data: %d\n", WSAGetLastError());
                    }
#else
                    ssize_t size_sent = send(client_socket_fd, &snake_action, sizeof(snake_action), 0);
                    if ( size_sent == -1 ) {
                        fprintf(stderr, "error sending data: %s\n", strerror(errno));
                    }
#endif
                }

                // Check if the server has sent us the updated game state.

#if defined(PLATFORM_WINDOWS)
                int received = recv(client_socket_fd, net_msg_buffer, (int)net_msg_buffer_size, 0);
                assert(received < (int)net_msg_buffer_size);
#else
                ssize_t received = recv(client_socket_fd, net_msg_buffer, net_msg_buffer_size, 0);
                assert(received < (ssize_t)net_msg_buffer_size);
#endif

                if ( received == -1 ) {
                    fprintf(stderr, "error receiving data: %s\n", strerror(errno));
                } else {
                    size_t bytes_deserialized = 0;
                    do {
                        bytes_deserialized += level_deserialize(net_msg_buffer + bytes_deserialized,
                                                                net_msg_buffer_size - bytes_deserialized,
                                                                &game.level);
                        bytes_deserialized += snake_deserialize(net_msg_buffer + bytes_deserialized,
                                                                net_msg_buffer_size - bytes_deserialized,
                                                                game.snakes + 0);
                        bytes_deserialized += snake_deserialize(net_msg_buffer + bytes_deserialized,
                                                                net_msg_buffer_size - bytes_deserialized,
                                                                game.snakes + 1);
                    } while ( bytes_deserialized < (size_t)received );
                }

                break;
            }
            case SESSION_TYPE_SERVER: {
                struct sockaddr_storage client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

                SnakeAction client_snake_action = {0};

                if ( server_client_socket_fd != INVALID_SOCKET ) {

#if defined(PLATFORM_WINDOWS)
                    int received = recv(server_client_socket_fd, (char*)&client_snake_action, (int)sizeof(client_snake_action), 0);
                    if (received < 0) {
                        int last_error = WSAGetLastError();
                        if (last_error != WSAEWOULDBLOCK) {
                            fprintf(stderr, "recv() error: %d\n", last_error);
                        }
                    }
#else
                    ssize_t received = recv(server_client_socket_fd, &client_snake_action, sizeof(client_snake_action), 0);
                    if (received < 0) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN){
                            fprintf(stderr, "recv() error?: %s\n", strerror(errno));
                        }
                    }
#endif
                }

                game_update(&game, snake_action, client_snake_action);

                if ( server_client_socket_fd == INVALID_SOCKET ) {
                    server_client_socket_fd = accept(server_socket_fd,
                                                     (struct sockaddr *)&client_addr,
                                                     &client_addr_len);

                    // socket is still -1 on error:
                    if (server_client_socket_fd == INVALID_SOCKET) {
#if defined(PLATFORM_WINDOWS)
                        int last_error = WSAGetLastError();
                        if (last_error != WSAEWOULDBLOCK) {
                            fprintf(stderr, "accept() failed: %d\n", last_error);
                        }
#else
                        if (errno != EWOULDBLOCK && errno != EAGAIN){
                            fprintf(stderr, "accept() failed: %s\n", strerror(errno));
                        }
#endif
                    } else {
                        printf("client connected!\n");
                    }
                } else {
                    size_t msg_size = level_serialize(&game.level, net_msg_buffer, net_msg_buffer_size);
                    msg_size += snake_serialize(game.snakes + 0, net_msg_buffer + msg_size, net_msg_buffer_size - msg_size);
                    msg_size += snake_serialize(game.snakes + 1, net_msg_buffer + msg_size, net_msg_buffer_size - msg_size);

#if defined(PLATFORM_WINDOWS)
                    int size_sent = send(server_client_socket_fd,
                                         net_msg_buffer, (int)msg_size,
                                         0);
#else
                    ssize_t size_sent = send(server_client_socket_fd,
                                             net_msg_buffer, msg_size,
                                             0);
#endif

                    if ( size_sent == -1 ) {
                        fprintf(stderr, "send() error?: %s\n", strerror(errno));
                    }

                    if ( (size_t)size_sent != msg_size ) {
                        printf("sent only %zu bytes\n", msg_size);
                    }
                }

                break;
            }
            case SESSION_TYPE_SINGLE_PLAYER: {
                SnakeAction other_snake_action = {0};
                game_update(&game, snake_action, other_snake_action);
                break;
            }
            }

            // Clear actions.
            snake_action = 0;
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
#if defined(PLATFORM_WINDOWS)
        closesocket(client_socket_fd);
#else
        close(client_socket_fd);
#endif
        break;
    case SESSION_TYPE_SERVER: {
#if defined(PLATFORM_WINDOWS)
        closesocket(server_socket_fd);
        closesocket(server_client_socket_fd);
#else
        close(server_socket_fd);
        close(server_client_socket_fd);
#endif
        break;
    }
    case SESSION_TYPE_SINGLE_PLAYER:
        break;
    }

    free(net_msg_buffer);
    game_destroy(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
