//
// User Interface Elements
//
#pragma once

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "ints.h"
#include "pixelfont.h"

#define UI_MAX_TEXT_LEN 128

typedef struct {
    bool prev_left_clicked;
    bool left_clicked;
    bool prev_right_clicked;
    bool right_clicked;
    float x;
    float y;
} UIMouseState;

#define UI_CHECKBOX_SIZE 20
#define UI_CHECKBOX_OUTLINE_SIZE 2
#define UI_CHECKBOX_FILL_GAP 5

typedef struct {
    S32 x;
    S32 y;
} UICheckBox;

#define UI_SLIDER_OUTLINE_SIZE 2
#define UI_SLIDER_CONTROL_WIDTH 6
#define UI_SLIDER_H_PADDING 4
#define UI_SLIDER_V_PADDING 2

typedef struct  {
    S32 x;
    S32 y;
    S32 pixel_width;
    S32 min;
    S32 max;
    bool active;
} UISlider;

S32 ui_slider_width(UISlider* slider);
S32 ui_slider_height(UISlider* slider, S32 font_height);

#define UI_DROPDOWN_H_PADDING 4
#define UI_DROPDOWN_V_PADDING 4

typedef struct {
    S32 x;
    S32 y;
    char** options;
    S32 option_count;

    S32 selected;
    bool dropped;
} UIDropDown;

S32 ui_dropdown_width(UIDropDown* drop_down, S32 font_width, S32 font_spacing);
S32 ui_dropdown_height(UIDropDown* drop_down, S32 font_height);

typedef struct {
    U8 red;
    U8 green;
    U8 blue;
    U8 alpha;
} UIColor;

// Performs immediate mode gui similar to another library seen on the internet.
typedef struct {
    PF_Font* font;
    UIColor outline_color;
    UIColor background_color;
    UIColor hovering_color;
    UIColor activated_color;
} UserInterface;

void ui_create(UserInterface* ui, PF_Font* font);
void ui_checkbox(UserInterface* ui,
                 SDL_Renderer* renderer,
                 UIMouseState* mouse_state,
                 UICheckBox* checkbox,
                 bool* value);
void ui_slider(UserInterface* ui,
               SDL_Renderer* renderer,
               UIMouseState* mouse_state,
               UISlider* slider,
               S32* value);
void ui_dropdown(UserInterface* ui,
                 SDL_Renderer* renderer,
                 UIMouseState* mouse_state,
                 UIDropDown* drop_down);
