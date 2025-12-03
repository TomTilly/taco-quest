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
#include "ui.h"

#define MS_TO_US(ms) ((ms) * 1000)
#define GAME_SIMULATE_TIME_INTERVAL_US MS_TO_US(175) // default = 150
#define SERVER_ACCEPT_QUEUE_LIMIT 5
#define MAX_PLAYER_NAME_LEN 16
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

typedef enum {
    SNAKE_SELECTION_STATE_NONE,
    SNAKE_SELECTION_STATE_SELECTED,
    SNAKE_SELECTION_STATE_PLACING,
} DevSnakeSelectionState;

typedef struct {
    bool face_north;
    bool face_west;
    bool face_south;
    bool face_east;
    bool chomp;
    bool constrict_left;
    bool constrict_right;
} ActionKeyState;

typedef struct {
    bool toggle_enabled;
    bool toggle_step_mode;
    bool step_forward;
    bool place_taco;
} DevKeyState;

typedef struct {
    bool enabled;
    bool step_mode;
    bool should_step;
    DevSnakeSelectionState snake_selection_state;
    S32 snake_selection_index;
    DevKeyState prev_key_state;
} DevState;

typedef struct {
    bool toggle_ready;
    bool cycle_color;
} LobbyActionKeyState;

typedef enum {
    LOBBY_PLAYER_STATE_NONE,
    LOBBY_PLAYER_STATE_NOT_READY,
    LOBBY_PLAYER_STATE_READY,
} LobbyPlayerState;

typedef enum {
    LOBBY_PLAYER_TYPE_LOCAL_KEYBOARD,
    LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER,
    LOBBY_PLAYER_TYPE_NETWORK,
} LobbyPlayerType;

typedef enum {
    LOBBY_ACTION_NONE = 0,
    LOBBY_ACTION_TOGGLE_READY = 1,
    LOBBY_ACTION_CYCLE_COLOR = 2,
} LobbyAction;

typedef struct {
    char name[MAX_PLAYER_NAME_LEN];
    LobbyPlayerState state;
    LobbyPlayerType type;
    SnakeColor snake_color;
    // -1 is self. if type is controller, refers to controller index, if type is network refers to client index.
    S32 input_index;
} LobbyPlayer;

typedef struct {
    Game game;
    ActionKeyState prev_action_key_states[MAX_SNAKE_COUNT];
    SnakeAction tick_actions[MAX_SNAKE_COUNT];
    ActionBuffer action_buffers[MAX_SNAKE_COUNT];
    DevState dev_state;
} AppStateGameServer;

typedef struct {
    Game game;
    ActionKeyState prev_action_key_state;
    SnakeAction tick_actions;
} AppStateGameClient;

typedef struct {
    LobbyPlayer players[MAX_SNAKE_COUNT];
    LobbyActionKeyState prev_action_key_states[MAX_SNAKE_COUNT];
    LobbyAction actions[MAX_SNAKE_COUNT];
} AppStateLobby;

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
                AppStateLobby* lobby_state,
                S32 starting_snake_length,
                S32 segement_health,
                S32 taco_count) {
    game->max_taco_count = taco_count;
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
                starting_snake_length,
                (S8)(segement_health));
    game->snakes[0].color = lobby_state->players[0].snake_color;

    snake_spawn(game->snakes + 1,
                (S16)(level->width - 3),
                2,
                DIRECTION_SOUTH,
                starting_snake_length,
                (S8)(segement_health));
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
                    starting_snake_length,
                    (S8)(segement_health));
        game->snakes[2].color = lobby_state->players[2].snake_color;
    }

    if (snake_count > 3) {
        snake_spawn(game->snakes + 3,
                    3,
                    (S16)(level->height - 3),
                    DIRECTION_EAST,
                    starting_snake_length,
                    (S8)(segement_health));
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

SnakeColor find_next_unique_snake_color(SnakeColor starting_color, AppStateLobby* lobby_state) {
    SnakeColor result = (starting_color + 1) % SNAKE_COLOR_COUNT;
    while(result != starting_color) {
        bool matches_another = false;
        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
            if (lobby_state->players[p].snake_color == result) {
                matches_another = true;
            }
        }
        if (!matches_another) {
            break;
        }
        result += 1;
        result %= SNAKE_COLOR_COUNT;
    }
    return result;
}

