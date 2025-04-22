#!/bin/bash

# Compile main.c to main.o
gcc -c main_gbm.c -o main_gbm.o -I/usr/include/libdrm

# Compile cube_render.cpp to cube_render.o
g++ -c cube_render.cpp -o cube_render.o -I.

# Link object files to create the executable
g++ cube_render.o main_gbm.o -o gbm_cube_demo -lGLESv2 -lEGL -ldrm -lm -lgbm

# Check if the compilation and linking were successful
if [ $? -eq 0 ]; then
    rm main_gbm.o cube_render.o
    echo "Compilation and linking successful!"
else
    echo "Compilation or linking failed."
fi

