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

bool snake_segment_push_by_snake_test(const char** input_level,
                                      const char** output_level,
                                      S32 segment_index,
                                      S32 push_by_snake_index,
                                      Direction direction) {
    Game input_game = {0};
    game_from_string(input_level, &input_game);

    PushState push_state = {0};
    init_push_state(&input_game, &push_state);
    push_state.original_snake_index = push_by_snake_index;

    snake_segment_push(&input_game, &push_state, 0, segment_index, direction);

    free(push_state.cells);

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
    return snake_segment_push_by_snake_test(input_level, output_level, segment_index, 0, direction);
}

bool snake_constrict_test(const char** input_level,
                          const char** output_level,
                          S32 snake_index,
                          SnakeConstrictState snake_constrict_state) {
    Game input_game = {0};
    game_from_string(input_level, &input_game);

    snake_constrict(&input_game, snake_index, snake_constrict_state);

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

#if 0
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
            "..a..",
            ".cb..",
            ".d...",
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
            "..a..",
            "..bc.",
            "...d.",
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

    {
        const char* input_level[] = {
            "..A..",
            "..Ba.",
            "..Cb.",
            "..Dc.",
            "..Ed.",
            "...e.",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".BA..",
            ".Cba.",
            ".Dcd.",
            ".E.e.",
            ".....",
            ".....",
            ".....",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, true));
    }

    {
        const char* input_level[] = {
            "...edaW",
            "....cbW",
            "......W",
            "......W",
            NULL
        };

        const char* output_level[] = {
            "...e.aW",
            "...dcbW",
            "......W",
            "......W",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 2, false));
    }

    {
        const char* input_level[] = {
            "WgfedaW",
            "WABCcbW",
            "...DE.W",
            "WWWWWWW",
            NULL
        };

        const char* output_level[] = {
            "Wgfe.aW",
            "WABdcbW",
            "..CDE.W",
            "WWWWWWW",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 2, false));
    }

    {
        const char* input_level[] = {
            "...W...",
            ".aBA...",
            ".bC....",
            ".cD....",
            ".d.....",
            NULL
        };

        const char* output_level[] = {
            "...W...",
            ".abA...",
            ".dcB...",
            "..DC...",
            ".......",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            ".......",
            "..ab...",
            "...c...",
            "...d...",
            ".......",
            NULL
        };

        const char* output_level[] = {
            "...a...",
            "...b...",
            "...c...",
            "...d...",
            ".......",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_NORTH));
    }

    {
        const char* input_level[] = {
            ".......",
            "...aA..",
            "..cbB..",
            "..dWC..",
            "..eWD..",
            NULL
        };

        const char* output_level[] = {
            ".....A.",
            "...abB.",
            "..edcC.",
            "...W.D.",
            "...W...",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            ".edcbaW",
            "..GFABW",
            "..HEDCW",
            "..I...W",
            "..JWWWW",
            NULL
        };

        const char* output_level[] = {
            "...edaW",
            ".HGFcbW",
            ".IJE.AW",
            "...DCBW",
            "...WWWW",
            NULL
        };

        EXPECT(snake_segment_constrict_test(input_level, output_level, 0, false));
    }

    {
        const char* input_level[] = {
            "......W",
            "..gfabW",
            "..hedcW",
            "..i...W",
            "..jWWWW",
            NULL
        };

        const char* output_level[] = {
            "......W",
            "..gf..W",
            "..hedaW",
            "..i.cbW",
            "..jWWWW",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            "......W",
            "..gfabW",
            "..hedcW",
            "..i...W",
            "..jWWWW",
            NULL
        };

        const char* output_level[] = {
            "......W",
            "....abW",
            "..hgdcW",
            "..ife.W",
            "..jWWWW",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 5, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            ".......",
            ".a..hij",
            ".b..g..",
            ".cdef..",
            ".......",
            NULL
        };

        Game input_game = {0};
        game_from_string(input_level, &input_game);

        EXPECT(snake_segment_is_pushable(&input_game, 0, 0, DIRECTION_EAST));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 0, DIRECTION_WEST));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 1, DIRECTION_EAST));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 1, DIRECTION_WEST));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 2, DIRECTION_EAST));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 2, DIRECTION_NORTH));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 3, DIRECTION_NORTH));
        EXPECT(!snake_segment_is_pushable(&input_game, 0, 3, DIRECTION_SOUTH));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 4, DIRECTION_NORTH));
        EXPECT(!snake_segment_is_pushable(&input_game, 0, 4, DIRECTION_SOUTH));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 5, DIRECTION_NORTH));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 5, DIRECTION_WEST));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 6, DIRECTION_EAST));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 6, DIRECTION_WEST));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 7, DIRECTION_WEST));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 7, DIRECTION_SOUTH));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 8, DIRECTION_SOUTH));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 8, DIRECTION_NORTH));

        EXPECT(snake_segment_is_pushable(&input_game, 0, 9, DIRECTION_SOUTH));
        EXPECT(snake_segment_is_pushable(&input_game, 0, 9, DIRECTION_NORTH));
    }

    {
        const char* input_level[] = {
            "WWWWWWW",
            "WABCD..",
            "W.fgE..",
            "W.ehFG.",
            "WcdijH.",
            "WbmlkI.",
            "WanopJ.",
            "WWWWWK.",
            NULL
        };

        const char* output_level[] = {
            "WWWWWWW",
            "WABC...",
            "WTTDE..",
            "WTTTFG.",
            "WT.TTH.",
            "WTTTTI.",
            "WTTTTJ.",
            "WWWWWK.",
            NULL
        };

        EXPECT(snake_constrict_test(input_level, output_level, 1, true));
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
            "..a..",
            "..b..",
            "..cd.",
            "...e.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 4, DIRECTION_NORTH));
    }

    {
        const char* input_level[] = {
            "..aW.",
            "..bW.",
            "..cW.",
            "..dW.",
            "..e..",
            NULL
        };

        const char* output_level[] = {
            "..aW.",
            "..bW.",
            ".dcW.",
            ".e.W.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 4, DIRECTION_NORTH));
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
            ".....",
            ".a...",
            ".bc..",
            "..d..",
            "..e..",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            "..aW.",
            "..bW.",
            "..cW.",
            "..dW.",
            "..eW.",
            NULL
        };

        const char* output_level[] = {
            "...W.",
            ".a.W.",
            ".bcW.",
            "..dW.",
            "..eW.",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 0, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            ".....",
            "..ba.",
            "..cW.",
            "..dW.",
            "..eW.",
            NULL
        };

        const char* output_level[] = {
            ".....",
            ".....",
            ".baW.",
            ".cdW.",
            "..eW.",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 1, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            ".....",
            "..cba",
            "..dWW",
            "..eW.",
            "...W.",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "..cba",
            "..dWW",
            "..eW.",
            "...W.",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 2, DIRECTION_SOUTH));
    }

    {
        const char* input_level[] = {
            ".ba..",
            ".cd..",
            ".fe..",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "..a..",
            "..bc.",
            ".fed.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 2, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            ".....",
            "...fg",
            ".cdeh",
            ".ba..",
            ".....",
            NULL
        };

        const char* output_level[] = {
            "...ef",
            "...dg",
            "..bch",
            "..a..",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 2, DIRECTION_EAST));
    }

    {
        const char* input_level[] = {
            ".....",
            "a..j.",
            "befi.",
            "cdgh.",
            ".....",
            NULL
        };

        const char* output_level[] = {
            ".....",
            "adej.",
            "bcfi.",
            "..gh.",
            ".....",
            NULL
        };

        EXPECT(snake_segment_push_test(input_level, output_level, 3, DIRECTION_NORTH));
    }
#endif

    {
        const char* input_level[] = {
            "WWW..",
            "WA012",
            "WBa..",
            "WCb..",
            "Wdc..",
            NULL
        };

        const char* output_level[] = {
            "WWW..",
            "WT012",
            "WTa..",
            "WTb..",
            "Wdc..",
            NULL
        };

        EXPECT(snake_constrict_test(input_level, output_level, 0, true));
    }

    if (g_failed) {
        printf("unittests failed\n");
        return 1;
    }
    printf("unittests passed\n");
    return 0;
}