S32 query_for_snake_at(Game* game, S32 cell_x, S32 cell_y) {
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        Snake *snake = game->snakes + i;
        for (S32 j = 0; j < snake->length; j++) {
            SnakeSegment *segment = snake->segments + j;
            if (segment->x == cell_x && segment->y == cell_y) {
                return i;
            }
        }
    }
    return -1;
}

bool dev_mode_should_step(const DevState * dev_state) {
    if (!dev_state->enabled) return true;
    if (!dev_state->step_mode) return true;
    return dev_state->should_step;
}

void add_snake_action_from_keystate(const U8* keyboard_state, ActionKeyState* prev_action_key_state, SnakeAction* tick_actions) {
    ActionKeyState current_action_key_state = {0};
    current_action_key_state.face_north = keyboard_state[SDL_SCANCODE_W];
    current_action_key_state.face_west = keyboard_state[SDL_SCANCODE_A];
    current_action_key_state.face_south = keyboard_state[SDL_SCANCODE_S];
    current_action_key_state.face_east = keyboard_state[SDL_SCANCODE_D];
    current_action_key_state.chomp = keyboard_state[SDL_SCANCODE_SPACE];
    current_action_key_state.constrict_left = keyboard_state[SDL_SCANCODE_Q];
    current_action_key_state.constrict_right = keyboard_state[SDL_SCANCODE_E];

    // Actions are triggered on the press event.
    if (!prev_action_key_state->face_north && current_action_key_state.face_north) {
        *tick_actions |= SNAKE_ACTION_FACE_NORTH;
    }

    if (!prev_action_key_state->face_west && current_action_key_state.face_west) {
        *tick_actions |= SNAKE_ACTION_FACE_WEST;
    }

    if (!prev_action_key_state->face_south && current_action_key_state.face_south) {
        *tick_actions |= SNAKE_ACTION_FACE_SOUTH;
    }

    if (!prev_action_key_state->face_east && current_action_key_state.face_east) {
        *tick_actions |= SNAKE_ACTION_FACE_EAST;
    }

    if (!prev_action_key_state->chomp && current_action_key_state.chomp) {
        *tick_actions |= SNAKE_ACTION_CHOMP;
    }

    // Constricting acts different, where you can hold it down.
    if (current_action_key_state.constrict_left) {
        *tick_actions |= SNAKE_ACTION_CONSTRICT_LEFT;
    }

    if (current_action_key_state.constrict_right) {
        *tick_actions |= SNAKE_ACTION_CONSTRICT_RIGHT;
    }

    *prev_action_key_state = current_action_key_state;
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

void draw_dev_state(DevState* dev_state, Game* game, PF_Font* font, S32 window_width, S32 cell_size) {
    if (!dev_state->enabled) {
        return;
    }

    PF_SetForeground(font, 255, 255, 0, 255);
    if (dev_state->step_mode ) {
        PF_RenderString(font, 2, 2, "Dev Step Mode!");
    } else {
        PF_RenderString(font, 2, 2, "Dev Mode!");
    }

    if (dev_state->snake_selection_state != SNAKE_SELECTION_STATE_NONE) {
        PF_RenderString(font, window_width / 3, 2, "Selected Snake %d", dev_state->snake_selection_index);
    }

    PF_SetForeground(font, 0, 0, 0, 255);
    PF_FontState font_state = PF_GetState(font);
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        Snake *snake = game->snakes + i;
        for (S32 j = 1; j < snake->length; j++) {
            SnakeSegment *segment = snake->segments + j;
            PF_RenderChar(font,
                          (S16)((segment->x * cell_size) + (cell_size - (font_state.char_width * font_state.scale)) / 2),
                          (S16)((segment->y * cell_size) + (cell_size - (font_state.char_height * font_state.scale)) / 2),
                          (char)('1' + j));
        }
    }
}

