#ifndef dev_mode_h
#define dev_mode_h

#include "game.h"
#include "pixelfont.h"
#include "ui.h"

#include <stdbool.h>

typedef enum {
    SNAKE_SELECTION_STATE_NONE,
    SNAKE_SELECTION_STATE_SELECTED,
    SNAKE_SELECTION_STATE_PLACING,
} DevModeSnakeSelectionState;

typedef struct {
    bool toggle_enabled;
    bool toggle_step_mode;
    bool step_forward;
    bool place_taco;
} DevModeKeyState;

typedef struct {
    bool enabled;
    bool step_mode;
    bool should_step;
    DevModeSnakeSelectionState snake_selection_state;
    S32 snake_selection_index;
    DevModeKeyState prev_key_state;
} DevMode;

bool dev_mode_should_step(const DevMode *dev_mode);
void dev_mode_draw(DevMode* dev_mode, Game* game, PF_Font* font, S32 window_width, S32 cell_size);
void dev_mode_handle_keystate(DevMode* dev_mode,
                              Game* game,
                              S32 cell_size,
                              const bool* keyboard_state,
                              UIMouseState* ui_mouse_state);
void dev_mode_handle_mouse(DevMode* dev_mode,
                           Game* game,
                           UIMouseState* ui_mouse_state,
                           S32 cell_size);

#endif
