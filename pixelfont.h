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

#define PF_CENTER   0x10000
#define PF_RIGHT    0x20000
#define PF_BOTTOM   0x20000

typedef struct PF_PixelFont PF_Font;

/** Configuration info used when loading a font. */
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

/** The current state of a `PF_Font`: the current foreground color, the current
 *  foreground color, the current letter spacing, etc.
 */
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

    /** The current rending scale, as set with `PF_SetScale`. */
    float scale;
} PF_FontState;

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

PF_FontState PF_GetState(PF_Font * font);

void PF_SetLetterSpacing(PF_Font * font, int new_spacing);

/** Set the current color for rendering text. */
void PF_SetForeground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Set the current color for rendering text background. */
void PF_SetBackground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Set the current rendering scale. */
void PF_SetScale(PF_Font * font, float scale);

/**
 *  Render character at pixel coordinate with the font's current rendering color.
 */
void PF_RenderChar(PF_Font * font, int x, int y, char character);

/**
 *  Render format string at pixel coordinate with the font's current rendering
 *  color.
 *  - note: By default, text is positioned with its top-left corner at `x`, `y`.
 *      To justify text, `x` may be bitwise OR'd with `PF_CENTER` or `PF_RIGHT`
 *      and `y` may be OR'd with `PF_CENTER` or `PF_BOTTOM`.
 *  - returns: Returns the width of the rendered string in pixels.
 */
Uint32 PF_RenderString(PF_Font * font, int x, int y, const char * format, ...);

#endif /* PIXELFONT_H */