bool app_game_server_handle_keystate(AppStateGameServer* app_game_server,
                                     const U8* keyboard_state,
                                     S32 cell_size,
                                     bool use_keyboard_for_snake_actions) {
    switch (app_game_server->game.state) {
    case GAME_STATE_PLAYING: {
        if (use_keyboard_for_snake_actions) {
            add_snake_action_from_keystate(keyboard_state,
                                           app_game_server->prev_action_key_states + 0,
                                           app_game_server->tick_actions + 0);
        }

        DevKeyState current_dev_key_state = {0};
        current_dev_key_state.toggle_enabled = keyboard_state[SDL_SCANCODE_GRAVE];
        current_dev_key_state.toggle_step_mode = keyboard_state[SDL_SCANCODE_TAB];
        current_dev_key_state.step_forward = keyboard_state[SDL_SCANCODE_RETURN];
        current_dev_key_state.place_taco = keyboard_state[SDL_SCANCODE_T];

        if (!app_game_server->dev_state.prev_key_state.toggle_enabled &&
            current_dev_key_state.toggle_enabled) {
            app_game_server->dev_state.enabled = !app_game_server->dev_state.enabled;
            if (!app_game_server->dev_state.enabled) {
                app_game_server->dev_state = (DevState){0};
            }
        }

        if (app_game_server->dev_state.enabled) {
            if (!app_game_server->dev_state.prev_key_state.toggle_step_mode &&
                current_dev_key_state.toggle_step_mode) {
                app_game_server->dev_state.step_mode = !app_game_server->dev_state.step_mode;
            }

            if (!app_game_server->dev_state.prev_key_state.step_forward &&
                current_dev_key_state.step_forward) {
                if (app_game_server->dev_state.step_mode) {
                    app_game_server->dev_state.should_step = true;
                }
            }

            if (!app_game_server->dev_state.prev_key_state.place_taco &&
                current_dev_key_state.place_taco) {
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                S32 cell_x = mouse_x / cell_size;
                S32 cell_y = mouse_y / cell_size;
                if (game_empty_at(&app_game_server->game, cell_x, cell_y)) {
                    level_set_cell(&app_game_server->game.level, cell_x, cell_y, CELL_TYPE_TACO);
                }
            }
        }
        app_game_server->dev_state.prev_key_state = current_dev_key_state;
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

void app_game_server_handle_mouse(AppStateGameServer* app_game_server, int mouse_x, int mouse_y, U32 mouse_buttons_state, S32 cell_size) {
    if (!app_game_server->dev_state.enabled) {
        return;
    }

    if (mouse_buttons_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        S32 cell_x = mouse_x / cell_size;
        S32 cell_y = mouse_y / cell_size;

        switch (app_game_server->dev_state.snake_selection_state) {
        case SNAKE_SELECTION_STATE_NONE: {
           S32 selected_snake_index = query_for_snake_at(&app_game_server->game, cell_x, cell_y);
           if (selected_snake_index >= 0) {
               app_game_server->dev_state.snake_selection_index = selected_snake_index;
               app_game_server->dev_state.snake_selection_state = SNAKE_SELECTION_STATE_SELECTED;
           }
           break;
        }
        case SNAKE_SELECTION_STATE_SELECTED: {
            S32 selected_snake_index = query_for_snake_at(&app_game_server->game, cell_x, cell_y);
            if (selected_snake_index < 0) {
                Snake* snake = app_game_server->game.snakes + app_game_server->dev_state.snake_selection_index;
                snake->length = 1;
                snake->segments[0].x = (S16)(cell_x);
                snake->segments[0].y = (S16)(cell_y);
                app_game_server->dev_state.snake_selection_state = SNAKE_SELECTION_STATE_PLACING;
            } else if (selected_snake_index != app_game_server->dev_state.snake_selection_index) {
                app_game_server->dev_state.snake_selection_index = selected_snake_index;
            }
            break;
        }
        case SNAKE_SELECTION_STATE_PLACING: {
            Snake* snake = app_game_server->game.snakes + app_game_server->dev_state.snake_selection_index;
            S32 previous_index = snake->length - 1;
            Direction adjacent_dir = direction_between_cells(cell_x,
                                                             cell_y,
                                                             snake->segments[previous_index].x,
                                                             snake->segments[previous_index].y);
            if (adjacent_dir != DIRECTION_NONE) {
                S32 previous_previous_index = snake->length - 2;
                if (previous_previous_index >= 0 &&
                    snake->segments[previous_previous_index].x == cell_x &&
                    snake->segments[previous_previous_index].y == cell_y) {
                   snake->length--;
                   break;
                }

                CellType cell_type = level_get_cell(&app_game_server->game.level, cell_x, cell_y);
                if (cell_type != CELL_TYPE_EMPTY) {
                   break;
                }
                if (query_for_snake_at(&app_game_server->game, cell_x, cell_y) >= 0) {
                   break;
                }

                // face the snake away from where we're dragging.
                if (snake->length == 1) {
                   snake->direction = opposite_direction(adjacent_dir);
                }

                S32 new_index = snake->length;

                snake->length++;
                snake->segments[new_index].x = (S16)(cell_x);
                snake->segments[new_index].y = (S16)(cell_y);
                snake->segments[new_index].health = snake->segments[0].health;
            }
            break;
        }
        default:
            break;
        }
    } else {
        if (app_game_server->dev_state.snake_selection_state == SNAKE_SELECTION_STATE_PLACING) {
            app_game_server->dev_state.snake_selection_state = SNAKE_SELECTION_STATE_NONE;
        }
    }
}

// returns whether or not a tick occurred.
void app_game_server_update(AppStateGameServer* app_game_server,
                            bool should_tick,
                            S64 time_since_update_us,
                            bool enable_chomping,
                            bool enable_constricting) {
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (app_game_server->tick_actions[i] != SNAKE_ACTION_NONE) {
            action_buffer_add(app_game_server->action_buffers + i, app_game_server->tick_actions[i]);
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
        add_snake_action_from_keystate(keyboard_state,
                                 &app_game_client->prev_action_key_state,
                                 &app_game_client->tick_actions);
    }
}

void app_lobby_handle_keystate(AppStateLobby* lobby_state, const U8* keyboard_state) {
    LobbyActionKeyState current_action_key_state = {0};
    current_action_key_state.toggle_ready = keyboard_state[SDL_SCANCODE_RETURN];
    current_action_key_state.cycle_color = keyboard_state[SDL_SCANCODE_SPACE];

    if (!lobby_state->prev_action_key_states[0].toggle_ready &&
        current_action_key_state.toggle_ready) {
        lobby_state->actions[0] |= LOBBY_ACTION_TOGGLE_READY;
    }
    if (!lobby_state->prev_action_key_states[0].cycle_color &&
        current_action_key_state.cycle_color) {
        lobby_state->actions[0] |= LOBBY_ACTION_CYCLE_COLOR;
    }

    lobby_state->prev_action_key_states[0] = current_action_key_state;
}

bool app_lobby_update(AppStateLobby* lobby_state) {
    bool all_ready = true;

    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (lobby_state->actions[i] & LOBBY_ACTION_TOGGLE_READY) {
            if (lobby_state->players[i].state == LOBBY_PLAYER_STATE_NOT_READY) {
                lobby_state->players[i].state = LOBBY_PLAYER_STATE_READY;
            } else if (lobby_state->players[i].state == LOBBY_PLAYER_STATE_READY) {
                lobby_state->players[i].state = LOBBY_PLAYER_STATE_NOT_READY;
            }
        }

        if (lobby_state->actions[i] & LOBBY_ACTION_CYCLE_COLOR) {
            lobby_state->players[i].snake_color = find_next_unique_snake_color(lobby_state->players[i].snake_color, lobby_state);
        }

        if (lobby_state->players[i].state == LOBBY_PLAYER_STATE_NOT_READY) {
            all_ready = false;
        }
    }

    return all_ready;
}

size_t lobby_state_serialize(AppStateLobby* lobby_state, void* buffer, size_t buffer_size) {
    size_t bytes_written = 0;
    assert(buffer_size >= (MAX_SNAKE_COUNT * (MAX_PLAYER_NAME_LEN + sizeof(lobby_state->players[0].state))));
    U8* buffer_ptr = buffer;
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        memcpy(buffer_ptr, lobby_state->players[i].name, MAX_PLAYER_NAME_LEN);
        bytes_written += MAX_PLAYER_NAME_LEN;
        buffer_ptr += MAX_PLAYER_NAME_LEN;

        memcpy(buffer_ptr, &lobby_state->players[i].state, sizeof(lobby_state->players[i].state));
        bytes_written += sizeof(lobby_state->players[i].state);
        buffer_ptr += sizeof(lobby_state->players[i].state);

        memcpy(buffer_ptr, &lobby_state->players[i].snake_color, sizeof(lobby_state->players[i].snake_color));
        bytes_written += sizeof(lobby_state->players[i].snake_color);
        buffer_ptr += sizeof(lobby_state->players[i].snake_color);
    }
    return bytes_written;
}

size_t lobby_state_deserialize(void* buffer, size_t buffer_size, AppStateLobby* lobby_state) {
    assert(buffer_size >= (MAX_SNAKE_COUNT * (MAX_PLAYER_NAME_LEN + sizeof(lobby_state->players[0].state))));
    U8* buffer_ptr = buffer;
    size_t bytes_read = 0;
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        memcpy(lobby_state->players[i].name, buffer_ptr, MAX_PLAYER_NAME_LEN);
        bytes_read += MAX_PLAYER_NAME_LEN;
        buffer_ptr += MAX_PLAYER_NAME_LEN;

        memcpy(&lobby_state->players[i].state, buffer_ptr, sizeof(lobby_state->players[i].state));
        bytes_read += sizeof(lobby_state->players[i].state);
        buffer_ptr += sizeof(lobby_state->players[i].state);

        memcpy(&lobby_state->players[i].snake_color, buffer_ptr, sizeof(lobby_state->players[i].snake_color));
        bytes_read += sizeof(lobby_state->players[i].snake_color);
        buffer_ptr += sizeof(lobby_state->players[i].snake_color);
    }
    return bytes_read;
}

