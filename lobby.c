#include "lobby.h"

#include <SDL2/SDL.h>

#include <assert.h>

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
            lobby_state->players[i].snake_color =
                lobby_find_next_unique_snake_color(lobby_state->players[i].snake_color,
                                                   lobby_state);
        }

        if (lobby_state->players[i].state == LOBBY_PLAYER_STATE_NOT_READY) {
            all_ready = false;
        }
    }

    return all_ready;
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

size_t lobby_state_serialize(AppStateLobby* lobby_state,
                             void* buffer,
                             size_t buffer_size) {
    size_t bytes_written = 0;
    assert(buffer_size >= (MAX_SNAKE_COUNT *
                           (MAX_LOBBY_PLAYER_NAME_LEN + sizeof(lobby_state->players[0].state)) +
                           sizeof(lobby_state->game_settings)));
    U8* buffer_ptr = buffer;
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        memcpy(buffer_ptr, lobby_state->players[i].name, MAX_LOBBY_PLAYER_NAME_LEN);
        bytes_written += MAX_LOBBY_PLAYER_NAME_LEN;
        buffer_ptr += MAX_LOBBY_PLAYER_NAME_LEN;

        memcpy(buffer_ptr, &lobby_state->players[i].state, sizeof(lobby_state->players[i].state));
        bytes_written += sizeof(lobby_state->players[i].state);
        buffer_ptr += sizeof(lobby_state->players[i].state);

        memcpy(buffer_ptr, &lobby_state->players[i].snake_color, sizeof(lobby_state->players[i].snake_color));
        bytes_written += sizeof(lobby_state->players[i].snake_color);
        buffer_ptr += sizeof(lobby_state->players[i].snake_color);
    }

    memcpy(buffer_ptr, &lobby_state->game_settings, sizeof(lobby_state->game_settings));
    bytes_written += sizeof(lobby_state->game_settings);
    buffer_ptr += sizeof(lobby_state->game_settings);

    return bytes_written;
}

size_t lobby_state_deserialize(void* buffer,
                               size_t buffer_size,
                               AppStateLobby* lobby_state) {
    assert(buffer_size >= (MAX_SNAKE_COUNT *
                           (MAX_LOBBY_PLAYER_NAME_LEN + sizeof(lobby_state->players[0].state)) +
                           sizeof(lobby_state->game_settings)));
    U8* buffer_ptr = buffer;
    size_t bytes_read = 0;
    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        memcpy(lobby_state->players[i].name, buffer_ptr, MAX_LOBBY_PLAYER_NAME_LEN);
        bytes_read += MAX_LOBBY_PLAYER_NAME_LEN;
        buffer_ptr += MAX_LOBBY_PLAYER_NAME_LEN;

        memcpy(&lobby_state->players[i].state, buffer_ptr, sizeof(lobby_state->players[i].state));
        bytes_read += sizeof(lobby_state->players[i].state);
        buffer_ptr += sizeof(lobby_state->players[i].state);

        memcpy(&lobby_state->players[i].snake_color, buffer_ptr, sizeof(lobby_state->players[i].snake_color));
        bytes_read += sizeof(lobby_state->players[i].snake_color);
        buffer_ptr += sizeof(lobby_state->players[i].snake_color);
    }

    memcpy(&lobby_state->game_settings, buffer_ptr, sizeof(lobby_state->game_settings));
    bytes_read += sizeof(lobby_state->game_settings);
    buffer_ptr += sizeof(lobby_state->game_settings);

    return bytes_read;
}

S32 lobby_find_network_player(AppStateLobby* lobby_state, S32 socket_index) {
    S32 result = -1;
    for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
        if (lobby_state->players[p].state != LOBBY_PLAYER_STATE_NONE &&
            lobby_state->players[p].type == LOBBY_PLAYER_TYPE_NETWORK &&
            lobby_state->players[p].input_index == socket_index) {
            result = p;
            break;
        }
    }
    return result;
}

void lobby_remove_player(AppStateLobby* lobby_state, S32 player_index) {
    assert(player_index >= 0 && player_index < MAX_SNAKE_COUNT);
    lobby_state->players[player_index].state = LOBBY_PLAYER_STATE_NONE;
    for (S32 p = player_index + 1; p < MAX_SNAKE_COUNT; p++) {
        if (lobby_state->players[p].state != LOBBY_PLAYER_STATE_NONE) {
            lobby_state->players[player_index] = lobby_state->players[p];
            lobby_state->players[p].state = LOBBY_PLAYER_STATE_NONE;
            break;
        }
    }
}

SnakeColor lobby_find_next_unique_snake_color(SnakeColor starting_color, AppStateLobby* lobby_state) {
    SnakeColor result = (starting_color + 1) % SNAKE_COLOR_COUNT;
    while(result != starting_color) {
        bool matches_another = false;
        for (S32 p = 0; p < MAX_SNAKE_COUNT; p++) {
            if (lobby_state->players[p].state != LOBBY_PLAYER_STATE_NONE &&
                lobby_state->players[p].snake_color == result) {
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
