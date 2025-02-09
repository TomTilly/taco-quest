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

#define LEVEL_WIDTH 24
#define LEVEL_HEIGHT 20
#define GAME_SIMULATE_TIME_INTERVAL_US 150000
#define SERVER_ACCEPT_QUEUE_LIMIT 5

#define SEND_FMT "SEND: %6zu bytes | "
#define RECV_FMT "RECV: %6zu bytes | "
#define PACKET_AFTER_FMT "%5d bytes - seq %5d (%s)\n"
#define AFTER_FMT "%5d bytes (%s)\n"

typedef enum {
    SESSION_TYPE_SINGLE_PLAYER,
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

uint64_t microseconds_between_timestamps(struct timespec* previous, struct timespec* current) {
    return (current->tv_sec - previous->tv_sec) * 1000000LL +
           ((current->tv_nsec - previous->tv_nsec)) / 1000;
}

typedef U8 PacketType;
enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_SNAKE_ACTION,
    PACKET_TYPE_LEVEL_STATE,
    PACKET_TYPE_SNAKE_STATE
};

typedef struct {
    U16 size;
    U16 sequence;
    PacketType type;
} PacketHeader;

typedef U8 ReceiveMessageStage;
enum {
    RECEIVE_MESSAGE_STAGE_PACKET_HEADER,
    RECEIVE_MESSAGE_STAGE_PAYLOAD,
    RECEIVE_MESSAGE_STAGE_COMPLETE,
};

typedef struct {
    ReceiveMessageStage stage;
    int bytes_received;
} ReceiveMessageState;

U16 server_sequence;
U16 client_sequence;

const char* packet_type_description(PacketType type) {
    switch (type) {
        case PACKET_TYPE_NONE:
            return "none";
        case PACKET_TYPE_SNAKE_ACTION:
            return "snake action";
        case PACKET_TYPE_LEVEL_STATE:
            return "level state";
        case PACKET_TYPE_SNAKE_STATE:
            return "snake";
        default:
            return "unknown";
    }
}

int receive_result(NetSocket** socket, int bytes) {
    if (bytes == -1) {
        fputs(net_get_error(), stderr);
        net_destroy_socket(*socket);
        *socket = NULL;
        return false;
    }

    return bytes;
}

int receive_packet(NetSocket** socket, PacketHeader* header, int offset) {
    net_log(RECV_FMT, sizeof(*header));
    int bytes_received = net_receive(*socket, header + offset, sizeof(*header) - offset);
    net_log(PACKET_AFTER_FMT, bytes_received, header->sequence,
            packet_type_description(header->type));

    return receive_result(socket, bytes_received);
}

int receive_payload(NetSocket** socket, void* buffer, int size, const char* description) {
    net_log(RECV_FMT, size);
    int bytes_received = net_receive(*socket, buffer, size);
    net_log(AFTER_FMT, bytes_received, description);

    return receive_result(socket, bytes_received);
}

// returns whether or not to continue running stages.
bool receive_message_stage(NetSocket** socket,
                           PacketHeader* packet_header,
                           PacketType expected_packet_type,
                           uint8_t* payload_buffer,
                           int payload_size,
                           const char* description,
                           ReceiveMessageState* state) {
    switch(state->stage) {
    case RECEIVE_MESSAGE_STAGE_PACKET_HEADER: {
        int bytes_received = receive_packet(socket, packet_header, state->bytes_received);
        if (bytes_received < 0) {
            memset(state, 0, sizeof(*state));
            return false;
        } else if (bytes_received > 0) {
            state->bytes_received += bytes_received;
        } else if (bytes_received == 0) {
            return false;
        }

        if (state->bytes_received == sizeof(*packet_header)) {
            if (packet_header->type != expected_packet_type) {
                fprintf(stderr,
                        "unrecognized packet type %d on sequence number %d\n",
                        packet_header->type,
                        packet_header->sequence);
                memset(state, 0, sizeof(*state));
                return false;
            }

            state->stage = RECEIVE_MESSAGE_STAGE_PAYLOAD;
            state->bytes_received = 0;
        }
        break;
    }
    case RECEIVE_MESSAGE_STAGE_PAYLOAD: {
        int bytes_received = receive_payload(socket,
                                             payload_buffer + state->bytes_received,
                                             payload_size,
                                             description);
        if (bytes_received < 0) {
            memset(state, 0, sizeof(*state));
            return false;
        } else if (bytes_received > 0) {
            state->bytes_received += bytes_received;
        } else if (bytes_received == 0) {
            return false;
        }

        if (state->bytes_received == payload_size) {
            state->stage = RECEIVE_MESSAGE_STAGE_COMPLETE;
        }
        break;
    }
    case RECEIVE_MESSAGE_STAGE_COMPLETE:
    default:
        assert(!"Invalid receive message state!");
        break;
    }

    return true;
}

