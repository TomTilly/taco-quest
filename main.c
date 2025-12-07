#include "game.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <SDL2/SDL.h>

#include "dev_mode.h"
#include "lobby.h"
#include "network.h"
#include "packet.h"
#include "pixelfont.h"
#include "ui.h"

#define MS_TO_US(ms) ((ms) * 1000)
#define SERVER_ACCEPT_QUEUE_LIMIT 5
#define MAX_GAME_CONTROLLERS 4

// Minus one due to the server itself not needing a client socket.
#define MAX_SERVER_CLIENT_COUNT (MAX_SNAKE_COUNT - 1)

typedef enum {
    APP_STATE_LOBBY,
    APP_STATE_GAME,
} AppState;

typedef enum {
    SESSION_TYPE_SINGLE_PLAYER,
    SESSION_TYPE_SERVER,
    SESSION_TYPE_CLIENT
} SessionType;

typedef struct {
    Game game;
    SnakeActionKeyState prev_snake_actions_key_states[MAX_SNAKE_COUNT];
    SnakeAction snake_actions[MAX_SNAKE_COUNT];
    ActionBuffer action_buffers[MAX_SNAKE_COUNT];
    DevMode dev_mode;
} AppStateGameServer;

typedef struct {
    Game game;
    SnakeActionKeyState prev_action_key_state;
    SnakeAction snake_actions;
} AppStateGameClient;

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

void reset_game(Game* game,
                AppStateLobby* lobby_state) {
    game->max_taco_count = lobby_state->game_settings.taco_count;

    Level* level = &game->level;

    S32 snake_count = 0;
    for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
        if (lobby_state->players[p].state != LOBBY_PLAYER_STATE_NONE) {
            snake_count++;
        }
    }

    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        game->snakes[i].length = 0;
        game->snakes[i].life_state = SNAKE_LIFE_STATE_DEAD;
    }

    snake_spawn(game->snakes + 0,
                3,
                2,
                DIRECTION_EAST,
                lobby_state->game_settings.starting_length,
                (S8)(lobby_state->game_settings.segment_health));

    game->snakes[0].color = lobby_state->players[0].snake_color;

    snake_spawn(game->snakes + 1,
                (S16)(level->width - 3),
                2,
                DIRECTION_SOUTH,
                lobby_state->game_settings.starting_length,
                (S8)(lobby_state->game_settings.segment_health));

    if (snake_count > 1) {
        game->snakes[1].color = lobby_state->players[1].snake_color;
    } else {
        game->snakes[1].color = (lobby_state->players[0].snake_color + 1) % SNAKE_COLOR_COUNT;
    }

    if (snake_count > 2) {
        snake_spawn(game->snakes + 2,
                    (S16)(level->width - 2),
                    (S16)(level->height - 3),
                    DIRECTION_WEST,
                    lobby_state->game_settings.starting_length,
                    (S8)(lobby_state->game_settings.segment_health));
        game->snakes[2].color = lobby_state->players[2].snake_color;
    }

    if (snake_count > 3) {
        snake_spawn(game->snakes + 3,
                    3,
                    (S16)(level->height - 3),
                    DIRECTION_EAST,
                    lobby_state->game_settings.starting_length,
                    (S8)(lobby_state->game_settings.segment_health));
        game->snakes[3].color = lobby_state->players[3].snake_color;
    }

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
        "X................X.....X",
        "X....XXXXX.......X.....X",
        "X................X.....X",
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

