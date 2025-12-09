#ifndef lobby_h
#define lobby_h

#include "game.h"
#include "list_dir.h"

#define MAX_LOBBY_PLAYER_NAME_LEN 16

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
    char name[MAX_LOBBY_PLAYER_NAME_LEN];
    LobbyPlayerState state;
    LobbyPlayerType type;
    SnakeColor snake_color;
    // -1 is self. if type is controller, refers to controller index, if type is network refers to client index.
    S32 input_index;
} LobbyPlayer;

typedef struct {
    LobbyPlayer players[MAX_SNAKE_COUNT];
    LobbyActionKeyState prev_actions_key_states[MAX_SNAKE_COUNT];
    LobbyAction actions[MAX_SNAKE_COUNT];
    ListDir map_list;
    S32 selected_map;
} AppStateLobby;

bool app_lobby_update(AppStateLobby* lobby_state);
void app_lobby_handle_keystate(AppStateLobby* lobby_state, const bool* keyboard_state);

size_t lobby_state_serialize(AppStateLobby* lobby_state,
                             GameSettings* game_settings,
                             void* buffer,
                             size_t buffer_size);
size_t lobby_state_deserialize(void* buffer,
                               size_t buffer_size,
                               AppStateLobby* lobby_state,
                               GameSettings* game_settings);
S32 lobby_find_network_player(AppStateLobby* lobby_state, S32 socket_index);
void lobby_remove_player(AppStateLobby* lobby_state, S32 player_index);
SnakeColor lobby_find_next_unique_snake_color(SnakeColor starting_color, AppStateLobby* lobby_state);

#endif
