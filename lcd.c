//
//  lcd.c
//  TacoQuest
//
//  Created by Thomas Foster on 1/31/26.
//

#include "lcd.h"
#include <stdarg.h>
#include <string.h>

static char error_string[1024];

static SDL_Texture * _create_texture(SDL_Renderer * renderer,
                                     int width,
                                     int height) {
    SDL_Texture * texture = SDL_CreateTexture(renderer,
                                              SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET,
                                              width,
                                              height);

    if ( texture != NULL ) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }

    return texture;
}

static void _set_error(const char * format, ...)
{
    if (format == NULL) {
        error_string[0] = '\0';
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(error_string, sizeof(error_string), format, args);
    va_end(args);
    error_string[sizeof(error_string) - 1] = '\0';
}

const char * lcd_get_error(void) {
    return error_string;
}

void lcd_get_screen_size(const LCD_Info * info, int * width, int * height) {
    int stride = info->pixel_size + info->pixel_gap;

    if ( width != NULL ) {
        *width = stride * info->pixels_per_row;
    }

    if ( height != NULL ) {
        *height = stride * info->pixels_per_col;
    }
}

void lcd_get_background_size(const LCD_Info * info, int * width, int * height) {
    int border = info->border_size * 2;

    lcd_get_screen_size(info, width, height);

    if ( width != NULL ) {
        *width += border;
    }

    if ( height != NULL ) {
        *height += border;
    }
}

void lcd_set_background(LCD_Screen * screen, U8 r, U8 g, U8 b)
{
    SDL_Renderer * renderer = screen->info.renderer;

    SDL_Texture * old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, screen->background_texture);

    SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, NULL);

    SDL_SetRenderTarget(renderer, old_target); // Restore previous state.
}

void lcd_clear(LCD_Screen * screen)
{
    SDL_Renderer * renderer = screen->info.renderer;

    SDL_Texture * old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, screen->screen_texture);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(renderer, NULL);

    SDL_SetRenderTarget(renderer, old_target); // Restore previous state.
}

LCD_Screen * lcd_create_screen(const LCD_Info * info)
{
    int width, height;

    LCD_Screen * screen = SDL_malloc(sizeof(*screen));
    if ( screen == NULL ) {
        _set_error("%s: could not allocate screen\n", __func__);
        goto error;
    }

    screen->info = *info;

    // Create background texture
    lcd_get_background_size(info, &width, &height);
    screen->background_texture = _create_texture(info->renderer, width, height);
    if ( screen->background_texture == NULL ) {
        _set_error("%s: could not create background texture: %s\n", __func__, SDL_GetError());
        goto error;
    }

    // Create screen texture
    lcd_get_screen_size(info, &width, &height);
    screen->screen_texture = _create_texture(info->renderer, width, height);
    if ( screen->screen_texture == NULL ) {
        _set_error("%s: could not create screen texture: %s\n", __func__, SDL_GetError());
        goto error;
    }

    lcd_set_background(screen, 192, 192, 192);
    lcd_clear(screen);
    return screen;

error:
    if ( screen ) {
        if ( screen->background_texture != NULL ) {
            SDL_DestroyTexture(screen->background_texture);
        }

        if ( screen->screen_texture != NULL ) {
            SDL_DestroyTexture(screen->screen_texture);
        }

        SDL_free(screen);
    }

    return NULL;
}

void lcd_render_surface(LCD_Screen * screen,
                       SDL_Surface * surface,
                       SDL_Rect * _src_rect,
                       int x,
                       int y)
{
    const LCD_Info * info = &screen->info;
    SDL_Renderer * renderer = info->renderer;

    SDL_Texture * old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, screen->screen_texture);

    int stride = info->pixel_size + info->pixel_gap;

    SDL_Rect src_rect;
    if ( _src_rect == NULL ) {
        src_rect = (SDL_Rect){ 0, 0, surface->w, surface->h };
    } else {
        src_rect = *_src_rect;
        // TODO: clamp if bad val
    }

    for ( int src_y = 0; src_y < surface->h; src_y++ ) {
        int dst_y = (y + src_y) * stride;
        if ( dst_y < 0 ) continue;
        if ( dst_y >= screen->info.pixels_per_col ) break;

        for ( int src_x = src_rect.x; src_x < src_rect.x + src_rect.w; src_x++ ) {
            int dst_x = (x + src_x) * stride;
            if ( dst_x < 0 ) continue;
            if ( dst_x >= screen->info.pixels_per_row ) break;

            Uint8 r, g, b, a;
            SDL_ReadSurfacePixel(surface, src_x, src_y, &r, &g, &b, &a);
            printf("%d, %d, %d, %d\n", r, g, b, a);

            if ( a == 0 ) continue;

            int pixel_size = info->pixel_size;
            SDL_FRect pixel_rect = {
                .x = dst_x + info->shadow_offset,
                .y = dst_y + info->shadow_offset,
                .w = pixel_size,
                .h = pixel_size
            };

            // Shadow
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, info->shadow_alpha);
            SDL_RenderFillRect(renderer, &pixel_rect);

            // Pixel
            pixel_rect.x -= info->shadow_offset;
            pixel_rect.y -= info->shadow_offset;
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_RenderFillRect(renderer, &pixel_rect);
        }
    }

    SDL_SetRenderTarget(renderer, old_target);
}

void lcd_render_screen(LCD_Screen * screen, int x, int y, int scale)
{
    SDL_FRect bg_dst_rect = {
        .x = x,
        .y = y,
        .w = screen->background_texture->w * scale,
        .h = screen->background_texture->h * scale
    };

    SDL_RenderTexture(screen->info.renderer,
                      screen->background_texture,
                      NULL,
                      &bg_dst_rect);

    SDL_FRect screen_dst_rect = {
        .x = x + screen->info.border_size,
        .y = y + screen->info.border_size,
        .w = screen->screen_texture->w * scale,
        .h = screen->screen_texture->h * scale
    };

    SDL_RenderTexture(screen->info.renderer,
                      screen->screen_texture,
                      NULL,
                      &screen_dst_rect);
}
