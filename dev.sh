#!/bin/bash

# Exit early if any commands fail
set -e
set -x

# Compile program
clang -std=c11 main.c -I /usr/local/Cellar/sdl2/2.0.16/include -L /usr/local/Cellar/sdl2/2.0.16/lib -l SDL2 -o taco-quest

# Run program
./taco-quest