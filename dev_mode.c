#include "dev_mode.h"

bool dev_mode_should_step(const DevMode *dev_mode) {
    if (!dev_mode->enabled) return true;
    if (!dev_mode->step_mode) return true;
    return dev_mode->should_step;
}

void dev_mode_draw(DevMode* dev_mode, Game* game, PF_Font* font, S32 window_width, S32 cell_size) {
    if (!dev_mode->enabled) {
        return;
    }

    PF_SetForeground(font, 255, 255, 0, 255);
    if (dev_mode->step_mode ) {
        PF_RenderString(font, 2, 2, "Dev Step Mode!");
    } else {
        PF_RenderString(font, 2, 2, "Dev Mode!");
    }

    if (dev_mode->snake_selection_state != SNAKE_SELECTION_STATE_NONE) {
        PF_RenderString(font, window_width / 3, 2, "Selected Snake %d", dev_mode->snake_selection_index);
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

void dev_mode_handle_keystate(DevMode* dev_mode, Game* game, S32 cell_size, const U8* keyboard_state) {
    DevModeKeyState current_dev_key_state = {0};
    current_dev_key_state.toggle_enabled = keyboard_state[SDL_SCANCODE_GRAVE];
    current_dev_key_state.toggle_step_mode = keyboard_state[SDL_SCANCODE_TAB];
    current_dev_key_state.step_forward = keyboard_state[SDL_SCANCODE_RETURN];
    current_dev_key_state.place_taco = keyboard_state[SDL_SCANCODE_T];

    if (!dev_mode->prev_key_state.toggle_enabled &&
        current_dev_key_state.toggle_enabled) {
        dev_mode->enabled = !dev_mode->enabled;
        if (!dev_mode->enabled) {
            *dev_mode = (DevMode){0};
        }
    }

    if (dev_mode->enabled) {
        if (!dev_mode->prev_key_state.toggle_step_mode &&
            current_dev_key_state.toggle_step_mode) {
            dev_mode->step_mode = !dev_mode->step_mode;
        }

        if (!dev_mode->prev_key_state.step_forward &&
            current_dev_key_state.step_forward) {
            if (dev_mode->step_mode) {
                dev_mode->should_step = true;
            }
        }

        if (!dev_mode->prev_key_state.place_taco &&
            current_dev_key_state.place_taco) {
            int mouse_x = 0;
            int mouse_y = 0;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            S32 cell_x = mouse_x / cell_size;
            S32 cell_y = mouse_y / cell_size;
            if (game_empty_at(game, cell_x, cell_y)) {
                level_set_cell(&game->level, cell_x, cell_y, CELL_TYPE_TACO);
            }
        }
    }

    dev_mode->prev_key_state = current_dev_key_state;
}

void dev_mode_handle_mouse(DevMode* dev_mode,
                           Game* game,
                           UIMouseState* ui_mouse_state,
                           S32 cell_size) {
    if (!dev_mode->enabled) {
        return;
    }

    if (ui_mouse_state->left_clicked) {
        S32 cell_x = ui_mouse_state->x / cell_size;
        S32 cell_y = ui_mouse_state->y / cell_size;

        switch (dev_mode->snake_selection_state) {
        case SNAKE_SELECTION_STATE_NONE: {
           S32 selected_snake_index = game_query_for_snake_at(game, cell_x, cell_y);
           if (selected_snake_index >= 0) {
               dev_mode->snake_selection_index = selected_snake_index;
               dev_mode->snake_selection_state = SNAKE_SELECTION_STATE_SELECTED;
           }
           break;
        }
        case SNAKE_SELECTION_STATE_SELECTED: {
            S32 selected_snake_index = game_query_for_snake_at(game, cell_x, cell_y);
            if (selected_snake_index < 0) {
                Snake* snake = game->snakes + dev_mode->snake_selection_index;
                snake->length = 1;
                snake->segments[0].x = (S16)(cell_x);
                snake->segments[0].y = (S16)(cell_y);
                dev_mode->snake_selection_state = SNAKE_SELECTION_STATE_PLACING;
            } else if (selected_snake_index != dev_mode->snake_selection_index) {
                dev_mode->snake_selection_index = selected_snake_index;
            }
            break;
        }
        case SNAKE_SELECTION_STATE_PLACING: {
            Snake* snake = game->snakes + dev_mode->snake_selection_index;
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

                CellType cell_type = level_get_cell(&game->level, cell_x, cell_y);
                if (cell_type != CELL_TYPE_EMPTY) {
                   break;
                }
                if (game_query_for_snake_at(game, cell_x, cell_y) >= 0) {
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
        if (dev_mode->snake_selection_state == SNAKE_SELECTION_STATE_PLACING) {
            dev_mode->snake_selection_state = SNAKE_SELECTION_STATE_NONE;
        }
    }
}
