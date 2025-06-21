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

void snake_constrict_test(const char** input_level,
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

    EXPECT(games_are_equal(&input_game, &output_game));
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

    // Right - Vertical Straight North
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
            "..a..",
            "..bc.",
            "..ed.",
            ".....",
            ".....",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL);
    }

    // Left - Vertical Straight North
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
            "..a..",
            ".cb..",
            ".de..",
            ".....",
            ".....",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL);
    }

    // Right - Vertical Straight South
    {
        const char* input_level[] = {
            "..e..",
            "..d..",
            "..c..",
            "..b..",
            "..a..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".....",
            ".de..",
            ".cb..",
            "..a..",
            NULL
        };

        Direction direction = DIRECTION_SOUTH;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction);
    }

    // Left - Vertical Straight South
    {
        const char* input_level[] = {
            "..e..",
            "..d..",
            "..c..",
            "..b..",
            "..a..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".....",
            "..ed.",
            "..bc.",
            "..a..",
            NULL
        };

        Direction direction = DIRECTION_SOUTH;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    // Right - Horizontal Straight West
    {
        const char* input_level[] = {
            ".....",
            ".....",
            "abcde",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".cd..",
            "abe..",
            ".....",
            ".....",
            NULL
        };

        Direction direction = DIRECTION_WEST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction);
    }

    // Left - Horizontal Straight West
    {
        const char* input_level[] = {
            ".....",
            ".....",
            "abcde",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".....",
            "abe..",
            ".cd..",
            ".....",
            NULL
        };

        Direction direction = DIRECTION_WEST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    // Right - Horizontal Straight East
    {
        const char* input_level[] = {
            ".....",
            ".....",
            "edcba",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".....",
            "..eba",
            "..dc.",
            ".....",
            NULL
        };

        Direction direction = DIRECTION_EAST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction);
    }

    // Left - Horizontal Straight East
    {
        const char* input_level[] = {
            ".....",
            ".....",
            "edcba",
            ".....",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "..dc.",
            "..eba",
            ".....",
            ".....",
            NULL
        };

        Direction direction = DIRECTION_EAST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    // Right - Inner Corner North East
    {
        const char* input_level[] = {
            "..a..",
            "..bc.",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..ab.",
            "...c.",
            ".....",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL);
    }

    // Left - Inner Corner North West
    {
        const char* input_level[] = {
            "..a..",
            ".cb..",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".ba..",
            ".c...",
            ".....",
            NULL
        };

        Direction direction = DIRECTION_EAST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    // Right - Inner Corner South East
    {
        const char* input_level[] = {
            ".....",
            "..ba.",
            "..c..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "...a.",
            "..cb.",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL);
    }

    // Left - Inner Corner South West
    {
        const char* input_level[] = {
            ".....",
            ".ab..",
            "..c..",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".a...",
            ".bc..",
            NULL
        };

        Direction direction = DIRECTION_WEST;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    // Left - Outer Corner North East
    {
        const char* input_level[] = {
            ".a.......",
            ".bcdefgh.",
            ".........",
            NULL
        };

        const char* output_level[] = {
            "ba.......",
            "c.gh.....",
            "def......",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, NULL);
    }

    // Right - Outer Corner North West
    {
        const char* input_level[] = {
            ".......a.",
            ".hgfedcb.",
            ".........",
            NULL
        };

        const char* output_level[] = {
            ".......ab",
            ".....hg.c",
            "......fed",
            NULL
        };

        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, NULL);
    }

    // Right - Outer Corner South East
    {
        const char* input_level[] = {
            ".........",
            "..bcdefgh",
            "..a......",
            NULL
        };

        const char* output_level[] = {
            ".def.....",
            ".c.gh....",
            ".ba......",
            NULL
        };

        Direction direction = DIRECTION_SOUTH;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_RIGHT, &direction);
    }

    // Left - Outer Corner South West
    {
        const char* input_level[] = {
            ".........",
            "hgfedcb..",
            "......a..",
            NULL
        };

        const char* output_level[] = {
            ".....fed.",
            "....hg.c.",
            "......ab.",
            NULL
        };

        Direction direction = DIRECTION_SOUTH;
        snake_constrict_test(input_level, output_level, SNAKE_ACTION_CONSTRICT_LEFT, &direction);
    }

    if (g_failed) {
        printf("unittests failed\n");
        return 1;
    }
    printf("unittests passed\n");
    return 0;
}
