/*
 *  pixelfont.h
 *
 *  A library for loading and rendering pixel fonts using SDL2.
 *
 *  Author: Tom Foster
 *  Email: foster.pianist@gmail.com
 *  Date: Oct. 20, 2024
 *
 *  Copyright (C) 2024 Tom Foster
 */

#ifndef PIXELFONT_H
#define PIXELFONT_H

#include <SDL2/SDL_render.h>

typedef struct PF_PixelFont PF_Font;

typedef enum {
    PF_HPOS_LEFT, // Render text left-justified at given coordinate.
    PF_HPOS_CENTER, // Render text centered at given coordinate.
    PF_HPOS_RIGHT // Render text right-justified at given coordinate.
} PF_HorizontalPositioning;

typedef enum {
    PF_VPOS_TOP,
    PF_VPOS_CENTER,
    PF_VPOS_BOTTOM
} PF_VerticalPositioning;

typedef struct {
    /** The renderer used to load the `PF_Font`. */
    SDL_Renderer * renderer;

    /** The path to the bitmap file containing the character sheet. */
    const char * bmp_file;

    /** The width of each character in the bitmap, in pixels. */
    int char_width;

    /** The height of each character in the bitmap, in pixels. */
    int char_height;

    /** The ASCII character in the upper left corner of the bitmap. */
    int first_char;

    /** The color of the background of the bitmap file. */
    SDL_Color bmp_background;
} PF_Config;

typedef struct {
    /** Width of font characters in pixels. */
    int char_width;

    /** Height of font characters in pixels. */
    int char_height;

    /** Distance between characters of a rendered string in pixels. */
    int letter_spacing;

    /** The current text rendering color, as set with `PF_SetForeground`. */
    SDL_Color foreground;

    /** The current background rendering color, as set with `PF_SetBackground`. */
    SDL_Color background;

    /** The current horizontal text positioning. */
    PF_HorizontalPositioning h_positioning;

    /** The current vertical text positioning. */
    PF_VerticalPositioning v_positioning;

    float scale;
} PF_FontInfo;

/**
 *  Load a character sprite sheet bitmap.
 *
 *  - parameter config: The configuration information used to load the font.
 *  - returns: A pointer to the allocated `PF_PixelFont` or `NULL` on failure.
 *             Use `PF_GetError` to get a string description of the error.
 *  - important: Characters in the bitmap must be pure white (255, 255, 255).
 */
PF_Font * PF_LoadFont(const PF_Config * config);

void PF_DestroyFont(PF_Font * font);

/** Get a string describing the most recent error. */
const char * PF_GetError(void);

PF_FontInfo PF_GetInfo(PF_Font * font);

void PF_SetLetterSpacing(PF_Font * font, int new_spacing);

/** Set the current color for rendering text. */
void PF_SetForeground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Set the current color for rendering text background. */
void PF_SetBackground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

void PF_SetHorizontalPositioning(PF_Font * font, PF_HorizontalPositioning hpos);
void PF_SetVerticalPositioning(PF_Font * font, PF_VerticalPositioning vpos);
void PF_SetScale(PF_Font * font, float scale);

/**
 *  Render character at pixel coordinate with current rendering color
 *  as set with `PF_SetForeground` and `PF_SetBackground`.
 */
void PF_RenderChar(PF_Font * font, int x, int y, char character);

/**
 *  Render format string at pixel coordinate with current rendering color
 *  as set with `PF_SetForeground` and `PF_SetBackground`.
 *
 *  - returns: The width of the rendered string in pixels.
 */
Uint32 PF_RenderString(PF_Font * font, int x, int y, const char * format, ...);

#endif /* PIXELFONT_H */
