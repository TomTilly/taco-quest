#include "game.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <SDL2/SDL.h>

#include "network.h"
#include "packet.h"
#include "pixelfont.h"

#define MS_TO_US(ms) ((ms) * 1000)
#define GAME_SIMULATE_TIME_INTERVAL_US MS_TO_US(175) // default = 150
#define SERVER_ACCEPT_QUEUE_LIMIT 5

// Minus one due to the server itself not needing a client socket.
#define MAX_SERVER_CLIENT_COUNT (MAX_SNAKE_COUNT - 1)

typedef enum {
    SESSION_TYPE_SINGLE_PLAYER,
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

static int __tick;

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

char* get_timestamp(void) {
    struct timespec ts = {0};
    timespec_get(&ts, TIME_UTC);
    static char buff[100];
    size_t length = strftime(buff, sizeof(buff), "%T", gmtime(&ts.tv_sec));
    long ms = ts.tv_nsec / 1000000;
    snprintf(buff + length, sizeof(buff) - length, ".%03ld", ms);
    return buff;
}

void net_action_log(const char* timestamp_str,
                    const char* type,
                    size_t bytes_to_send,
                    size_t bytes_sent,
                    int seq,
                    const char* desc) {
    net_log("%s [tk %5d] %s: %4zu of %4zu bytes | ", timestamp_str, __tick, type, bytes_to_send, bytes_sent);
    if ( seq != -1 ) {
        net_log(" seq %3d (%s)\n", seq, desc);
    } else {
        net_log("         (%s)\n", desc);
    }
}

float score_to_percent(S32 score) {
    return ((float)(score) / (float)(SCORE_TO_WIN)) * 100.0f;
}

char longest_snake_text_indicator(bool is_longest_snake) {
    return is_longest_snake ? '*' : ' ';
}

void reset_game(Game* game) {
    Level* level = &game->level;

    snake_spawn(game->snakes + 0,
                3,
                2,
                DIRECTION_EAST);

    snake_spawn(game->snakes + 1,
                (S16)(level->width - 3),
                2,
                DIRECTION_SOUTH);

    snake_spawn(game->snakes + 2,
                (S16)(level->width - 3),
                (S16)(level->height - 3),
                DIRECTION_WEST);

    const char tile_map[LEVEL_HEIGHT][LEVEL_WIDTH + 1] = {
        "XXXXXXXXXXXXXXXXXXXXXXXX",
        "X......................X",
        "X......................X",
        "X.....X................X",
        "X.....X................X",
        "X.....X................X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X.....X.....XXXX.......X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X.....X........X.......X",
        "X..............XXXXX...X",
        "X......................X",
        "X......................X",
        "X......................X",
        "X.................X....X",
        "X....XXXXX........X....X",
        "X.................X....X",
        "X......................X",
        "XXXXXXXXXXXXXXXXXXXXXXXX",
    };

    for (S32 y = 0; y < level->height; y++) {
        for (S32 x = 0; x < level->width; x++) {
            if (tile_map[y][x] == 'X') {
                level_set_cell(level, x, y, CELL_TYPE_WALL);
            } else {
                level_set_cell(level, x, y, CELL_TYPE_EMPTY);
            }
        }
    }
}

int main(S32 argc, char** argv) {
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

    //
    // Init game and level
    //
    Game game = {0};
    game_init(&game, LEVEL_WIDTH, LEVEL_HEIGHT);

    Level* level = &game.level;

    reset_game(&game);

    NetSocket* server_socket = NULL; // Used by server to listen for client connections.
    NetSocket* client_socket = NULL; // Used by client to send and receive.

    NetSocket* server_client_sockets[MAX_SERVER_CLIENT_COUNT] = { 0 };

    // TODO: Only do this when doing networking
    const char* net_log_file_name = session_type == SESSION_TYPE_SERVER ?
        "net_server.log" : "net_client.log";
    if (!net_init(net_log_file_name)) {
        fprintf(stderr, "%s\n", net_get_error());
        return EXIT_FAILURE;
    }

    switch(session_type) {
    case SESSION_TYPE_SINGLE_PLAYER:
        puts("Starting single player session.\n");

        window_title = "Taco Quest";

        game.state = GAME_STATE_PLAYING;

        break;
    case SESSION_TYPE_CLIENT: {
        printf("Starting client session, with address: %s:%s\n", ip, port);

        window_title = "Taco Quest (Client)";

        net_log("CLIENT\n");

        client_socket = net_create_client(ip, port);
        if ( client_socket == NULL ) {
            fprintf(stderr, "%s\n", net_get_error());
            return EXIT_FAILURE;
        }

        printf("Connected to %s:%s\n", ip, port);
        break;
    }
    case SESSION_TYPE_SERVER: {
        printf("Starting server session, with port: %s\n", port);

        window_title = "Taco Quest (Server)";

        net_log("SERVER\n");

        server_socket = net_create_server(port);
        if ( server_socket == NULL ) {
            fputs(net_get_error(), stderr);
            return EXIT_FAILURE;
        }

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

    S32 min_display_dimension = (display_size.h < display_size.w) ? display_size.h : display_size.w;
    S32 max_level_dimension = (LEVEL_HEIGHT > LEVEL_WIDTH) ? LEVEL_HEIGHT : LEVEL_WIDTH;
    // stupid hack to account for title bar and start menu.
    min_display_dimension -= 100;
    // Make sure the window dimention is a multiple of the level dimension so the level fits in the
    // window.
    min_display_dimension -= (min_display_dimension % max_level_dimension);

    int window_x = SDL_WINDOWPOS_CENTERED;
    int window_y = display_size.h / 4;
    S32 window_width = min_display_dimension;
    S32 window_height = min_display_dimension;

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

    PF_Config font_config = {
        .renderer = renderer,
        .bmp_file = "assets/arcade.bmp",
        .char_width = 8,
        .char_height = 8,
        .first_char = ' ',
        .bmp_background = { 0, 0, 0, 0 }
    };
    PF_Font * font = PF_LoadFont(&font_config);

    struct timespec last_frame_timestamp = {0};
    timespec_get(&last_frame_timestamp, TIME_UTC);

    // Seed random with time.
    srand((U32)(time(NULL)));

    const char* snake_bitmap_filepath = "assets/sprite-sheet.bmp";
    SDL_Surface* snake_surface = SDL_LoadBMP(snake_bitmap_filepath);
    if (snake_surface == NULL) {
        fprintf(stderr, "Failed to load bitmap %s: %s\n", snake_bitmap_filepath, SDL_GetError());
        return EXIT_FAILURE;
    }
    SDL_Texture* snake_texture = SDL_CreateTextureFromSurface(renderer, snake_surface);
    if (snake_surface == NULL) {
        fprintf(stderr, "Failed to create texture from surface %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    int64_t time_since_update_us = 0;

    ActionBuffer server_actions = { 0 };
    ActionBuffer clients_actions[MAX_SERVER_CLIENT_COUNT] = { 0 };

    size_t net_msg_buffer_size = 1024 * 1024;
    char* net_msg_buffer = (char*)malloc(net_msg_buffer_size);

    U16 server_sequence = 0;
    U16 client_sequence = 0;
    Packet client_receive_packet = {0};
    Packet server_receive_packets[MAX_SERVER_CLIENT_COUNT] = {0};
    PacketTransmissionState recv_game_state_state = {0};
    PacketTransmissionState recv_snake_action_states[MAX_SERVER_CLIENT_COUNT] = {0};

    bool quit = false;
    while (!quit) {
        // Calculate how much time has elapsed (in microseconds).
        struct timespec current_frame_timestamp = {0};
        timespec_get(&current_frame_timestamp, TIME_UTC);
        time_since_update_us += microseconds_between_timestamps(&last_frame_timestamp, &current_frame_timestamp);
        last_frame_timestamp = current_frame_timestamp;

        SnakeAction snake_action = SNAKE_ACTION_NONE;

        // Handle events, such as input or window changes.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                if (game.state == GAME_STATE_PLAYING) {
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
                    case SDLK_SPACE:
                        snake_action |= SNAKE_ACTION_CHOMP;
                        break;
                    }
                } else if (game.state == GAME_STATE_WAITING) {
                    if (event.key.keysym.sym == SDLK_RETURN && session_type == SESSION_TYPE_SERVER) {
                        game.state = GAME_STATE_PLAYING;
                    }
                } else if (game.state == GAME_STATE_GAME_OVER) {
                    if (event.key.keysym.sym == SDLK_RETURN &&
                        (session_type == SESSION_TYPE_SERVER || session_type == SESSION_TYPE_SINGLE_PLAYER)) {
                        reset_game(&game);
                        game.state = GAME_STATE_PLAYING;
                    }
                }
                break;
            }
        }

        // The server/single player mode should only update the game state if a tick has passed.
        bool game_should_tick = false;
        if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            if (game.state == GAME_STATE_PLAYING) {
                game_should_tick = true;
                __tick++;
            }
        }

        switch (session_type) {
        case SESSION_TYPE_CLIENT: {
            // TODO: Check if our socket is still alive, otherwise we have to press a key after
            // the server disconnects.

            // Send our action to the server if there is one.
            if (snake_action != SNAKE_ACTION_NONE) {
                Packet packet = {
                    .header = {
                        .type = PACKET_TYPE_SNAKE_ACTION,
                        .payload_size = sizeof(snake_action),
                        .sequence = client_sequence++
                    },
                    .payload = (U8*)&snake_action
                };

                if (!packet_send(client_socket, &packet)) {
                    fprintf(stderr, "failed to send entire header for snake action\n");
                }
            }

            packet_receive(client_socket,
                           &client_receive_packet,
                           &recv_game_state_state);

            if ( recv_game_state_state.stage == PACKET_PROGRESS_STAGE_COMPLETE ) {
                if (client_receive_packet.header.type == PACKET_TYPE_LEVEL_STATE) {
                    game_deserialize(client_receive_packet.payload,
                                     client_receive_packet.header.payload_size,
                                     &game);
#if 0
                    size_t bytes_deserialized = 0;

                    bytes_deserialized += level_deserialize(client_receive_packet.payload + bytes_deserialized,
                                                            client_receive_packet.header.payload_size - bytes_deserialized,
                                                            &game.level);
                    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
                        bytes_deserialized += snake_deserialize(client_receive_packet.payload + bytes_deserialized,
                                                                client_receive_packet.header.payload_size - bytes_deserialized,
                                                                game.snakes + s);
                    }
#endif

                    memset(&recv_game_state_state, 0, sizeof(recv_game_state_state));
                    free(client_receive_packet.payload);
                    memset(&client_receive_packet, 0, sizeof(client_receive_packet));
                }
            } else if ( recv_game_state_state.stage == PACKET_PROGRESS_STAGE_ERROR ) {
                if ( client_receive_packet.payload != NULL ) {
                    free(client_receive_packet.payload);
                }
                memset(&client_receive_packet, 0, sizeof(client_receive_packet));
                fputs(net_get_error(), stderr);
                net_destroy_socket(client_socket);
                client_socket = NULL;
            }

            snake_action = 0;
            break;
        }
        case SESSION_TYPE_SERVER: {
            // Every frame:
            action_buffer_add(&server_actions, snake_action, game.snakes[0].direction);

            // Server receive input from client, update, then send game state to client
            // TODO: receive multiple snake actions, handle the last one.
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                if (server_client_sockets[i] != NULL) {
                    packet_receive(server_client_sockets[i],
                                   &server_receive_packets[i],
                                   &recv_snake_action_states[i]);

                    if (recv_snake_action_states[i].stage == PACKET_PROGRESS_STAGE_COMPLETE) {
                        SnakeAction client_snake_action = *(SnakeAction*)server_receive_packets[i].payload;
                        action_buffer_add(&clients_actions[i], client_snake_action, game.snakes[i + 1].direction);

                        // Resent packet state
                        memset(&recv_snake_action_states[i], 0, sizeof(recv_snake_action_states[i]));
                        free(server_receive_packets[i].payload);
                        memset(&server_receive_packets[i], 0, sizeof(server_receive_packets[i]));
                    } else if ( recv_snake_action_states[i].stage == PACKET_PROGRESS_STAGE_ERROR ) {
                        if ( server_receive_packets[i].payload != NULL ) {
                            free(server_receive_packets[i].payload);
                        }
                        memset(&server_receive_packets[i], 0, sizeof(server_receive_packets[i]));
                        fputs(net_get_error(), stderr);
                        net_destroy_socket(server_client_sockets[i]);
                        server_client_sockets[i] = NULL;
                    }
                }
            }

            if (!game_should_tick) {
                break;
            }

            // Every Tick:
            SnakeAction snake_actions[MAX_SNAKE_COUNT];
            snake_actions[0] = action_buffer_remove(&server_actions);
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                snake_actions[i + 1] = action_buffer_remove(&clients_actions[i]);
            }
            game_update(&game, snake_actions);

            // Listen for client connections
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                if (server_client_sockets[i] == NULL) {
                    bool result = net_accept(server_socket, &server_client_sockets[i]);
                    if (result) {
                        if (server_client_sockets[i] != NULL) {
                            printf("client connected!\n");
                        }
                    } else {
                        fprintf(stderr, "%s\n", net_get_error());
                    }
                } else {
                    // Serialize game state
                    size_t msg_size = game_serialize(&game,
                                                     net_msg_buffer,
                                                     net_msg_buffer_size);
#if 0
                    size_t msg_size = level_serialize(&game.level, net_msg_buffer, net_msg_buffer_size);
                    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
                        msg_size += snake_serialize(game.snakes + s, net_msg_buffer + msg_size, net_msg_buffer_size - msg_size);
                    }
#endif

                    // Send packet header
                    Packet packet = {
                        .header = {
                            .type = PACKET_TYPE_LEVEL_STATE,
                            .payload_size = (U16)(msg_size),
                            .sequence = server_sequence++
                        },
                        .payload = (U8*)net_msg_buffer
                    };

                    if (!packet_send(server_client_sockets[i], &packet)) {
                        printf("failed to send game state for tick: %d\n", __tick);
                    }
                }
            }

            break;
        }
        case SESSION_TYPE_SINGLE_PLAYER: {

            if ( snake_action != SNAKE_ACTION_NONE ) {
                action_buffer_add(&server_actions, snake_action,game.snakes[0].direction);
            }

            if (!game_should_tick) {
                break;
            }

            SnakeAction snake_actions[MAX_SNAKE_COUNT];
            snake_actions[0] = action_buffer_remove(&server_actions);
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                snake_actions[i + 1] = SNAKE_ACTION_NONE;
            }
            game_update(&game, snake_actions);
            break;
        }
        }

        //
        // Render game
        //

        // Clear window
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        // Draw level
        S32 cell_size = min_display_dimension / max_level_dimension;
        float scale = (float)(cell_size / 16); // 16 is the sprite sheet tile size.

        for(S32 y = 0; y < level->height; y++) {
            for(S32 x = 0; x < level->width; x++) {
                // TODO: Asserts
                CellType cell_type = level_get_cell(level, x, y);

                SDL_Rect cell_rect = {
                    .x = x * cell_size,
                    .y = y * cell_size,
                    .w = cell_size,
                    .h = cell_size
                };

                if (((x + y) % 2) == 0) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x22, 0x22, 0x22, 0xFF);
                }
                SDL_RenderFillRect(renderer, &cell_rect);

                if (cell_type == CELL_TYPE_EMPTY) {
                    continue;
                }

                switch (cell_type) {
                    case CELL_TYPE_EMPTY: {
                        break;
                    }
                    case CELL_TYPE_WALL: {
                        SDL_SetRenderDrawColor(renderer, 0x55, 0x55, 0x55, 0xFF);
                        SDL_RenderFillRect(renderer, &cell_rect);
                        break;
                    }
                    case CELL_TYPE_TACO: {
                        SDL_Rect source_rect = {64, 0, 16, 16};
                        rc = SDL_RenderCopy(renderer,
                                                snake_texture,
                                                &source_rect,
                                                &cell_rect);
                        if (rc != 0) {
                            fprintf(stderr, "Tom F was wrong: %s\n", SDL_GetError());
                            return EXIT_FAILURE;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
            snake_draw(renderer, snake_texture, game.snakes + s, s, cell_size);
        }
        SDL_SetTextureColorMod(snake_texture, 255, 255, 255);

        PF_SetScale(font, scale * 2.0f);
        PF_SetForeground(font, 255, 255, 255, 255);
        switch(game.state) {
        case GAME_STATE_WAITING:
            switch(session_type) {
            case SESSION_TYPE_CLIENT:
                PF_RenderString(font, 0, 0, "Waiting for game to start");
                break;
            case SESSION_TYPE_SERVER:
                PF_RenderString(font, 0, 0, "Press enter to start game");
                break;
            case SESSION_TYPE_SINGLE_PLAYER:
                break;
            }
            break;
        case GAME_STATE_PLAYING: {
            S32 longest_snake_length = 0;
            S32 longest_snake_index = 0;
            for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
                if (game.snakes[s].length >= longest_snake_length) {
                    longest_snake_length = game.snakes[s].length;
                    longest_snake_index = s;
                }
            }
            PF_SetHorizontalPositioning(font, PF_HPOS_RIGHT);
            PF_SetForeground(font, 0, 255, 0, 255);
            PF_RenderString(font, cell_size / 2, 0, "%.0f%%%c", score_to_percent(game.snakes[0].score), longest_snake_text_indicator(longest_snake_index == 0));
            PF_SetForeground(font, 255, 125, 125, 255);
            PF_RenderString(font, min_display_dimension - (S32)(2.5 * cell_size), 0, "%.0f%%%c", score_to_percent(game.snakes[1].score), longest_snake_text_indicator(longest_snake_index == 1));
            PF_SetForeground(font, 0, 255, 255, 255);
            PF_RenderString(font, min_display_dimension - (S32)(2.5 * cell_size), (min_display_dimension - cell_size), "%.0f%%%c", score_to_percent(game.snakes[2].score), longest_snake_text_indicator(longest_snake_index == 2));
            break;
        }
        case GAME_STATE_GAME_OVER: {
            const char* winning_player = "invalid";
            if (game.snakes[0].score == SCORE_TO_WIN) {
                winning_player = "Green";
            } else if (game.snakes[1].score == SCORE_TO_WIN) {
                winning_player = "Pink";
            } else if (game.snakes[2].score == SCORE_TO_WIN) {
                winning_player = "Cyan";
            }
            PF_RenderString(font, 0, 0, "Game Over! %s Snake Wins !", winning_player);
            break;
        }
        }


        // Render updates
        SDL_RenderPresent(renderer);

        // Allow process to go to sleep so we don't use 100% of CPU
        SDL_Delay(1);
    }

    switch(session_type) {
    case SESSION_TYPE_CLIENT:
        if ( client_socket ) {
            net_destroy_socket(client_socket);
        }
        break;
    case SESSION_TYPE_SERVER:
        net_destroy_socket(server_socket);
        for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
            if ( server_client_sockets[i] ) {
                net_destroy_socket(server_client_sockets[i]);
            }
        }
        break;
    case SESSION_TYPE_SINGLE_PLAYER:
        break;
    }

    net_shutdown();
    free(net_msg_buffer);
    game_destroy(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
