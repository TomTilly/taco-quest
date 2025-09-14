#include "../game.h"

#include <stdio.h>
#include <ctype.h>

bool g_failed = false;

#define EXPECT(condition)                               \
    if (!(condition)) {                                 \
        printf("%s:%d:0 failed\n", __FILE__, __LINE__); \
        g_failed = true;                                \
    }

char char_from_level_string(const char* string, S32 width, S32 x, S32 y) {
    return string[(y * width) + x];
}

char new_char_from_level_string(const char** string, S32 x, S32 y) {
    return *(string[y] + x);
}

bool game_from_string(const char** strings, Game* game) {
    // Process string array to get width and height.
    S32 width = (S32)(strlen(strings[0]));
    S32 height = 0;

    for (S32 i = 0; strings[i] != NULL; i++) {
        if (strlen(strings[i]) != width) {
            printf("Level string mismatch. Good luck finding it. Use a debugger.\n");
            return false;
        }
        height++;
    }

    // Build the level.
    if (!game_init(game, width, height, 0)) {
        return false;
    }

    S32 snake_capacities[MAX_SNAKE_COUNT] = {0};

    for (S16 y = 0; y < height; y++) {
        for (S16 x = 0; x < width; x++) {
            char ch = new_char_from_level_string(strings, x, y);
            level_set_cell(&game->level, x, y, CELL_TYPE_EMPTY);

            if (ch == 'W') {
                level_set_cell(&game->level, x, y, CELL_TYPE_WALL);
            } else if (ch == 'T') {
                level_set_cell(&game->level, x, y, CELL_TYPE_TACO);
            } else if (islower(ch)) {
                snake_capacities[0]++;
            } else if (isupper(ch)) {
                snake_capacities[1]++;
            } else if (isdigit(ch)) {
                snake_capacities[2]++;
            }
        }
    }

    for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
        if (snake_capacities[i] == 0) {
            continue;
        }
        if (!snake_init(game->snakes + i, snake_capacities[i])) {
            return false;
        }
        game->snakes[i].length = snake_capacities[i];
    }

    for (S16 y = 0; y < height; y++) {
        for (S16 x = 0; x < width; x++) {
            char ch = new_char_from_level_string(strings, x, y);
            S32 snake_index = 0;
            S32 segment_index = 0;

            if (islower(ch)) {
                snake_index = 0;
                segment_index = ch - 'a';
            } else if (isupper(ch)) {
                snake_index = 1;
                segment_index = ch - 'A';
            } else if (isdigit(ch)) {
                snake_index = 2;
                segment_index = ch - '0';
            } else {
                continue;
            }

            game->snakes[snake_index].segments[segment_index].x = x;
            game->snakes[snake_index].segments[segment_index].y = y;
            game->snakes[snake_index].segments[segment_index].health = SNAKE_SEGMENT_MAX_HEALTH;
        }
    }
    return true;
}

void print_game(Game* game) {
    char** string = malloc(game->level.height * sizeof(char*));
    S32 string_length = game->level.width + 1; // plus one for null terminator.
    for (S32 i = 0; i < game->level.height; i++) {
        string[i] = malloc(string_length);
        memset(string[i], 0, string_length);
    }

    for (S32 y = 0; y < game->level.height; y++) {
        for (S32 x = 0; x < game->level.width; x++) {
            CellType cell = level_get_cell(&game->level, x, y);
            switch(cell) {
            case CELL_TYPE_EMPTY:
                string[y][x] = '.';
                break;
            case CELL_TYPE_WALL:
                string[y][x] = 'W';
                break;
            case CELL_TYPE_TACO:
                string[y][x] = 'T';
                break;
            default:
                string[y][x] = ' ';
                break;
            }
        }
    }

    char base_chars[MAX_SNAKE_COUNT] = {'a', 'A', '0'};
    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        for (S32 e = 0; e < game->snakes[s].length; e++) {
            SnakeSegment* segment = game->snakes[s].segments + e;
            string[segment->y][segment->x] = (char)(base_chars[s] + e);
        }
    }

    for (S32 i = 0; i < game->level.height; i++) {
        printf("%s\n", string[i]);
    }

    for (S32 i = 0; i < game->level.height; i++) {
        free(string[i]);
    }
    free(string);
}

