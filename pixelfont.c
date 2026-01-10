#include "pixelfont.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#define PF_BUFFER_SIZE 255
#define PF_SET_ERROR(fmt, ...) \
snprintf(pf_error, PF_BUFFER_SIZE, fmt, ##__VA_ARGS__)

static char pf_error[PF_BUFFER_SIZE + 1];

struct PF_PixelFont {
    SDL_Renderer * renderer;
    SDL_Texture * texture;
    PF_FontState state;

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

// TODO: What is this
//static inline void
//GetColor(const SDL_Color * color, Uint8 * r, Uint8 * g, Uint8 * b, Uint8 * a)
//{
//    if ( r ) *r = color->r;
//    if ( g ) *g = color->g;
//    if ( b ) *b = color->b;
//    if ( a ) *a = color->a;
//}

static void
RenderChar(PF_Font * font, int x, int y, char ch)
{
    ch -= font->first_char;

    const uint8_t char_w = (uint8_t)(font->state.char_width);
    const uint8_t char_h = (uint8_t)(font->state.char_height);

    SDL_FRect src = {
        .x = (float)((ch % font->cols) * char_w),
        .y = (float)((ch / font->cols) * char_h),
        .w = (float)(char_w),
        .h = (float)(char_h)
    };

    SDL_FRect dst = {
        .x = (float)(x),
        .y = (float)(y),
        .w = (float)(char_w * font->state.scale),
        .h = (float)(char_h * font->state.scale)
    };

    SDL_SetTextureColorMod(font->texture,
                           font->state.foreground.r,
                           font->state.foreground.g,
                           font->state.foreground.b);
    SDL_SetTextureAlphaMod(font->texture, font->state.foreground.a);
    SDL_RenderTexture(font->renderer, font->texture, &src, &dst);
}

/** `width` is in characters */
static inline void
RenderBackground(PF_Font * font, int x, int y, int width)
{
    if ( font->state.background.a == 0 ) {
        return;
    }

    SDL_FRect dst = {
        .x = (float)(x),
        .y = (float)(y),
        .w = (float)(width * font->state.char_width),
        .h = (float)(font->state.char_height)
    };

    SDL_Color saved = GetRenderColor(font->renderer);
    SetRenderColor(font->renderer, font->state.background);
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

    Uint32 key = SDL_MapRGB(SDL_GetPixelFormatDetails(surface->format),
                            NULL,
                            config->bmp_background.r,
                            config->bmp_background.g,
                            config->bmp_background.b);
    SDL_SetSurfaceColorKey(surface, true, key);

    font->texture = SDL_CreateTextureFromSurface(config->renderer, surface);

    if ( font->texture == NULL ) {
        PF_SET_ERROR("failed to create font texture for '%s'\n", config->bmp_file);
        goto error;
    }

    if(!SDL_SetTextureScaleMode(font->texture, SDL_SCALEMODE_NEAREST)) {
        PF_SET_ERROR("Failed to set texture scale mode on '%s' error: %s\n",
                     config->bmp_file,
                     SDL_GetError());
        goto error;
    }

    font->renderer = config->renderer;
    font->first_char = (char)(config->first_char);
    font->state.char_width = config->char_width;
    font->state.char_height = config->char_height;
    font->state.scale = 1;

    font->cols = (Uint16)(surface->w / config->char_width);

    SDL_DestroySurface(surface);

    return font;

error:
    if ( surface ) {
        SDL_DestroySurface(surface);
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
    font->state.letter_spacing = new_value;
}

int
PF_GetLetterSpacing(PF_Font * font)
{
    return font->state.letter_spacing;
}

void
PF_SetForeground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SetColor(&font->state.foreground, r, g, b, a);
}

void
PF_SetBackground(PF_Font * font, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SetColor(&font->state.background, r, g, b, a);
}

void
PF_SetScale(PF_Font * font, float scale)
{
    font->state.scale = scale;
}

PF_FontState PF_GetState(PF_Font * font)
{
    return font->state;
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

    Uint32 text_w = (Uint32)(len * font->state.char_width * font->state.scale);
    Uint32 text_h = (Uint32)(font->state.char_height * font->state.scale);

    // Apply horizontal positing.
    if ( x & PF_CENTER ) {
        x -= text_w / 2;
    }

    if ( x & PF_RIGHT ) {
        x -= text_w;
    }

    // Apply vertical positing.
    if ( y & PF_CENTER ) {
        y -= text_h / 2;
    }

    if ( y & PF_BOTTOM ) {
        y -= text_h;
    }

    // Clear positioning bits.
    x &= 0xFFFF;
    y &= 0xFFFF;

    RenderBackground(font, x, y, len);

    const char * ch = buffer;
    Uint32 x1 = x;
    while ( *ch ) {
        RenderChar(font, x1, y, *ch);
        x1 += (Uint32)((font->state.char_width + font->state.letter_spacing) * font->state.scale);
        ch++;
    }

    return x1 - x;
}
