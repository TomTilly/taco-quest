/*
 * Beeper Sound Library
 *
 * author: Thomas Foster
 * created: 04/2022
 * description: Library for emulating IBM PC Speaker sound.
 */
#ifndef beeper_h
#define beeper_h

#include <stdbool.h>

#define NOISE 0x8000 // Experimental!

bool InitSound(void); // Returns false on failure, use SDL_GetError() to check.
void SetVolume(unsigned volume); // 0 to 15
void PlaySound(unsigned frequency, unsigned milliseconds);
void StopSound(void); // Also stops music.
void PlayMusic(const char * music, ...);

#endif /* beeper_h */