void print_games(Game* a, Game* b) {
    printf("A:\n");
    print_game(a);
    printf("B:\n");
    print_game(b);
}

bool games_are_equal(Game* a, Game* b) {
    if (a->level.width != b->level.width) {
        printf("width mismatch: %d -> %d\n", a->level.width, b->level.width);
        return false;
    }
    if (a->level.height != b->level.height) {
        printf("height mismatch: %d -> %d\n", a->level.height, b->level.height);
        return false;
    }

    for (S32 y = 0; y < a->level.height; y++) {
        for (S32 x = 0; x < a->level.width; x++) {
            CellType a_cell = level_get_cell(&a->level, x, y);
            CellType b_cell = level_get_cell(&b->level, x, y);
            if (a_cell != b_cell) {
                print_games(a, b);
                return false;
            }
        }
    }

    for (S32 s = 0; s < MAX_SNAKE_COUNT; s++) {
        Snake* a_snake = a->snakes + s;
        Snake* b_snake = b->snakes + s;

        if (a_snake->direction != b_snake->direction) {
            print_games(a, b);
            return false;
        }
        if (a_snake->length != b_snake->length) {
            print_games(a, b);
            return false;
        }

        for (S32 e = 0; e < a_snake->length; e++) {
            SnakeSegment* a_segment = a_snake->segments + e;
            SnakeSegment* b_segment = b_snake->segments + e;

            if (a_segment->x != b_segment->x) {
                print_games(a, b);
                return false;
            }
            if (a_segment->y != b_segment->y) {
                print_games(a, b);
                return false;
            }
            if (a_segment->health != b_segment->health) {
                print_games(a, b);
                return false;
            }
        }
    }

    return true;
}

bool snake_constrict_test(const char** input_level,
                          const char** output_level,
                          SnakeAction snake_action,
                          Direction* snake_directions) {
    Game input_game = {0};
    game_from_string(input_level, &input_game);
    if (snake_directions) {
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            if (input_game.snakes[i].length > 0) {
                input_game.snakes[i].direction = snake_directions[i];
            }
        }
    }

    SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
    snake_actions[0] = snake_action;

    game_update(&input_game, snake_actions);

    Game output_game = {0};
    game_from_string(output_level, &output_game);
    if (snake_directions) {
        for (S32 i = 0; i < MAX_SNAKE_COUNT; i++) {
            if (output_game.snakes[i].length > 0) {
                output_game.snakes[i].direction = snake_directions[i];
            }
        }
    }

    bool result = games_are_equal(&input_game, &output_game);
    game_destroy(&input_game);
    game_destroy(&output_game);
    return result;
}

bool snake_segment_constrict_test(const char** input_level,
                                  const char** output_level,
                                  S32 segment_index,
                                  bool left) {
    Game input_game = {0};
    game_from_string(input_level, &input_game);

    snake_segment_constrict(&input_game, 0, segment_index, left);

    Game output_game = {0};
    game_from_string(output_level, &output_game);

    bool result = games_are_equal(&input_game, &output_game);
    game_destroy(&input_game);
    game_destroy(&output_game);
    return result;
}

bool snake_segment_push_test(const char** input_level,
                             const char** output_level,
                             S32 segment_index,
                             Direction direction) {
    Game input_game = {0};
    game_from_string(input_level, &input_game);

    snake_segment_push(&input_game, 0, segment_index, direction);

    Game output_game = {0};
    game_from_string(output_level, &output_game);

    bool result = games_are_equal(&input_game, &output_game);
    game_destroy(&input_game);
    game_destroy(&output_game);
    return result;
}