// TODO: do we really need to pass in PacketHeader ?
void receive_message(NetSocket** socket,
                     PacketHeader* packet_header,
                     PacketType expected_packet_type,
                     uint8_t* payload_buffer,
                     int payload_size,
                     const char* description,
                     ReceiveMessageState* state) {
    while (state->stage != RECEIVE_MESSAGE_STAGE_COMPLETE) {
        if (!receive_message_stage(socket,
                                   packet_header,
                                   expected_packet_type,
                                   payload_buffer,
                                   payload_size,
                                   description,
                                   state)) {
            break;
        }
    }
}

S32 main (S32 argc, char** argv) {
    printf("hello taco, size_t is %zu bytes\n", sizeof(size_t));

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

    NetSocket* server_socket = NULL;
    NetSocket* client_socket = NULL;
    NetSocket* server_client_socket = NULL; // TODO: will be an array to handle multiple connections

    // TODO: Only do this when doing networking
    if (!net_init()) {
        fprintf(stderr, "%s\n", net_get_error());
        return EXIT_FAILURE;
    }

    if (session_type == SESSION_TYPE_SINGLE_PLAYER) {
        // ip = "127.0.0.1";
        port = "12345";
        session_type = SESSION_TYPE_SERVER;
    }

    switch(session_type) {
    case SESSION_TYPE_SINGLE_PLAYER:
        puts("Starting single player session.\n");

        window_title = "Taco Quest";

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

    U8* client_buffer = NULL;
    size_t client_buffer_size = 0;
    size_t client_received = 0;

    ReceiveMessageState receive_client_snake_action_message_state = {0};
    // ReceiveMessageState receive_game_state_message_state = {0};

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

        if (session_type == SESSION_TYPE_CLIENT) {
            // TODO: Check if our socket is still alive, otherwise we have to press a key after
            // the server disconnects.

            // Send our action to the server if there is one.
            if (snake_action != SNAKE_ACTION_NONE) {
                PacketHeader packet_header = {
                    .type = PACKET_TYPE_SNAKE_ACTION,
                    .size = sizeof(snake_action),
                    .sequence = client_sequence++
                };

                net_log(SEND_FMT, sizeof(packet_header));
                int bytes_sent = net_send(client_socket,
                                          &packet_header,
                                          sizeof(packet_header));
                net_log(PACKET_AFTER_FMT, bytes_sent, packet_header.sequence, "snake action packet header");

                if (bytes_sent == -1) {
                    fprintf(stderr, "%s\n", net_get_error());
                    fprintf(stderr, "ending session.\n");
                    // TODO: do we want to exit on action send failure?
                    return EXIT_FAILURE;
                } else if (bytes_sent == sizeof(packet_header)) {

                    // Packet send success:
                    net_log(SEND_FMT, sizeof(snake_action));
                    bytes_sent = net_send(client_socket,
                                          &snake_action,
                                          sizeof(snake_action));
                    net_log(AFTER_FMT, bytes_sent, "snake action");

                    if (bytes_sent == -1) {
                        fprintf(stderr, "%s\n", net_get_error());
                        fprintf(stderr, "ending session.\n");
                        return EXIT_FAILURE;
                    }
                } else {
                    fprintf(stderr, "failed to send entire header for snake action\n");
                }
            }

            // TODO: We've made a mistake, we need to allocate the client buffer between receiving
            // the packet header and the payload.
            //
            // Its weird that client buffer is passed in as null on the first call. Maybe when
            // client_buffer is null, we are able to run the packet header stage explicitly.
            //
            // if (client_buffer == NULL) {
            //     receive_message_packet_header();
            //     if (receive_game_state_message_state.stage == RECEIVE_MESSAGE_STAGE_PAYLOAD) {
            //         allocate client buffer
            //     }
            // }
            // if (client_buffer) {
            //     receive_message();
            //     if (receive_game_state_message_state.stage == RECEIVE_MESSAGE_STAGE_COMPLETE) {
            //         // deserialize and cleanup client_buffer
            //     }
            // }
#if 0

            PacketHeader packet_header;
            receive_message_stage(&server_client_socket,
                                  &packet_header,
                                  PACKET_TYPE_SNAKE_ACTION,
                                  client_buffer,
                                  client_buffer_size,
                                  "game state",
                                  &receive_game_state_message_state);
            if (receive_game_state_message_state.stage == RECEIVE_MESSAGE_STAGE_PAYLOAD) {
                client_buffer_size = packet_header.size;
                client_buffer = malloc(client_buffer_size);
                receive_message_stage(&server_client_socket,
                                      &packet_header,
                                      PACKET_TYPE_SNAKE_ACTION,
                                      client_buffer,
                                      client_buffer_size,
                                      "game state",
                                      &receive_game_state_message_state);
            }

            if (receive_game_state_message_state.stage == RECEIVE_MESSAGE_STAGE_COMPLETE) {
                memset(&receive_game_state_message_state,
                       0,
                       sizeof(receive_game_state_message_state));
                size_t bytes_deserialized = 0;
                // TODO: do we still need this loop now that we have packet
                do {
                    bytes_deserialized += level_deserialize(client_buffer + bytes_deserialized,
                                                            client_buffer_size - bytes_deserialized,
                                                            &game.level);
                    bytes_deserialized += snake_deserialize(client_buffer + bytes_deserialized,
                                                            client_buffer_size - bytes_deserialized,
                                                            game.snakes + 0);
                    bytes_deserialized += snake_deserialize(client_buffer + bytes_deserialized,
                                                            client_buffer_size - bytes_deserialized,
                                                            game.snakes + 1);
                } while ( bytes_deserialized < (size_t)client_buffer_size );

                free(client_buffer);
                client_buffer = NULL;
            }
#else
            // Check if the server has sent us the updated game state.
            if ( client_buffer == NULL ) {
                PacketHeader packet_header;

                if (receive_packet(&client_socket, &packet_header, 0) == sizeof(packet_header)) {
                    if (packet_header.type == PACKET_TYPE_LEVEL_STATE ) {
                        client_buffer_size = packet_header.size;
                        client_buffer = malloc(client_buffer_size);
                        client_received = 0;
                    }
                }
            }

            // If we have a client buffer, we are either reading the start of or continuing in the middle of a packet message.
            if ( client_buffer ) {
                if (receive_payload(&client_socket,
                                    client_buffer + client_received,
                                    (int)(client_buffer_size - client_received),
                                    "game state")) {
                    size_t bytes_deserialized = 0;
                    // TODO: do we still need this loop now that we have packet
                    do {
                        bytes_deserialized += level_deserialize(client_buffer + bytes_deserialized,
                                                                client_buffer_size - bytes_deserialized,
                                                                &game.level);
                        bytes_deserialized += snake_deserialize(client_buffer + bytes_deserialized,
                                                                client_buffer_size - bytes_deserialized,
                                                                game.snakes + 0);
                        bytes_deserialized += snake_deserialize(client_buffer + bytes_deserialized,
                                                                client_buffer_size - bytes_deserialized,
                                                                game.snakes + 1);
                    } while ( bytes_deserialized < (size_t)client_received );

                    free(client_buffer);
                    client_buffer = NULL;
                }
            }
#endif

            snake_action = 0;
        } else if (time_since_update_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            // Update the game state if a tick has passed.
            time_since_update_us -= GAME_SIMULATE_TIME_INTERVAL_US;

            switch(session_type) {
            case SESSION_TYPE_CLIENT: {
                // Nothing to see here
                break;
            }
            case SESSION_TYPE_SERVER: {
                SnakeAction client_snake_action = {0};

                // Server receive input from client, update, then send game state to client
                if (server_client_socket != NULL) {
                    PacketHeader packet_header;

                    receive_message(&server_client_socket,
                                    &packet_header,
                                    PACKET_TYPE_SNAKE_ACTION,
                                    &client_snake_action,
                                    sizeof(client_snake_action),
                                    "client snake action",
                                    &receive_client_snake_action_message_state);
                    if (receive_client_snake_action_message_state.stage == RECEIVE_MESSAGE_STAGE_COMPLETE) {
                        memset(&receive_client_snake_action_message_state,
                               0,
                               sizeof(receive_client_snake_action_message_state));
                    }
#if 0
                    if (receive_packet(&server_client_socket, &packet_header, 0) == sizeof(packet_header)) {
                        if (packet_header.type == PACKET_TYPE_SNAKE_ACTION) {
                            receive_payload(&server_client_socket,
                                            &client_snake_action,
                                            sizeof(client_snake_action),
                                            "client snake action");
                        } else {
                            printf("unrecognized packet up in har: %d\n", packet_header.type);
                        }
                    }
#endif
                }

                game_update(&game, snake_action, client_snake_action);

                // Listen for client connections
                if (server_client_socket == NULL) {
                    bool result = net_accept(server_socket, &server_client_socket);
                    if (result) {
                        if (server_client_socket != NULL) {
                            printf("client connected!\n");
                        }
                    } else {
                        fprintf(stderr, "%s\n", net_get_error());
                    }
                } else {
                    // Serialize game state
                    size_t msg_size = level_serialize(&game.level, net_msg_buffer, net_msg_buffer_size);
                    msg_size += snake_serialize(game.snakes + 0, net_msg_buffer + msg_size, net_msg_buffer_size - msg_size);
                    msg_size += snake_serialize(game.snakes + 1, net_msg_buffer + msg_size, net_msg_buffer_size - msg_size);

                    // Send packet header

                    PacketHeader packet_header = {
                        .type = PACKET_TYPE_LEVEL_STATE,
                        .size = (U16)(msg_size),
                        .sequence = server_sequence++
                    };

                    net_log(SEND_FMT, sizeof(packet_header));
                    int bytes_sent = net_send(server_client_socket,
                                              &packet_header,
                                              sizeof(packet_header));
                    net_log(PACKET_AFTER_FMT, bytes_sent, packet_header.sequence, "level state packet header");

                    if (bytes_sent == -1) {
                        fprintf(stderr, "%s\n", net_get_error());
                    } else if (bytes_sent != sizeof(packet_header)) {
                        printf("sent only %d of %zu bytes of header\n",
                               bytes_sent, sizeof(packet_header));
                    }

                    // Send game state to client.
                    net_log(SEND_FMT, (size_t)msg_size);
                    bytes_sent = net_send(server_client_socket,
                                          net_msg_buffer,
                                          (int)msg_size);
                    net_log(AFTER_FMT, bytes_sent, "game state");

                    if (bytes_sent == -1) {
                        fprintf(stderr, "%s\n", net_get_error());
                    } else if (bytes_sent != (int)msg_size) {
                        assert(bytes_sent == (int)msg_size);
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
        SDL_Delay(1);
    }

    switch(session_type) {
    case SESSION_TYPE_CLIENT:
        net_destroy_socket(client_socket);
        break;
    case SESSION_TYPE_SERVER: {
        net_destroy_socket(server_socket);
        if ( server_client_socket ) {
            net_destroy_socket(server_client_socket);
        }
        break;
    }
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
