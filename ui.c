#include "ui.h"

#include <assert.h>
#include <string.h>

S32 ui_slider_width(UISlider* slider) { return slider->pixel_width + (2 * UI_SLIDER_H_PADDING) + 2; }

S32 ui_slider_height(UISlider* slider, S32 font_height) {
    (void)(slider);
    return 2 * font_height + (2 * UI_SLIDER_V_PADDING);
}

S32 ui_dropdown_width(const char** options,
                      S32 option_count,
                      S32 font_width,
                      S32 font_spacing) {
    S32 longest_option_len = 0;
    for (S32 i = 0; i < option_count; i++) {
        S32 option_len = (S32)(strnlen(options[i], UI_MAX_TEXT_LEN));
        if (option_len > longest_option_len) {
            longest_option_len = option_len;
        }
    }
    return (2 + longest_option_len) * (font_width + font_spacing) +
           (2 * UI_DROPDOWN_H_PADDING);
}

S32 ui_dropdown_height(UIDropDown* drop_down, S32 option_count, S32 font_height) {
    S32 result = font_height + 2 * UI_DROPDOWN_V_PADDING;
    if (drop_down->dropped) {
        result += (option_count - 1) * font_height;
    }
    return result;
}

void ui_create(UserInterface* ui, PF_Font* font) {
    ui->font = font;

    ui->outline_color.alpha = 255;
    ui->outline_color.red = 255;
    ui->outline_color.green = 255;
    ui->outline_color.blue = 255;

    ui->background_color.alpha = 255;
    ui->background_color.red = 0;
    ui->background_color.green = 0;
    ui->background_color.blue = 0;

    ui->hovering_color.alpha = 255;
    ui->hovering_color.red = 75;
    ui->hovering_color.green = 75;
    ui->hovering_color.blue = 75;

    ui->activated_color.alpha = 255;
    ui->activated_color.red = 105;
    ui->activated_color.green = 105;
    ui->activated_color.blue = 105;
}

void ui_checkbox(UserInterface* ui,
                 SDL_Renderer* renderer,
                 UIMouseState* mouse_state,
                 UICheckBox* checkbox,
                 bool* value) {
    bool mouse_is_over = mouse_state->x >= checkbox->x &&
                         mouse_state->x <= (checkbox->x + UI_CHECKBOX_SIZE) &&
                         mouse_state->y >= checkbox->y &&
                         mouse_state->y <= (checkbox->y + UI_CHECKBOX_SIZE);

    UIColor active_background_color = ui->background_color;
    if (mouse_is_over) {
        active_background_color = ui->hovering_color;
    }

    // Update element if clicked.
    if (!mouse_state->prev_left_clicked && mouse_state->left_clicked && mouse_is_over) {
        *value = !*value;
    }

    // Outline
    SDL_FRect outline_rect = {
        (float)(checkbox->x),
        (float)(checkbox->y),
        (float)(UI_CHECKBOX_SIZE),
        (float)(UI_CHECKBOX_SIZE)};
    SDL_SetRenderDrawColor(renderer,
                           ui->outline_color.red,
                           ui->outline_color.green,
                           ui->outline_color.blue,
                           ui->outline_color.alpha);
    SDL_RenderFillRect(renderer, &outline_rect);

    // Background
    SDL_FRect background_rect = {
        (float)(outline_rect.x + UI_CHECKBOX_OUTLINE_SIZE),
        (float)(outline_rect.y + UI_CHECKBOX_OUTLINE_SIZE),
        (float)(outline_rect.w - (2 * UI_CHECKBOX_OUTLINE_SIZE)),
        (float)(outline_rect.h - (2 * UI_CHECKBOX_OUTLINE_SIZE))};
    SDL_SetRenderDrawColor(renderer,
                           active_background_color.red,
                           active_background_color.green,
                           active_background_color.blue,
                           active_background_color.alpha);
    SDL_RenderFillRect(renderer, &background_rect);

    if (*value) {
        // Checkmark
        SDL_FRect checked_rect = {
            (float)(outline_rect.x + UI_CHECKBOX_FILL_GAP),
            (float)(outline_rect.y + UI_CHECKBOX_FILL_GAP),
            (float)(outline_rect.w - (2 * UI_CHECKBOX_FILL_GAP)),
            (float)(outline_rect.h - (2 * UI_CHECKBOX_FILL_GAP))};
        SDL_SetRenderDrawColor(renderer,
                               ui->outline_color.red,
                               ui->outline_color.green,
                               ui->outline_color.blue,
                               ui->outline_color.alpha);
        SDL_RenderFillRect(renderer, &checked_rect);
    }
}

