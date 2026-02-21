#ifndef demo_h
#define demo_h

#include <stdio.h>
#include <stdint.h>
#include "snake.h"
#include "game.h"
#include "ints.h"

typedef struct {
    SnakeColor snake_color;
    S16 initial_x;
    S16 initial_y;
    Direction direction;
    S32 length;
} SnakeInfo;

typedef struct {
    U32 version;
    char map_name[64];
    U8 num_players;
    SnakeInfo snakes[MAX_SNAKE_COUNT];
    GameSettings settings;
} DemoHeader;

FILE* create_demo_file(void);

int demo_write_header(DemoHeader* header, FILE* file);

// Caller should free returned memory
DemoHeader* demo_read_header(FILE* file);

#endif /* demo_h */