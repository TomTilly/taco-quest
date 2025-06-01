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

bool game_from_string(const char* string, S32 width, S32 height, Game* game) {
    // Build the level.
    if (!game_init(game, width, height, 0)) {
        return false;
    }
    S32 snake_capacities[MAX_SNAKE_COUNT] = {0};
    for (S32 y = 0; y < height; y++) {
        for (S32 x = 0; x < width; x++) {
            char ch = char_from_level_string(string, width, x, y);
            level_set_cell(&game->level, x, y, CELL_TYPE_EMPTY);

            if (ch == 'W') {
                level_set_cell(&game->level, x, y, CELL_TYPE_WALL);
            } else if (ch == 'T') {
                level_set_cell(&game->level, x, y, CELL_TYPE_TACO);
            } else if (isdigit(ch)) {
                snake_capacities[0]++;
            } else if (islower(ch)) {
                snake_capacities[1]++;
            } else if (isupper(ch)) {
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
            char ch = char_from_level_string(string, width, x, y);
            S32 snake_index = 0;
            S32 segment_index = 0;

            if (isdigit(ch)) {
                snake_index = 0;
                segment_index = ch - '0';
            } else if (islower(ch)) {
                snake_index = 1;
                segment_index = ch - 'a';
            } else if (isupper(ch)) {
                snake_index = 2;
                segment_index = ch - 'A';
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

    char base_chars[MAX_SNAKE_COUNT] = {'0', 'a', 'A'};
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

int main(int argc, char** argv) {
    (void)(argc);
    (void)(argv);

    // Test case
    {
        const char* input_level =
            "......."
            "..0abc."
            "..1...."
            "..2...."
            ".......";

        const char* output_level =
            "..0...."
            "..1abc."
            "..2...."
            "......."
            ".......";

        Game input_game = {0};
        game_from_string(input_level, 7, 5, &input_game);

        SnakeAction snake_actions[MAX_SNAKE_COUNT] = {0};
        snake_actions[0] = SNAKE_ACTION_FACE_NORTH;
        snake_actions[1] = SNAKE_ACTION_FACE_WEST;

        game_update(&input_game, snake_actions);

        Game output_game = {0};
        game_from_string(output_level, 7, 5, &output_game);
        output_game.snakes[1].direction = DIRECTION_WEST;

        EXPECT(games_are_equal(&input_game, &output_game));
    }

    if (g_failed) {
        printf("unittests failed\n");
        return 1;
    }
    printf("unittests passed\n");
    return 0;
}