bool draw_game(Game* game,
               SDL_Renderer* renderer,
               SDL_Texture* snake_texture,
               S32 cell_size,
               S32 max_segment_health) {
    // draw level
    for(S32 y = 0; y < game->level.height; y++) {
        for(S32 x = 0; x < game->level.width; x++) {
            // TODO: Asserts
            CellType cell_type = level_get_cell(&game->level, x, y);

            SDL_Rect cell_rect = {
                .x = x * cell_size,
                .y = y * cell_size,
                .w = cell_size,
                .h = cell_size
            };

            if (((x + y) % 2) == 0) {
                SDL_SetRenderDrawColor(renderer, 0x11, 0x11, 0x11, 0xFF);
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
                    SDL_SetRenderDrawColor(renderer, 0x59, 0x44, 0x2a, 0xFF);
                    SDL_RenderFillRect(renderer, &cell_rect);
                    break;
                }
                case CELL_TYPE_TACO: {
                    SDL_Rect source_rect = {64, 0, 16, 16};
                    int rc = SDL_RenderCopy(renderer,
                                        snake_texture,
                                        &source_rect,
                                        &cell_rect);
                    if (rc != 0) {
                        fprintf(stderr, "Tom F was wrong: %s\n", SDL_GetError());
                        return false;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // draw snakes
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        snake_draw(renderer, snake_texture, game->snakes + s, cell_size, max_segment_health);
    }

    return true;
}

bool app_game_server_handle_keystate(AppStateGameServer* app_game_server,
                                     const U8* keyboard_state,
                                     S32 cell_size,
                                     bool use_keyboard_for_snake_actions) {
    switch (app_game_server->game.state) {
    case GAME_STATE_PLAYING: {
        if (use_keyboard_for_snake_actions) {
            snake_action_handle_keystate(keyboard_state,
                                         app_game_server->prev_snake_actions_key_states + 0,
                                         app_game_server->snake_actions + 0);
        }

        dev_mode_handle_keystate(&app_game_server->dev_mode,
                                 &app_game_server->game,
                                 cell_size,
                                 keyboard_state);
        break;
    }
    case GAME_STATE_WAITING:
        break;
    case GAME_STATE_GAME_OVER:
        if (keyboard_state[SDL_SCANCODE_RETURN]) {
            app_game_server->game.state = GAME_STATE_WAITING;
            return true;
        }
        break;
    }

    return false;
}

// returns whether or not a tick occurred.
void app_game_server_update(AppStateGameServer* app_game_server,
                            bool should_tick,
                            S64 time_since_update_us,
                            bool enable_chomping,
                            bool enable_constricting) {
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (app_game_server->snake_actions[i] != SNAKE_ACTION_NONE) {
            action_buffer_add(app_game_server->action_buffers + i, app_game_server->snake_actions[i]);
        }
    }

    if (app_game_server->game.state == GAME_STATE_WAITING) {
        app_game_server->game.wait_to_start_ms -= (S32)(time_since_update_us / 1000);
        if (app_game_server->game.wait_to_start_ms <= 0) {
            app_game_server->game.state = GAME_STATE_PLAYING;
        }
    }

    if (should_tick) {
        SnakeAction snake_actions[MAX_SNAKE_COUNT];
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            snake_actions[i] = action_buffer_remove(&app_game_server->action_buffers[i]);
            if (!enable_chomping) {
                snake_actions[i] &= ~SNAKE_ACTION_CHOMP;
            }
            if (!enable_constricting) {
                snake_actions[i] &= ~(SNAKE_ACTION_CONSTRICT_LEFT | SNAKE_ACTION_CONSTRICT_RIGHT);
            }
        }
        game_update(&app_game_server->game, snake_actions);
    }
}

void app_game_client_handle_keystate(AppStateGameClient* app_game_client, const U8* keyboard_state) {
    if (app_game_client->game.state == GAME_STATE_PLAYING) {
        snake_action_handle_keystate(keyboard_state,
                                     &app_game_client->prev_action_key_state,
                                     &app_game_client->snake_actions);
    }
}

void app_server_update(AppState* app_state,
                       AppStateLobby* lobby_state,
                       AppStateGameServer* server_game_state,
                       bool should_tick,
                       S64 time_since_last_frame_us) {
    if (*app_state == APP_STATE_LOBBY) {
        if (app_lobby_update(lobby_state)) {
            *app_state = APP_STATE_GAME;
            server_game_state->game.wait_to_start_ms = 3000;
            reset_game(&server_game_state->game,
                       lobby_state);
        }
    } else if (*app_state == APP_STATE_GAME) {
        app_game_server_update(server_game_state,
                               should_tick,
                               time_since_last_frame_us,
                               lobby_state->game_settings.enable_chomping,
                               lobby_state->game_settings.enable_constricting);
    }
}

void init_controller_for_player(SDL_GameController* game_controllers[MAX_GAME_CONTROLLERS],
                                S32 joystick_index,
                                AppStateLobby* lobby_state,
                                SessionType session_type) {
    for (S32 c = 0; c < MAX_GAME_CONTROLLERS; c++) {
        if (game_controllers[c] != NULL) {
            if (SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(game_controllers[c])) == joystick_index) {
                return;
            }
            continue;
        }

        game_controllers[c] = SDL_GameControllerOpen(joystick_index);

        if (game_controllers[c] == NULL) {
            fprintf(stderr, "Failed to open game controller %s\n", SDL_GetError());
        } else if (session_type == SESSION_TYPE_SINGLE_PLAYER || session_type == SESSION_TYPE_SERVER) {
            S32 next_available_lobby_player = -1;
            for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                if (lobby_state->players[p].state == LOBBY_PLAYER_STATE_NONE) {
                    next_available_lobby_player = p;
                    break;
                }
            }
            if (next_available_lobby_player >= 0) {
                lobby_state->players[next_available_lobby_player].state = LOBBY_PLAYER_STATE_NOT_READY;
                lobby_state->players[next_available_lobby_player].type = LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER;
                lobby_state->players[next_available_lobby_player].snake_color =
                    lobby_find_next_unique_snake_color(lobby_state->players[0].snake_color, lobby_state);
                lobby_state->players[next_available_lobby_player].input_index = c;
                strncpy(lobby_state->players[next_available_lobby_player].name,
                        "ServerCTPlayer",
                        MAX_LOBBY_PLAYER_NAME_LEN);
            } else {
                fprintf(stderr, "No more space in lobby for another controller\n");
                SDL_GameControllerClose(game_controllers[c]);
                game_controllers[c] = NULL;
            }
        } else if (session_type == SESSION_TYPE_CLIENT) {
            printf("using controller instead of keyboard\n");
            lobby_state->players[0].type = LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER;
            lobby_state->players[0].input_index = c;
        }
        break;
    }
}

