#!/bin/bash

# Exit early if any commands fail
set -e
set -x

# Compile program
clang -std=c11 *.c -Wall -Wextra -Werror -lSDL2 -g -o taco-quest
