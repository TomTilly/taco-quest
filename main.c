#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "SDL2/SDL.h"

int32_t main () {
    printf("hello taco\n");

    int rc = SDL_Init(SDL_INIT_EVERYTHING);
    if (rc < 0) {
        printf("SDL_Init failed %s\n", SDL_GetError());
        return 1;
    }

    const char* title = "Taco Quest";
    SDL_Window* window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    if (window == NULL) {
        printf("SDL_CreateWindow failed %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface* window_surface = SDL_GetWindowSurface(window);

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
                break;
            }
        }

        SDL_FillRect(window_surface, NULL, SDL_MapRGB(window_surface->format, 0x00, 0x00, 0x00));
        SDL_UpdateWindowSurface(window);
        SDL_Delay(0);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
