//
//  lcd.h
//  TacoQuest
//
//  Created by Thomas Foster on 1/31/26.
//

#ifndef lcd_h
#define lcd_h

#include <SDL3/SDL.h>
#include "ints.h"

typedef struct {
    SDL_Renderer * renderer;
    int border_size;
    int pixels_per_row;
    int pixels_per_col;
    int pixel_size; // How many pixels is a pixel?
    int shadow_offset;
    int shadow_alpha;
    int pixel_gap;
} LCD_Info;

typedef struct {
    LCD_Info info;
    SDL_Texture * background_texture;
    SDL_Texture * screen_texture;
} LCD_Screen;

LCD_Screen * lcd_create_screen(const LCD_Info * info);
void lcd_get_screen_size(const LCD_Info * info, int * width, int * height);
void lcd_set_background(LCD_Screen * screen, U8 r, U8 g, U8 b);
void lcd_render_surface(LCD_Screen * screen,
                       SDL_Surface * surface,
                       SDL_Rect * _src_rect,
                       int x,
                       int y);
void lcd_render_screen(LCD_Screen * screen, int x, int y, int scale);

#endif /* lcd_h */
