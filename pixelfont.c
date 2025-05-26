#include "pixelfont.h"
#include <stdio.h>
#include <stdarg.h>

#define PF_BUFFER_SIZE 255
#define PF_SET_ERROR(fmt, ...) \
snprintf(pf_error, PF_BUFFER_SIZE, "%s Error: " fmt, __func__, ##__VA_ARGS__)

static char pf_error[PF_BUFFER_SIZE + 1];

struct PF_PixelFont {
    SDL_Renderer * renderer;
    SDL_Texture * texture;
    PF_FontInfo info;

    // Number of character columns in the texture.
    char first_char;
    Uint16 cols;

    // TODO: line_spacing and handle new line option?
};

static inline SDL_Color
GetRenderColor(SDL_Renderer * renderer)
{
    SDL_Color color;
    SDL_GetRenderDrawColor(renderer, &color.r, &color.g, &color.b, &color.a);
    return color;
}

static inline void
SetRenderColor(SDL_Renderer * renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static inline void
SetColor(SDL_Color * color, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    *color = (SDL_Color){ r, g, b, a };
}

static inline void
GetColor(const SDL_Color * color, Uint8 * r, Uint8 * g, Uint8 * b, Uint8 * a)
{
    if ( r ) *r = color->r;
    if ( g ) *g = color->g;
    if ( b ) *b = color->b;
    if ( a ) *a = color->a;
}

static void
RenderChar(PF_Font * font, int x, int y, char ch)
{
    ch -= font->first_char;

    const Uint8 char_w = font->info.char_width;
    const Uint8 char_h = font->info.char_height;

    SDL_Rect src = {
        .x = (ch % font->cols) * char_w,
        .y = (ch / font->cols) * char_h,
        .w = char_w,
        .h = char_h
    };

    SDL_Rect dst = {
        .x = x,
        .y = y,
        .w = char_w * font->info.scale,
        .h = char_h * font->info.scale
    };

    SDL_SetTextureColorMod(font->texture,
                           font->info.foreground.r,
                           font->info.foreground.g,
                           font->info.foreground.b);
    SDL_SetTextureAlphaMod(font->texture, font->info.foreground.a);
    SDL_RenderCopy(font->renderer, font->texture, &src, &dst);
}

/** `width` is in characters */
static inline void
RenderBackground(PF_Font * font, int x, int y, int width)
{
    if ( font->info.background.a == 0 ) {
        return;
    }

    SDL_Rect dst = {
        .x = x,
        .y = y,
        .w = width * font->info.char_width,
        .h = font->info.char_height
    };

    SDL_Color saved = GetRenderColor(font->renderer);
    SetRenderColor(font->renderer, font->info.background);
    SDL_RenderFillRect(font->renderer, &dst);
    SetRenderColor(font->renderer, saved); // Set it back!
}

// PUBLIC:

PF_Font *
PF_LoadFont(const PF_Config * config)
{
    pf_error[0] = '\0';

    if ( config->renderer == NULL ) {
        PF_SET_ERROR("renderer is NULL\n");
        return NULL;
    }

    if ( config->bmp_file == NULL ) {
        PF_SET_ERROR("bitmap parameter is NULL\n");
        return NULL;
    }

    if ( config->char_width == 0 ) {
        PF_SET_ERROR("character width is 0. Don't do that.");
        return NULL;
    }

    if ( config->char_height == 0 ) {
        PF_SET_ERROR("character height is 0. Don't do that.");
        return NULL;
    }

    PF_Font * font = calloc(1, sizeof(*font));
    SDL_Surface * surface = NULL;

    if ( font == NULL ) {
        PF_SET_ERROR("could not allocate font\n");
        goto error;
    }

    surface = SDL_LoadBMP(config->bmp_file);

    if ( surface == NULL ) {
        PF_SET_ERROR("failed to load '%s'\n", config->bmp_file);
        goto error;
    }

    Uint32 key = SDL_MapRGB(surface->format,
                            config->bmp_background.r,
                            config->bmp_background.g,
                            config->bmp_background.b);
    SDL_SetColorKey(surface, SDL_TRUE, key);

    font->texture = SDL_CreateTextureFromSurface(config->renderer, surface);

    if ( font->texture == NULL ) {
        PF_SET_ERROR("failed to create font texture for '%s'\n", config->bmp_file);
        goto error;
    }

    font->renderer = config->renderer;
    font->first_char = config->first_char;
    font->info.char_width = config->char_width;
    font->info.char_height = config->char_height;
    font->info.scale = 1;

    font->cols = surface->w / config->char_width;

    SDL_FreeSurface(surface);

    return font;

error:
    if ( surface ) {
        SDL_FreeSurface(surface);
    }

    if ( font ) {
        if ( font->texture ) {
            SDL_DestroyTexture(font->texture);
        }
        free(font);
    }

    return NULL;
}

const char *
PF_GetError(void)
{
    return pf_error;
}

void
PF_DestroyFont(PF_Font * font)
{
    SDL_DestroyTexture(font->texture);
}

void
PF_SetLetterSpacing(PF_Font * font, int new_value)
{
    font->info.letter_spacing = new_value;
}

int
PF_GetLetterSpacing(PF_Font * font)
{
    return font->info.letter_spacing;
}

void
PF_SetForeground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SetColor(&font->info.foreground, r, g, b, a);
}

void
PF_SetBackground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SetColor(&font->info.background, r, g, b, a);
}

void
PF_SetHorizontalPositioning(PF_Font * font, PF_HorizontalPositioning hpos)
{
    font->info.h_positioning = hpos;
}

void
PF_SetScale(PF_Font * font, float scale)
{
    font->info.scale = scale;
}

void
PF_SetVerticalPositioning(PF_Font * font, PF_VerticalPositioning vpos)
{
    font->info.v_positioning = vpos;
}


PF_FontInfo PF_GetInfo(PF_Font * font)
{
    return font->info;
}

void
PF_RenderChar(PF_Font * font, int x, int y, char ch)
{
    RenderBackground(font, x, y, 1);
    RenderChar(font, x, y, ch);
}

Uint32
PF_RenderString(PF_Font * font, int x, int y, const char * format, ...)
{
    char buffer[PF_BUFFER_SIZE + 1]; // TODO: (Maybe) grow this exponentially.

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, PF_BUFFER_SIZE, format, args);
    va_end(args);

    int len = (int)strlen(buffer);

    if ( len >= PF_BUFFER_SIZE ) {
        PF_SET_ERROR("Warning: string '%s' has length greater than %d "
                     "and will be truncated.",
                     buffer, PF_BUFFER_SIZE);
    }

    Uint32 text_w = len * font->info.char_width;
    Uint32 text_h = font->info.char_height;

    // Apply horizontal positing
    if ( font->info.h_positioning == PF_HPOS_CENTER ) {
        x -= text_w / 2;
    } else if ( font->info.h_positioning == PF_HPOS_RIGHT ) {
        x -= text_w;
    }

    // Apply vertical positing
    if ( font->info.v_positioning == PF_VPOS_CENTER ) {
        y -= text_h / 2;
    } else if ( font->info.v_positioning == PF_VPOS_BOTTOM ) {
        y -= text_h;
    }

    RenderBackground(font, x, y, len);

    const char * ch = buffer;
    Uint32 x1 = x;
    while ( *ch ) {
        RenderChar(font, x1, y, *ch);
        x1 += (font->info.char_width + font->info.letter_spacing) * font->info.scale;
        ch++;
    }

    return x1 - x;
}
