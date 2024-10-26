#!/bin/bash

# Exit early if any commands fail
set -e
set -x

# Compile program
clang -std=c11 main.c -Wall -Wextra -Werror -I /usr/local/Cellar/sdl2/2.0.16/include -L /usr/local/Cellar/sdl2/2.0.16/lib -l SDL2 -g -o taco-quest

# Run program
./taco-quest