int main(int argc, char** argv) {
    (void)(argc);
    (void)(argv);

    // Snake on snake collision
    {
        const char* input_level[] = {
            ".......",
            "..aABC.",
            "..b....",
            "..c....",
            ".......",
            NULL
        };

        const char* output_level[] = {
            "..a....",
            "..bABC.",
            "..c....",
            ".......",
            ".......",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[0] = SNAKE_ACTION_FACE_NORTH;
        snake_actions[1] = SNAKE_ACTION_FACE_WEST;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, &output_game);
        output_game.snakes[1].direction = DIRECTION_WEST;

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    // Snake eat taco
    {
        const char* input_level[] = {
            "..T....",
            "..a....",
            "..b....",
            "..c....",
            ".......",
            NULL
        };

        const char* output_level[] = {
            "..a....",
            "..b....",
            "..c....",
            "..d....",
            ".......",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[0] = SNAKE_ACTION_FACE_NORTH;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, &output_game);

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    // Snake collide with wall
    {
        const char* input_level[] = {
            ".......",
            "..cbaW.",
            ".......",
            ".......",
            ".......",
            NULL
        };

        const char* output_level[] = {
            ".......",
            "..cbaW.",
            ".......",
            ".......",
            ".......",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[0] = SNAKE_ACTION_FACE_EAST;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, &output_game);
        output_game.snakes[0].direction = DIRECTION_EAST;

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    // Snake chomps
    {
        const char* input_level[] = {
            "..W....",
            "..a....",
            "..bABC.",
            "..c....",
            ".......",
            NULL
        };

        const char* output_level[] = {
            "..W....",
            "..a....",
            "..bABC.",
            "..c....",
            ".......",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);
        input_game.snakes[1].direction = DIRECTION_WEST;

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[1] = SNAKE_ACTION_CHOMP;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, &output_game);
        output_game.snakes[1].direction = DIRECTION_WEST;
        output_game.snakes[0].segments[1].health = 1;

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    // Snake chomps low health to turn into tacos
    {
        const char* input_level[] = {
            "..a....",
            "..b....",
            "..cABC.",
            "..d....",
            "..efg..",
            NULL
        };

        const char* output_level[] = {
            "..a....",
            "..b....",
            "..ABCD.",
            "..T....",
            "..TTT..",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);
        input_game.snakes[1].direction = DIRECTION_WEST;
        input_game.snakes[0].segments[2].health = 1;

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[1] = SNAKE_ACTION_CHOMP;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, &output_game);
        output_game.snakes[1].direction = DIRECTION_WEST;

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    // Constrict tests

    // No collision

    // Straight
    // Right Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..b..",
    //         "..c..",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         "..bc.",
    //         "..ed.",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL));
    // }

    // // Left Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..b..",
    //         "..c..",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         ".cb..",
    //         ".de..",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL));
    // }

    // // Right Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         "..c..",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         ".de..",
    //         ".cb..",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         "..c..",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         "..ed.",
    //         "..bc.",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "abcde",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".cd..",
    //         "abe..",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "abcde",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         "abe..",
    //         ".cd..",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "edcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         "..eba",
    //         "..dc.",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "edcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         "..dc.",
    //         "..eba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Inner Corner
    // // Right North East
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..bc.",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..ab.",
    //         "...c.",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL));
    // }

    // // Left North West
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         ".cb..",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".ba..",
    //         ".c...",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL));
    // }

    // // Right South East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         "..ba.",
    //         "..c..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         "...a.",
    //         "..cb.",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left South West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".ab..",
    //         "..c..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".a...",
    //         ".bc..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Collision

    // // Straight, first wall

    // // Right Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..bW.",
    //         "..c..",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         "..bW.",
    //         "..cd.",
    //         "...e.",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL));
    // }

    // // Left Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         ".Wb..",
    //         "..c..",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         ".Wb..",
    //         ".dc..",
    //         ".e...",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL));
    // }

    // // Right Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         "..c..",
    //         ".Wb..",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".e...",
    //         ".dc..",
    //         ".Wb..",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         "..c..",
    //         "..bW.",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         "...e.",
    //         "..cd.",
    //         "..bW.",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".W...",
    //         "abcde",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".Wde.",
    //         "abc..",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "abcde",
    //         ".W...",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         "abc..",
    //         ".Wde.",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "edcba",
    //         "...W.",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         "..cba",
    //         ".edW.",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         "...W.",
    //         "edcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".edW.",
    //         "..cba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Straight, second wall

    // // Right Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..b..",
    //         "..cW.",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         "..b..",
    //         "..cW.",
    //         "..de.",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL));
    // }

    // // Left Vertical North
    // {
    //     const char* input_level[] = {
    //         "..a..",
    //         "..b..",
    //         ".Wc..",
    //         "..d..",
    //         "..e..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         "..a..",
    //         "..b..",
    //         ".Wc..",
    //         ".ed..",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL));
    // }

    // // Right Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         ".Wc..",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".ed..",
    //         ".Wc..",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Vertical South
    // {
    //     const char* input_level[] = {
    //         "..e..",
    //         "..d..",
    //         "..cW.",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         "..de.",
    //         "..cW.",
    //         "..b..",
    //         "..a..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_SOUTH;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         ".dcba",
    //         ".eW..",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         ".dcba",
    //         ".eW..",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal West
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         "..W..",
    //         "edcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".eW..",
    //         ".dcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Right Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         ".....",
    //         "edcba",
    //         "..W..",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".....",
    //         ".dcba",
    //         ".eW..",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left Horizontal East
    // {
    //     const char* input_level[] = {
    //         ".....",
    //         "..W..",
    //         "edcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     const char* output_level[] = {
    //         ".....",
    //         ".eW..",
    //         ".dcba",
    //         ".....",
    //         ".....",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // // Inner Corner, Wall

    // // Right North East
    // {
    //     const char* input_level[] = {
    //         "..aW.",
    //         "..bc.",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, input_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL));
    // }

    // // Left North West
    // {
    //     const char* input_level[] = {
    //         ".Wa..",
    //         ".cb..",
    //         ".....",
    //         NULL
    //     };

    //     EXPECT(snake_constrict_test(input_level, input_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL));
    // }

    // // Right South East
    // {
    //     const char* input_level[] = {
    //         "....",
    //         "..ba",
    //         "..cW",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_EAST;
    //     EXPECT(snake_constrict_test(input_level, input_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction));
    // }

    // // Left South West
    // {
    //     const char* input_level[] = {
    //         "....",
    //         "ab..",
    //         "Wc..",
    //         NULL
    //     };

    //     Direction direction = DIRECTION_WEST;
    //     EXPECT(snake_constrict_test(input_level, input_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction));
    // }

    // New Constricting tests
    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        const char* output_level[] = {
            ".ba..",
            ".cd..",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, true));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        const char* output_level[] = {
            "..ab.",
            "..dc.",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            ".ba..",
            ".cd..",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".ba..",
            ".cd..",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, true));
    }

    {
        const char* input_level[] = {
            "..ab.",
            "..dc.",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..ab.",
            "..dc.",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            ".ba..",
            ".cd..",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            "..ab.",
            "..dc.",
            "..e..",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, true));
    }

    // New Pushing Tests
    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "..ba.",
            "..c..",
            "..d..",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".ab..",
            "..c..",
            "..d..",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_WEST));
    }

    {
        const char* input_level[] = {
            "..a..",
            ".cb..",
            ".d...",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".ab..",
            ".dc..",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_WEST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..bc.",
            "...d.",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "..ba.",
            "..cd.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, input_level, 0, DIRECTION_SOUTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 0, DIRECTION_NORTH));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        const char* output_level[] = {
            "..ab.",
            "...c.",
            "..ed.",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        const char* output_level[] = {
            ".ba..",
            ".c...",
            ".de..",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_WEST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            "..e..",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_SOUTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_NORTH));
    }

    {
        const char* input_level[] = {
            "..a..",
            ".cb..",
            ".d...",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".ba..",
            ".c...",
            ".d...",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_WEST));
        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_NORTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_SOUTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..bc.",
            "...d.",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..ab.",
            "...c.",
            "...d.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_EAST));
        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_NORTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_SOUTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_WEST));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            NULL
        };

        const char* output_level[] = {
            "..a..",
            "..b..",
            ".dc..",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 3, DIRECTION_WEST));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_NORTH));
        EXPECT(snake_segment_push_test(input_level, input_level, 1, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            "..a..",
            "..b..",
            "..c..",
            "..d..",
            NULL
        };

        const char* output_level[] = {
            "..a..",
            "..b..",
            "..cd.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 3, DIRECTION_EAST));
    }

    if (g_failed) {
        printf("unittests failed\n");
        return 1;
    }
    printf("unittests passed\n");
    return 0;
}