void ui_slider(UserInterface* ui,
               SDL_Renderer* renderer,
               UIMouseState* mouse_state,
               UISlider* slider,
               S32* value) {
    PF_FontState font_state = PF_GetState(ui->font);

    S32 width = ui_slider_width(slider);
    S32 height = ui_slider_height(slider, font_state.char_height);

    // Outline
    SDL_FRect outline_rect = {
        (float)(slider->x),
        (float)(slider->y),
        (float)(width),
        (float)(height)};
    SDL_SetRenderDrawColor(renderer,
                           ui->outline_color.red,
                           ui->outline_color.green,
                           ui->outline_color.blue,
                           ui->outline_color.alpha);
    SDL_RenderFillRect(renderer, &outline_rect);

    // TODO: Compress this logic.
    bool mouse_is_over = mouse_state->x >= slider->x &&
                         mouse_state->x <= (slider->x + width) &&
                         mouse_state->y >= slider->y &&
                         mouse_state->y <= (slider->y + height);

    UIColor active_background_color = ui->background_color;
    if (mouse_is_over && !slider->active) {
        active_background_color = ui->hovering_color;
    }

    // Background
    SDL_FRect background_rect = {
        (float)(outline_rect.x + UI_SLIDER_OUTLINE_SIZE),
        (float)(outline_rect.y + UI_SLIDER_OUTLINE_SIZE),
        (float)(outline_rect.w - (2 * UI_SLIDER_OUTLINE_SIZE)),
        (float)(outline_rect.h - (2 * UI_SLIDER_OUTLINE_SIZE))};
    SDL_SetRenderDrawColor(renderer,
                           active_background_color.red,
                           active_background_color.green,
                           active_background_color.blue,
                           active_background_color.alpha);
    SDL_RenderFillRect(renderer, &background_rect);

    // Line
    SDL_FRect line_rect = {
        (float)(slider->x + 2),
        (float)(slider->y + (height / 2)),
        (float)(width - 4),
        (float)(1)};
    SDL_SetRenderDrawColor(renderer,
                           ui->outline_color.red,
                           ui->outline_color.green,
                           ui->outline_color.blue,
                           ui->outline_color.alpha);
    SDL_RenderFillRect(renderer, &line_rect);

    if (!mouse_state->prev_left_clicked && mouse_state->left_clicked && mouse_is_over) {
        slider->active = true;
    }

    S32 slider_range = slider->max - slider->min;

    if (slider->active) {
        if (!mouse_state->left_clicked) {
            slider->active = false;
        } else {
            S32 mouse_x = (S32)(mouse_state->x - (slider->x + UI_SLIDER_H_PADDING));
            if (mouse_x < 0) {
                mouse_x = 0;
            }
            if (mouse_x >= slider->pixel_width) {
                mouse_x = slider->pixel_width;
            }

            float slider_scale = (float)(mouse_x) / (float)(slider->pixel_width);
            *value = slider->min + (S32)(slider_scale * (float)(slider_range));
        }
    }

    // Slider
    float slider_scale = (S32)(*value - slider->min) / (float)(slider_range);
    float slider_pixel = (float)(slider->pixel_width) * slider_scale;
    S32 slider_x = slider->x + (S32)(slider_pixel);
    SDL_FRect slider_rect = {
        (float)(UI_SLIDER_H_PADDING + slider_x),
        (float)(slider->y + UI_SLIDER_V_PADDING),
        (float)(UI_SLIDER_CONTROL_WIDTH),
        (float)(height - 4)};
    SDL_SetRenderDrawColor(renderer,
                           ui->outline_color.red,
                           ui->outline_color.green,
                           ui->outline_color.blue,
                           ui->outline_color.alpha);
    SDL_RenderFillRect(renderer, &slider_rect);
}