void app_server_update(AppState* app_state,
                       AppStateLobby* lobby_state,
                       AppStateGameServer* server_game_state,
                       bool should_tick,
                       S64 time_since_last_frame_us,
                       S32 starting_snake_length,
                       S32 segment_health,
                       S32 max_taco_count,
                       bool enable_chomping,
                       bool enable_constricting) {
    if (*app_state == APP_STATE_LOBBY) {
        if (app_lobby_update(lobby_state)) {
            *app_state = APP_STATE_GAME;
            server_game_state->game.wait_to_start_ms = 3000;
            reset_game(&server_game_state->game,
                       lobby_state,
                       starting_snake_length,
                       segment_health,
                       max_taco_count);
        }
    } else if (*app_state == APP_STATE_GAME) {
        app_game_server_update(server_game_state,
                               should_tick,
                               time_since_last_frame_us,
                               enable_chomping,
                               enable_constricting);
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
                lobby_state->players[next_available_lobby_player].snake_color = SNAKE_COLOR_YELLOW;
                lobby_state->players[next_available_lobby_player].input_index = c;
                strncpy(lobby_state->players[1].name, "ServerCTPlayer", MAX_PLAYER_NAME_LEN);
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
                        lobby_state->players[p].state = LOBBY_PLAYER_STATE_NONE;
                    }
                    break;
                }
            }
        }

        SDL_GameControllerClose(game_controllers[c]);
        game_controllers[c] = NULL;
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
    AppState app_state = APP_STATE_LOBBY;
    AppStateGameClient client_game_state = {0};
    AppStateGameServer server_game_state = {0};
    AppStateLobby lobby_state = {0};

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
        strncpy(lobby_state.players[0].name, "ServerKBPlayer", MAX_PLAYER_NAME_LEN);
    }

    game_init(game, LEVEL_WIDTH, LEVEL_HEIGHT, 6);
    reset_game(game, &lobby_state, 5, 6, 3);

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

    UICheckBox ui_enable_chomping_checkbox = {15, 35, true};
    UICheckBox ui_enable_constricting_checkbox = {15, 60, true};
    UISlider ui_segment_health_slider = {
        .x = 260,
        .y = 60,
        .pixel_width = 150,
        .value = 3,
        .min = 1,
        .max = 10
    };

    UISlider ui_snake_length_slider = {
        .x = 500,
        .y = 60,
        .pixel_width = 150,
        .value = 5,
        .min = 1,
        .max = 100
    };

    UISlider ui_taco_count_slider = {
        .x = 740,
        .y = 60,
        .pixel_width = 150,
        .value = 5,
        .min = 1,
        .max = 100
    };

    S32 cell_size = min_display_dimension / max_level_dimension;

    int64_t time_since_tick_us = 0;

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
        int64_t time_since_last_frame_us = microseconds_between_timestamps(&last_frame_timestamp, &current_frame_timestamp);
        time_since_tick_us += time_since_last_frame_us;
        last_frame_timestamp = current_frame_timestamp;

        ui_mouse_state.prev_clicked = ui_mouse_state.clicked;

        // Clear actions
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            lobby_state.actions[i] = 0;
        }
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            server_game_state.tick_actions[i] = 0;
        }
        client_game_state.tick_actions = 0;

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
                    ui_mouse_state.clicked = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                switch (event.button.button) {
                case SDL_BUTTON_LEFT:
                    ui_mouse_state.clicked = false;
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
                    int mouse_x = 0;
                    int mouse_y = 0;
                    U32 mouse_buttons_state = SDL_GetMouseState(&mouse_x, &mouse_y);
                    app_game_server_handle_mouse(&server_game_state, mouse_x, mouse_y, mouse_buttons_state, cell_size);
                    break;
                }
                }
            }
        }

        // Handle game pad input if any
        for (S32 c = 0; c < MAX_GAME_CONTROLLERS; c++) {
            if (game_controllers[c] == NULL) {
                continue;
            }

            S32 player_index = -1;
            for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
                if (lobby_state.players[p].type == LOBBY_PLAYER_TYPE_LOCAL_CONTROLLER &&
                    lobby_state.players[p].input_index == c) {
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

                if (!lobby_state.prev_action_key_states[player_index].toggle_ready &&
                    current_action_key_state.toggle_ready) {
                    lobby_state.actions[player_index] |= LOBBY_ACTION_TOGGLE_READY;
                }

                if (!lobby_state.prev_action_key_states[player_index].cycle_color &&
                    current_action_key_state.cycle_color) {
                    lobby_state.actions[player_index] |= LOBBY_ACTION_CYCLE_COLOR;
                }

                lobby_state.prev_action_key_states[player_index] = current_action_key_state;
            } else if (app_state == APP_STATE_GAME) {
                ActionKeyState current_action_key_state = {0};
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

                SnakeAction* tick_actions = NULL;
                ActionKeyState* prev_action_key_states = NULL;
                if (session_type == SESSION_TYPE_SINGLE_PLAYER || session_type == SESSION_TYPE_SERVER) {
                    tick_actions = server_game_state.tick_actions + player_index;
                    prev_action_key_states = server_game_state.prev_action_key_states + player_index;
                } else if (session_type == SESSION_TYPE_CLIENT) {
                    tick_actions = &client_game_state.tick_actions;
                    prev_action_key_states = &client_game_state.prev_action_key_state;
                } else {
                    break;
                }

                if (!prev_action_key_states->face_north && current_action_key_state.face_north) {
                    *tick_actions |= SNAKE_ACTION_FACE_NORTH;
                }

                if (!prev_action_key_states->face_west && current_action_key_state.face_west) {
                    *tick_actions |= SNAKE_ACTION_FACE_WEST;
                }

                if (!prev_action_key_states->face_south && current_action_key_state.face_south) {
                    *tick_actions |= SNAKE_ACTION_FACE_SOUTH;
                }

                if (!prev_action_key_states->face_east && current_action_key_state.face_east) {
                    *tick_actions |= SNAKE_ACTION_FACE_EAST;
                }

                if (!prev_action_key_states->chomp && current_action_key_state.chomp) {
                    *tick_actions |= SNAKE_ACTION_CHOMP;
                }

                // Constricting acts different, where you can hold it down.
                if (current_action_key_state.constrict_left) {
                    *tick_actions |= SNAKE_ACTION_CONSTRICT_LEFT;
                }

                if (current_action_key_state.constrict_right) {
                    *tick_actions |= SNAKE_ACTION_CONSTRICT_RIGHT;
                }

                *prev_action_key_states = current_action_key_state;
            }
        }

        // The server/single player mode should only update the game state if a tick has passed.
        bool should_tick = false;
        bool should_send_state = false;
        if (time_since_tick_us >= GAME_SIMULATE_TIME_INTERVAL_US) {
            time_since_tick_us -= GAME_SIMULATE_TIME_INTERVAL_US;
            should_send_state = true;
            if (server_game_state.game.state == GAME_STATE_PLAYING &&
                dev_mode_should_step(&server_game_state.dev_state)) {
                server_game_state.dev_state.should_step = false;
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

            if (client_game_state.tick_actions != SNAKE_ACTION_NONE) {
                Packet packet = {
                    .header = {
                        .type = PACKET_TYPE_SNAKE_ACTION,
                        .payload_size = sizeof(client_game_state.tick_actions),
                        .sequence = client_sequence++
                    },
                    .payload = (U8*)&client_game_state.tick_actions
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
                if (server_client_sockets[i] != NULL) {
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
                        fputs(net_get_error(), stderr);
                        net_destroy_socket(server_client_sockets[i]);
                        server_client_sockets[i] = NULL;
                    }
                }
            }

            // TODO: Consolidate with SINGLE code path
            app_server_update(&app_state,
                              &lobby_state,
                              &server_game_state,
                              should_tick,
                              time_since_last_frame_us,
                              ui_snake_length_slider.value,
                              ui_segment_health_slider.value,
                              ui_taco_count_slider.value,
                              ui_enable_chomping_checkbox.value,
                              ui_enable_constricting_checkbox.value);
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
                                        find_next_unique_snake_color(lobby_state.players[0].snake_color, &lobby_state);

                                    snprintf(lobby_state.players[p].name, MAX_PLAYER_NAME_LEN, "ClientPlayer_%d", p);
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
                              time_since_last_frame_us,
                              ui_snake_length_slider.value,
                              ui_segment_health_slider.value,
                              ui_taco_count_slider.value,
                              ui_enable_chomping_checkbox.value,
                              ui_enable_constricting_checkbox.value);
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
            S32 font_height = (S32)(font_state.char_height * font_state.scale);

            PF_RenderString(font, 3, 6, "Settings");
            PF_RenderString(font, 42, 38, "Chomping");
            PF_RenderString(font, 42, 64, "Constricting");
            PF_RenderString(font, 240, 38, "Segment HP: %d", ui_segment_health_slider.value);
            PF_RenderString(font, 480, 38, "Start Len: %d", ui_snake_length_slider.value);
            PF_RenderString(font, 720, 38, "Tacos: %d", ui_taco_count_slider.value);
            ui_checkbox(&ui, &ui_mouse_state, &ui_enable_chomping_checkbox, renderer);
            ui_checkbox(&ui, &ui_mouse_state, &ui_enable_constricting_checkbox, renderer);
            ui_slider(&ui, &ui_mouse_state, &ui_segment_health_slider, renderer);
            ui_slider(&ui, &ui_mouse_state, &ui_snake_length_slider, renderer);
            ui_slider(&ui, &ui_mouse_state, &ui_taco_count_slider, renderer);

            S32 players_start_y = 6 * font_height;
            PF_RenderString(font, 3, players_start_y, "Players");
            for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
                if (lobby_state.players[i].state != LOBBY_PLAYER_STATE_NONE) {
                    PF_RenderString(font, (S32)(font_state.char_width * font_state.scale), players_start_y + font_height * (i + 2),
                                    "%d: %s Color: %s Ready: %s",
                                    i + 1,
                                    lobby_state.players[i].name,
                                    snake_color_string(lobby_state.players[i].snake_color),
                                    lobby_state.players[i].state == LOBBY_PLAYER_STATE_READY ? "Yes" : "No");
                }
            }

        } else if (app_state == APP_STATE_GAME) {
            if (!draw_game(game, renderer, snake_texture, cell_size, ui_segment_health_slider.value)) {
                return EXIT_FAILURE;
            }

            SDL_SetTextureColorMod(snake_texture, 255, 255, 255);

            switch(session_type) {
            case SESSION_TYPE_SERVER:
            case SESSION_TYPE_SINGLE_PLAYER: {
                PF_SetScale(font, font_scale);
                draw_dev_state(&server_game_state.dev_state, game, font, window_width, cell_size);
                break;
            }
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