void close_controller_for_player(SDL_GameController* game_controllers[MAX_GAME_CONTROLLERS],
                                 S32 joystick_index,
                                 AppStateLobby* lobby_state,
                                 SessionType session_type) {
    for (S32 c = 0; c < MAX_GAME_CONTROLLERS; c++) {
        if (game_controllers[c] == NULL) {
            continue;
        }

        if (SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(game_controllers[c])) != joystick_index) {
            continue;
        }

        // Clean up the lobby player entry.
        if (session_type == SESSION_TYPE_SINGLE_PLAYER || session_type == SESSION_TYPE_SERVER) {
            for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                if (lobby_state->players[p].type == LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER &&
                    lobby_state->players[p].input_index == c) {

                    if (p == 0) {
                        lobby_state->players[p].type = LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD;
                        lobby_state->players[p].input_index = -1;
                    } else {
                        lobby_remove_player(lobby_state, p);
                    }
                    break;
                }
            }
        }

        SDL_GameControllerClose(game_controllers[c]);
        game_controllers[c] = NULL;
    }
}

void controller_handle_input(AppState app_state,
                             SessionType session_type,
                             SDL_GameController* game_controllers[MAX_GAME_CONTROLLERS],
                             AppStateLobby* lobby_state,
                             AppStateGameServer* server_game_state,
                             AppStateGameClient* client_game_state) {
    for (S32 c = 0; c < MAX_GAME_CONTROLLERS; c++) {
        if (game_controllers[c] == NULL) {
            continue;
        }

        S32 player_index = -1;
        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
            if (lobby_state->players[p].type == LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER &&
                lobby_state->players[p].input_index == c) {
                player_index = p;
                break;
            }
        }

        if (player_index < 0) {
            continue;
        }

        if (app_state == APP_STATE_LOBBY) {
            LobbyActionKeyState current_action_key_state = {0};
            current_action_key_state.toggle_ready =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_A);
            current_action_key_state.cycle_color =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_X);

            if (!lobby_state->prev_actions_key_states[player_index].toggle_ready &&
                current_action_key_state.toggle_ready) {
                lobby_state->actions[player_index] |= LOBBY_ACTION_TOGGLE_READY;
            }

            if (!lobby_state->prev_actions_key_states[player_index].cycle_color &&
                current_action_key_state.cycle_color) {
                lobby_state->actions[player_index] |= LOBBY_ACTION_CYCLE_COLOR;
            }

            lobby_state->prev_actions_key_states[player_index] = current_action_key_state;
        } else if (app_state == APP_STATE_GAME) {
            SnakeActionKeyState current_action_key_state = {0};
            current_action_key_state.face_north =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_DPAD_UP);
            current_action_key_state.face_west =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            current_action_key_state.face_south =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            current_action_key_state.face_east =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            current_action_key_state.chomp =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_A);
            current_action_key_state.constrict_left =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_X);
            current_action_key_state.constrict_right =
                SDL_GameControllerGetButton(game_controllers[c], SDL_CONTROLLER_BUTTON_B);

            SnakeAction* snake_actions = NULL;
            SnakeActionKeyState* prev_snake_actions_key_states = NULL;
            if (session_type == SESSION_TYPE_SINGLE_PLAYER || session_type == SESSION_TYPE_SERVER) {
                snake_actions = server_game_state->snake_actions + player_index;
                prev_snake_actions_key_states = server_game_state->prev_snake_actions_key_states + player_index;
            } else if (session_type == SESSION_TYPE_CLIENT) {
                snake_actions = &client_game_state->snake_actions;
                prev_snake_actions_key_states = &client_game_state->prev_action_key_state;
            } else {
                break;
            }

            if (!prev_snake_actions_key_states->face_north && current_action_key_state.face_north) {
                *snake_actions |= SNAKE_ACTION_FACE_NORTH;
            }

            if (!prev_snake_actions_key_states->face_west && current_action_key_state.face_west) {
                *snake_actions |= SNAKE_ACTION_FACE_WEST;
            }

            if (!prev_snake_actions_key_states->face_south && current_action_key_state.face_south) {
                *snake_actions |= SNAKE_ACTION_FACE_SOUTH;
            }

            if (!prev_snake_actions_key_states->face_east && current_action_key_state.face_east) {
                *snake_actions |= SNAKE_ACTION_FACE_EAST;
            }

            if (!prev_snake_actions_key_states->chomp && current_action_key_state.chomp) {
                *snake_actions |= SNAKE_ACTION_CHOMP;
            }

            // Constricting acts different, where you can hold it down.
            if (current_action_key_state.constrict_left) {
                *snake_actions |= SNAKE_ACTION_CONSTRICT_LEFT;
            }

            if (current_action_key_state.constrict_right) {
                *snake_actions |= SNAKE_ACTION_CONSTRICT_RIGHT;
            }

            *prev_snake_actions_key_states = current_action_key_state;
        }
    }
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

void handle_client_disconnect(NetSocket** server_client_sockets,
                              AppStateLobby* lobby_state,
                              S32 socket_index) {
    S32 lobby_player_index = lobby_find_network_player(lobby_state, socket_index);
    if (lobby_player_index >= 0) {
        lobby_remove_player(lobby_state, lobby_player_index);
    }

    net_destroy_socket(server_client_sockets[socket_index]);
    server_client_sockets[socket_index] = NULL;
}