void ui_dropdown(UserInterface* ui,
                 SDL_Renderer* renderer,
                 UIMouseState* mouse_state,
                 UIDropDown* drop_down,
                 char** options,
                 S32 option_count,
                 S32* selected) {
    assert(*selected >= 0 && *selected < option_count);

    PF_FontState font_state = PF_GetState(ui->font);

    S32 font_height = (S32)(font_state.char_height * font_state.scale);

    S32 width = ui_dropdown_width(options,
                                  option_count,
                                  (S32)(font_state.char_width * font_state.scale),
                                  font_state.letter_spacing);
    S32 height = ui_dropdown_height(drop_down, option_count, font_height);

    // Outline
    SDL_FRect outline_rect = {
        (float)(drop_down->x),
        (float)(drop_down->y),
        (float)(width),
        (float)(height)};
    SDL_SetRenderDrawColor(renderer,
                           ui->outline_color.red,
                           ui->outline_color.green,
                           ui->outline_color.blue,
                           ui->outline_color.alpha);
    SDL_RenderFillRect(renderer, &outline_rect);

    // TODO: Compress this logic.
    bool mouse_is_over = mouse_state->x >= drop_down->x &&
                         mouse_state->x <= (drop_down->x + width) &&
                         mouse_state->y >= drop_down->y &&
                         mouse_state->y <= (drop_down->y + height);

    UIColor active_background_color = ui->background_color;
    if (mouse_is_over && !drop_down->dropped) {
        active_background_color = ui->hovering_color;
    }

    // Background
    SDL_FRect background_rect = {
        (float)(outline_rect.x + 1),
        (float)(outline_rect.y + 1),
        (float)(outline_rect.w - 2),
        (float)(outline_rect.h - 2)};
    SDL_SetRenderDrawColor(renderer,
                           active_background_color.red,
                           active_background_color.green,
                           active_background_color.blue,
                           active_background_color.alpha);
    SDL_RenderFillRect(renderer, &background_rect);

    // If click occurs, set whether active based on mouse position.
    if (mouse_state->prev_left_clicked && !mouse_state->left_clicked) {
        if (drop_down->dropped) {
            // Figure out which option we're over and select it.
            S32 left = drop_down->x;
            S32 right = drop_down->x + width;
            for (S32 i = 0; i < option_count; i++) {
                S32 bottom =
                    drop_down->y + (i * (font_height));
                S32 top = bottom + font_height;
                if (mouse_state->x >= left &&
                    mouse_state->x <= right &&
                    mouse_state->y >= bottom &&
                    mouse_state->y <= top) {
                    *selected = i;
                    drop_down->dropped = false;
                    break;
                }
            }
            if (drop_down->dropped) {
                drop_down->dropped = false;
            }
        } else {
            drop_down->dropped = mouse_is_over;
        }
    }

    // Draw the drop down indicator
    if (drop_down->dropped) {
        // Draw a hovering box behind the text that the mouse is over.
        // TODO: compress with above.
        S32 left = drop_down->x;
        S32 right = drop_down->x + width;
        for (S32 i = 0; i < option_count; i++) {
            S32 bottom = drop_down->y + UI_DROPDOWN_V_PADDING + (i * font_height);
            S32 top = bottom + font_height;
            if (mouse_state->x >= left &&
                mouse_state->x <= right &&
                mouse_state->y >= bottom &&
                mouse_state->y <= top) {
                SDL_FRect hovering_rect = {
                    (float)(left + 1),
                    (float)(bottom),
                    (float)(width - UI_DROPDOWN_H_PADDING),
                    (float)(font_height)};
                SDL_SetRenderDrawColor(renderer,
                                       ui->hovering_color.red,
                                       ui->hovering_color.green,
                                       ui->hovering_color.blue,
                                       ui->hovering_color.alpha);
                SDL_RenderFillRect(renderer, &hovering_rect);
                break;
            }
        }

        PF_RenderString(ui->font,
                        drop_down->x + UI_DROPDOWN_H_PADDING,
                        drop_down->y + UI_DROPDOWN_V_PADDING,
                        "-");

        // Draw all the text options.
        for (S32 i = 0; i < option_count; i++) {
            PF_RenderString(
                ui->font,
                2 * (font_state.char_width + font_state.letter_spacing) + drop_down->x + UI_DROPDOWN_H_PADDING,
                drop_down->y + UI_DROPDOWN_V_PADDING + (i * font_height),
                options[i]);
        }
    } else {
        // Draw the selected text.
        PF_RenderString(
            ui->font,
            2 * (font_state.char_width + font_state.letter_spacing) + drop_down->x + UI_DROPDOWN_H_PADDING,
            drop_down->y + UI_DROPDOWN_V_PADDING,
            options[*selected]);

        PF_RenderString(
            ui->font,
            drop_down->x + UI_DROPDOWN_H_PADDING,
            drop_down->y + UI_DROPDOWN_V_PADDING,
            "+");
    }
}