int main(S32 argc, char** argv) {
    const char* port = NULL;
    const char* ip = NULL;
    const char* window_title = NULL;
    const char* player_name = NULL;

    SessionType session_type = SESSION_TYPE_SINGLE_PLAYER;
    for (S32 i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            session_type = SESSION_TYPE_SERVER;

            if (argc <= (i + 1)) {
                puts("Expected port argument for server mode");
                return EXIT_FAILURE;
            }

            port = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            session_type = SESSION_TYPE_CLIENT;

            if (argc <= (i + 2)) {
                puts("Expected ip and port arguments for client mode");
                return EXIT_FAILURE;
            }

            ip = argv[i + 1];
            port = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (argc <= (i + 1)) {
                puts("Expected player name argument");
                return EXIT_FAILURE;
            }

            player_name = argv[i + 1];
            i++;
        } else {
            puts("Unexpected argument passed");
            return EXIT_FAILURE;
        }
    }

    //
    // Init game and level
    //
    AppState app_state = APP_STATE_LOBBY;
    AppStateGameClient client_game_state = {0};
    AppStateGameServer server_game_state = {0};
    AppStateLobby lobby_state = {0};

    lobby_state.game_settings.enable_chomping = true;
    lobby_state.game_settings.enable_constricting = true;
    lobby_state.game_settings.head_invincible = true;
    lobby_state.game_settings.zero_tacos_respawn = false;
    lobby_state.game_settings.segment_health = 3;
    lobby_state.game_settings.starting_length = 5;
    lobby_state.game_settings.taco_count = 5;
    lobby_state.game_settings.tick_ms = 175;

    NetSocket* server_socket = NULL; // Used by server to listen for client connections.
    NetSocket* client_socket = NULL; // Used by client to send and receive.

    NetSocket* server_client_sockets[MAX_SERVER_CLIENT_COUNT] = { 0 };
    U16 server_sequence = 0;
    U16 client_sequence = 0;

    // TODO: Only do this when doing networking
    const char* net_log_file_name = session_type == SESSION_TYPE_SERVER ?
        "net_server.log" : "net_client.log";
    if (!net_init(net_log_file_name)) {
        fprintf(stderr, "%s\n", net_get_error());
        return EXIT_FAILURE;
    }

    Game* game = NULL;

    switch(session_type) {
    case SESSION_TYPE_SINGLE_PLAYER:
        puts("Starting single player session.\n");

        window_title = "Taco Quest";

        game = &server_game_state.game;
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
        game = &client_game_state.game;

        if (player_name) {
            Packet packet = {
                .header = {
                    .type = PACKET_TYPE_CLIENT_NAME,
                    .payload_size = (U16)(strnlen(player_name, MAX_LOBBY_PLAYER_NAME_LEN)),
                    .sequence = client_sequence++
                },
                .payload = (U8*)player_name
            };

            printf("sending player name: %s\n", player_name);
            if (!packet_send(client_socket, &packet)) {
                fprintf(stderr, "failed to send entire header for snake action\n");
            }
        }
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

        game = &server_game_state.game;
        break;
    }
    }

    // Create the server player in the lobby.
    if (session_type == SESSION_TYPE_SINGLE_PLAYER || session_type == SESSION_TYPE_SERVER) {
        lobby_state.players[0].state = LOBBY_PLAYER_STATE_NOT_READY;
        lobby_state.players[0].type = LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD;
        lobby_state.players[0].snake_color = SNAKE_COLOR_RED;
        lobby_state.players[0].input_index = -1;
        if (player_name) {
            strncpy(lobby_state.players[0].name, player_name, MAX_LOBBY_PLAYER_NAME_LEN);
        } else {
            strncpy(lobby_state.players[0].name, "ServerKBPlayer", MAX_LOBBY_PLAYER_NAME_LEN);
        }
    }

    game_init(game, LEVEL_WIDTH, LEVEL_HEIGHT, 6);
    reset_game(game, &lobby_state);

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
    if ( font == NULL ) {
        fprintf(stderr, "PF_LoadFont failed: %s", PF_GetError());
        return EXIT_FAILURE;
    }

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

    SDL_GameController* game_controllers[MAX_GAME_CONTROLLERS];
    memset(game_controllers, 0, sizeof(game_controllers[0]) * MAX_GAME_CONTROLLERS);

    UserInterface ui = {0};
    ui_create(&ui, font);

    UIMouseState ui_mouse_state = {0};

    UICheckBox ui_enable_chomping_checkbox = {15, 35};
    UICheckBox ui_enable_constricting_checkbox = {15, 60};
    UICheckBox ui_head_invincible_checkbox = {15, 85};
    UICheckBox ui_zero_tacos_respawn_checkbox = {15, 110};
    UISlider ui_segment_health_slider = {
        .x = 260,
        .y = 60,
        .pixel_width = 150,
        .min = 1,
        .max = 10
    };

    UISlider ui_snake_length_slider = {
        .x = 500,
        .y = 60,
        .pixel_width = 150,
        .min = 1,
        .max = 100
    };

    UISlider ui_taco_count_slider = {
        .x = 740,
        .y = 60,
        .pixel_width = 150,
        .min = 1,
        .max = 100
    };

    UISlider ui_tick_ms_slider = {
        .x = 500,
        .y = 110,
        .pixel_width = 150,
        .min = 10,
        .max = 500
    };

    S32 cell_size = min_display_dimension / max_level_dimension;

    int64_t time_since_tick_us = 0;

    size_t net_msg_buffer_size = 1024 * 1024;
    char* net_msg_buffer = (char*)malloc(net_msg_buffer_size);

    Packet client_receive_packet = {0};
    Packet server_receive_packets[MAX_SERVER_CLIENT_COUNT] = {0};
    PacketTransmissionState recv_game_state_state = {0};
    PacketTransmissionState recv_snake_action_states[MAX_SERVER_CLIENT_COUNT] = {0};

    bool quit = false;

    while (!quit) {
        // Calculate how much time has elapsed (in microseconds).
        struct timespec current_frame_timestamp = {0};
        timespec_get(&current_frame_timestamp, TIME_UTC);
        int64_t time_since_last_frame_us = microseconds_between_timestamps(&last_frame_timestamp, &current_frame_timestamp);
        time_since_tick_us += time_since_last_frame_us;
        last_frame_timestamp = current_frame_timestamp;

        ui_mouse_state.prev_left_clicked = ui_mouse_state.left_clicked;
        ui_mouse_state.prev_right_clicked = ui_mouse_state.right_clicked;

        // Clear actions
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            lobby_state.actions[i] = 0;
        }
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            server_game_state.snake_actions[i] = 0;
        }
        client_game_state.snake_actions = 0;

        // Handle events, such as input or window changes.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (app_state == APP_STATE_LOBBY) {
                    init_controller_for_player(game_controllers,
                                               event.cdevice.which,
                                               &lobby_state,
                                               session_type);
                }
                // TODO: support connecting a controller during game ?
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (app_state == APP_STATE_LOBBY) {
                    close_controller_for_player(game_controllers,
                                               event.cdevice.which,
                                               &lobby_state,
                                               session_type);
                }
                break;
            case SDL_MOUSEMOTION:
                ui_mouse_state.x = event.button.x;
                ui_mouse_state.y = event.button.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button) {
                case SDL_BUTTON_LEFT:
                    ui_mouse_state.left_clicked = true;
                    break;
                case SDL_BUTTON_RIGHT:
                    ui_mouse_state.right_clicked = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                switch (event.button.button) {
                case SDL_BUTTON_LEFT:
                    ui_mouse_state.left_clicked = false;
                    break;
                case SDL_BUTTON_RIGHT:
                    ui_mouse_state.right_clicked = false;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }

        // Handle snake action keys separately.
        {
            const U8* keyboard_state = SDL_GetKeyboardState(NULL);

            if (app_state == APP_STATE_LOBBY) {
                if (lobby_state.players[0].type == LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD) {
                    app_lobby_handle_keystate(&lobby_state, keyboard_state);
                }
            } else if (app_state == APP_STATE_GAME) {
                switch(session_type) {
                case SESSION_TYPE_CLIENT:
                    if (lobby_state.players[0].type == LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD) {
                        app_game_client_handle_keystate(&client_game_state, keyboard_state);
                    }
                    break;
                case SESSION_TYPE_SERVER:
                case SESSION_TYPE_SINGLE_PLAYER: {
                    if (app_game_server_handle_keystate(&server_game_state,
                                                        keyboard_state,
                                                        cell_size,
                                                        lobby_state.players[0].type == LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD)) {
                        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                            if (lobby_state.players[p].state == LOBBY_PLAYER_STATE_READY) {
                                lobby_state.players[p].state = LOBBY_PLAYER_STATE_NOT_READY;
                            }
                        }
                        app_state = APP_STATE_LOBBY;
                    }
                    dev_mode_handle_mouse(&server_game_state.dev_mode,
                                          &server_game_state.game,
                                          &ui_mouse_state,
                                          cell_size);
                    break;
                }
                }
            }
        }

        controller_handle_input(app_state,
                                session_type,
                                game_controllers,
                                &lobby_state,
                                &server_game_state,
                                &client_game_state);

        // The server/single player mode should only update the game state if a tick has passed.
        bool should_tick = false;
        bool should_send_state = false;
        if (time_since_tick_us >= MS_TO_US(lobby_state.game_settings.tick_ms)) {
            time_since_tick_us -= MS_TO_US(lobby_state.game_settings.tick_ms);
            should_send_state = true;
            if (server_game_state.game.state == GAME_STATE_PLAYING &&
                dev_mode_should_step(&server_game_state.dev_mode)) {
                server_game_state.dev_mode.should_step = false;
                __tick++;
                should_tick = true;
            }
        }

        switch (session_type) {
        case SESSION_TYPE_CLIENT: {
            // TODO: Check if our socket is still alive, otherwise we have to press a key after
            // the server disconnects.

            // Send our lobby or snake action to the server if there is one.
            if (lobby_state.actions[0] != LOBBY_ACTION_NONE) {
                Packet packet = {
                    .header = {
                        .type = PACKET_TYPE_LOBBY_ACTION,
                        .payload_size = sizeof(lobby_state.actions[0]),
                        .sequence = client_sequence++
                    },
                    .payload = (U8*)lobby_state.actions
                };

                if (!packet_send(client_socket, &packet)) {
                    fprintf(stderr, "failed to send entire header for snake action\n");
                }

                lobby_state.actions[0] = LOBBY_ACTION_NONE;
            }

            if (client_game_state.snake_actions != SNAKE_ACTION_NONE) {
                Packet packet = {
                    .header = {
                        .type = PACKET_TYPE_SNAKE_ACTION,
                        .payload_size = sizeof(client_game_state.snake_actions),
                        .sequence = client_sequence++
                    },
                    .payload = (U8*)&client_game_state.snake_actions
                };

                if (!packet_send(client_socket, &packet)) {
                    fprintf(stderr, "failed to send entire header for snake action\n");
                }
            }

            packet_receive(client_socket,
                           &client_receive_packet,
                           &recv_game_state_state);

            if ( recv_game_state_state.stage == PACKET_PROGRESS_STAGE_COMPLETE ) {
                if (client_receive_packet.header.type == PACKET_TYPE_LOBBY_STATE) {
                    if (app_state == APP_STATE_GAME) {
                        app_state = APP_STATE_LOBBY;
                    }
                    lobby_state_deserialize(client_receive_packet.payload,
                                            client_receive_packet.header.payload_size,
                                            &lobby_state);

                } else if (client_receive_packet.header.type == PACKET_TYPE_LEVEL_STATE) {
                    if (app_state == APP_STATE_LOBBY) {
                        app_state = APP_STATE_GAME;
                    }
                    game_deserialize(client_receive_packet.payload,
                                     client_receive_packet.header.payload_size,
                                     game);
                }

                memset(&recv_game_state_state, 0, sizeof(recv_game_state_state));
                free(client_receive_packet.payload);
                memset(&client_receive_packet, 0, sizeof(client_receive_packet));
            } else if ( recv_game_state_state.stage == PACKET_PROGRESS_STAGE_ERROR ) {
                if ( client_receive_packet.payload != NULL ) {
                    free(client_receive_packet.payload);
                }
                memset(&client_receive_packet, 0, sizeof(client_receive_packet));
                fputs(net_get_error(), stderr);
                net_destroy_socket(client_socket);
                client_socket = NULL;
            }
            break;
        }
        case SESSION_TYPE_SERVER: {
            // Server recEive input from client, update, then send game state to client
            // TODO: receive multiple snake actions, handle the last one.
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                if (server_client_sockets[i] == NULL) {
                    continue;
                }

                packet_receive(server_client_sockets[i],
                               &server_receive_packets[i],
                               &recv_snake_action_states[i]);

                if (recv_snake_action_states[i].stage == PACKET_PROGRESS_STAGE_COMPLETE) {
                    if (server_receive_packets[i].header.type == PACKET_TYPE_LOBBY_ACTION &&
                        app_state == APP_STATE_LOBBY) {
                        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                            if (lobby_state.players[p].type == LOBBY_PLAYER_TYPE_NETWORK &&
                                lobby_state.players[p].input_index == i) {
                                lobby_state.actions[p] = *(LobbyAction*)server_receive_packets[i].payload;
                                break;
                            }
                        }
                    } else if (server_receive_packets[i].header.type == PACKET_TYPE_SNAKE_ACTION &&
                               app_state == APP_STATE_GAME) {
                        SnakeAction client_snake_action = *(SnakeAction*)server_receive_packets[i].payload;
                        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                            if (lobby_state.players[p].type == LOBBY_PLAYER_TYPE_NETWORK &&
                                lobby_state.players[p].input_index == i) {
                                action_buffer_add(server_game_state.action_buffers + p, client_snake_action);
                                break;
                            }
                        }
                    } else if (server_receive_packets[i].header.type == PACKET_TYPE_CLIENT_NAME) {
                        printf("received client name: %.*s\n",
                               server_receive_packets[i].header.payload_size,
                               server_receive_packets[i].payload);
                        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                            if (lobby_state.players[p].type == LOBBY_PLAYER_TYPE_NETWORK &&
                                lobby_state.players[p].input_index == i) {
                                size_t name_len = server_receive_packets[i].header.payload_size;
                                if (name_len >= MAX_LOBBY_PLAYER_NAME_LEN) {
                                    name_len = MAX_LOBBY_PLAYER_NAME_LEN - 1;
                                }
                                strncpy(lobby_state.players[p].name,
                                        (char*)(server_receive_packets[i].payload),
                                        name_len);
                                lobby_state.players[p].name[name_len] = 0;
                                break;
                            }
                        }
                    }

                    // Resent packet state
                    memset(&recv_snake_action_states[i], 0, sizeof(recv_snake_action_states[i]));
                    free(server_receive_packets[i].payload);
                    memset(&server_receive_packets[i], 0, sizeof(server_receive_packets[i]));
                } else if ( recv_snake_action_states[i].stage == PACKET_PROGRESS_STAGE_ERROR ) {
                    if ( server_receive_packets[i].payload != NULL ) {
                        free(server_receive_packets[i].payload);
                    }
                    memset(&server_receive_packets[i], 0, sizeof(server_receive_packets[i]));
                    memset(&recv_snake_action_states[i], 0, sizeof(recv_snake_action_states[i]));
                    fputs(net_get_error(), stderr);
                    handle_client_disconnect(server_client_sockets, &lobby_state, i);
                }
            }

            // TODO: Consolidate with SINGLE code path
            app_server_update(&app_state,
                              &lobby_state,
                              &server_game_state,
                              should_tick,
                              time_since_last_frame_us);
            if (!should_send_state) {
                break;
            }

            // Listen for client connections
            for (S32 i = 0; i < MAX_SERVER_CLIENT_COUNT; i++) {
                if (server_client_sockets[i] == NULL) {
                    bool result = net_accept(server_socket, &server_client_sockets[i]);
                    if (result) {
                        if (server_client_sockets[i] != NULL) {
                            for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                                if (lobby_state.players[p].state == LOBBY_PLAYER_STATE_NONE) {
                                    lobby_state.players[p].state = LOBBY_PLAYER_STATE_NOT_READY;
                                    lobby_state.players[p].type = LOBBY_PLAYER_TYPE_NETWORK;
                                    lobby_state.players[p].input_index = i;
                                    lobby_state.players[p].snake_color =
                                        lobby_find_next_unique_snake_color(lobby_state.players[0].snake_color, &lobby_state);

                                    snprintf(lobby_state.players[p].name, MAX_LOBBY_PLAYER_NAME_LEN, "NetPlayer_%d", p);
                                    printf("assigning connected client %d to player %d\n", i, p);
                                    break;
                                }
                            }
                        }
                    } else {
                        fprintf(stderr, "%s\n", net_get_error());
                    }
                } else if (app_state == APP_STATE_LOBBY) {
                    // Serialize game state
                    size_t msg_size = lobby_state_serialize(&lobby_state,
                                                            net_msg_buffer,
                                                            net_msg_buffer_size);

                    // Send packet header
                    Packet packet = {
                        .header = {
                            .type = PACKET_TYPE_LOBBY_STATE,
                            .payload_size = (U16)(msg_size),
                            .sequence = server_sequence++
                        },
                        .payload = (U8*)net_msg_buffer
                    };

                    if (!packet_send(server_client_sockets[i], &packet)) {
                        printf("failed to send lobby state for tick: %d\n", __tick);
                        handle_client_disconnect(server_client_sockets, &lobby_state, i);
                    }
                } else if (app_state == APP_STATE_GAME) {
                    // Serialize game state
                    size_t msg_size = game_serialize(game,
                                                     net_msg_buffer,
                                                     net_msg_buffer_size);

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
                        handle_client_disconnect(server_client_sockets, &lobby_state, i);
                    }
                }
            }

            break;
        }
        case SESSION_TYPE_SINGLE_PLAYER: {
            app_server_update(&app_state,
                              &lobby_state,
                              &server_game_state,
                              should_tick,
                              time_since_last_frame_us);
            break;
        }
        }

        //
        // Render game
        //

        // Clear window
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        float font_scale = (float)(cell_size / 16); // 16 is the sprite sheet tile size.

        // Draw level
        if (app_state == APP_STATE_LOBBY) {
            PF_SetForeground(font, 255, 255, 255, 255);
            PF_SetScale(font, font_scale);

            PF_FontState font_state = PF_GetState(font);

            PF_RenderString(font, 3, 6, "Settings");
            PF_RenderString(font, 42, 38, "Chomping");
            PF_RenderString(font, 42, 64, "Constricting");
            PF_RenderString(font, 42, 90, "Head Invincible");
            PF_RenderString(font, 42, 116, "Zero Tacos Respawn");
            PF_RenderString(font, 240, 38, "Segment HP: %d", lobby_state.game_settings.segment_health);
            PF_RenderString(font, 480, 38, "Start Len: %d", lobby_state.game_settings.starting_length);
            PF_RenderString(font, 720, 38, "Tacos: %d", lobby_state.game_settings.taco_count);
            PF_RenderString(font, 480, 90, "Tick MS: %d", lobby_state.game_settings.tick_ms);

            {
                UIMouseState* mouse_state = &ui_mouse_state;
                UIMouseState empty = {0};

                // The client cannot modify the lobby ui, its readonly, so just always pass in an
                // empty state.
                if (session_type == SESSION_TYPE_CLIENT) {
                    mouse_state = &empty;
                }

                ui_checkbox(&ui,
                            renderer,
                            mouse_state,
                            &ui_enable_chomping_checkbox,
                            &lobby_state.game_settings.enable_chomping);
                ui_checkbox(&ui,
                            renderer,
                            mouse_state,
                            &ui_enable_constricting_checkbox,
                            &lobby_state.game_settings.enable_constricting);
                ui_checkbox(&ui,
                            renderer,
                            mouse_state,
                            &ui_head_invincible_checkbox,
                            &lobby_state.game_settings.head_invincible);
                ui_checkbox(&ui,
                            renderer,
                            mouse_state,
                            &ui_zero_tacos_respawn_checkbox,
                            &lobby_state.game_settings.zero_tacos_respawn);
                ui_slider(&ui,
                          renderer,
                          mouse_state,
                          &ui_segment_health_slider,
                          &lobby_state.game_settings.segment_health);
                ui_slider(&ui,
                          renderer,
                          mouse_state,
                          &ui_snake_length_slider,
                          &lobby_state.game_settings.starting_length);
                ui_slider(&ui,
                          renderer,
                          mouse_state,
                          &ui_taco_count_slider,
                          &lobby_state.game_settings.taco_count);
                ui_slider(&ui,
                          renderer,
                          mouse_state,
                          &ui_tick_ms_slider,
                          &lobby_state.game_settings.tick_ms);

                server_game_state.game.head_invincible = lobby_state.game_settings.head_invincible;
                server_game_state.game.zero_taco_respawn = lobby_state.game_settings.zero_tacos_respawn;
            }

            S32 lobby_cell_size = 40;
            S32 players_start_y = 4 * lobby_cell_size - 12;
            S32 players_offset = 30;

            PF_RenderString(font, 3, players_start_y, "Players");
            for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
                if (lobby_state.players[i].state != LOBBY_PLAYER_STATE_NONE) {
                    PF_SetForeground(font, 255, 255, 255, 255);
                    S32 name_len = (S32)(strnlen(lobby_state.players[i].name, MAX_LOBBY_PLAYER_NAME_LEN));
                    S32 name_pixel_width = (S32)(((name_len * font_state.char_width) + ((name_len - 1) * font_state.letter_spacing)) * font_state.scale);
                    PF_RenderString(font,
                                    130 - (name_pixel_width / 2),
                                    players_start_y + players_offset + lobby_cell_size * 2 * i,
                                    "%s",
                                    lobby_state.players[i].name);

                    if (lobby_state.players[i].state == LOBBY_PLAYER_STATE_READY) {
                        PF_SetForeground(font, 0, 255, 0, 255);
                        PF_RenderString(font,
                                        260,
                                        players_start_y + players_offset + (lobby_cell_size * 2 * i) + lobby_cell_size - 5,
                                        "Ready");
                    } else {
                        PF_SetForeground(font, 255, 0, 0, 255);
                        PF_RenderString(font,
                                        260,
                                        players_start_y + players_offset + (lobby_cell_size * 2 * i) + lobby_cell_size - 5,
                                        "Not Ready");
                    }

                    Snake snake = {0};
                    snake_init(&snake, 5);
                    snake.length = 4;
                    snake.direction = DIRECTION_EAST;
                    snake.color = lobby_state.players[i].snake_color;
                    snake.segments[0].x = 4;
                    snake.segments[0].y = (S16)(5 + (i * 2));
                    snake.segments[0].health = 3;
                    snake.segments[1].x = 3;
                    snake.segments[1].y = (S16)(5 + (i * 2));
                    snake.segments[1].health = 3;
                    snake.segments[2].x = 2;
                    snake.segments[2].y = (S16)(5 + (i * 2));
                    snake.segments[2].health = 3;
                    snake.segments[3].x = 1;
                    snake.segments[3].y = (S16)(5 + (i * 2));
                    snake.segments[3].health = 3;
                    snake_draw(renderer, snake_texture, &snake, lobby_cell_size, 3);
                    snake_destroy(&snake);
                }
            }


        } else if (app_state == APP_STATE_GAME) {
            if (!draw_game(game, renderer, snake_texture, cell_size, lobby_state.game_settings.segment_health)) {
                return EXIT_FAILURE;
            }

            SDL_SetTextureColorMod(snake_texture, 255, 255, 255);

            if (session_type == SESSION_TYPE_SERVER || session_type == SESSION_TYPE_SINGLE_PLAYER) {
                PF_SetScale(font, font_scale);
                dev_mode_draw(&server_game_state.dev_mode, game, font, window_width, cell_size);
            }
            
            PF_SetScale(font, font_scale * 2.0f);
            PF_SetForeground(font, 255, 255, 255, 255);

            switch(session_type) {
            case SESSION_TYPE_CLIENT:
                if (game->state == GAME_STATE_WAITING) {
                    PF_RenderString(font, 0, 0, "Game Starting in %d seconds", (S32)(ceil(client_game_state.game.wait_to_start_ms / 1000.0)));
                } else if (game->state == GAME_STATE_GAME_OVER) {
                    PF_RenderString(font, 0, 0, "Game Over!");
                }
                break;
            case SESSION_TYPE_SERVER:
            case SESSION_TYPE_SINGLE_PLAYER:
                if (game->state == GAME_STATE_WAITING) {
                    PF_RenderString(font, 0, 0, "Game Starting in %d seconds", (S32)(ceil(server_game_state.game.wait_to_start_ms / 1000.0)));
                } else if (game->state == GAME_STATE_GAME_OVER) {
                    PF_RenderString(font, 0, 0, "Game Over!");
                }
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

    for (S32 c = 0; c < MAX_GAME_CONTROLLERS; c++) {
        if (game_controllers[c] != NULL) {
            SDL_GameControllerClose(game_controllers[c]);
            game_controllers[c] = NULL;
        }
    }

    net_shutdown();
    free(net_msg_buffer);
    PF_DestroyFont(font);
    game_destroy(game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
