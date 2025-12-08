#!/bin/bash
set -e

CXX=clang++
CC=clang

SRC_DIR=src
EXT_DIR=dep

INCLUDES="-I${EXT_DIR}/glad/include -I${EXT_DIR}/glfw/include -I${EXT_DIR}/stb/"
LIBS="-lglfw -ldl -lGL -lm -lpthread -lX11 -lXrandr -lXi -lXxf86vm -lXcursor"

echo "Compiling GLAD (C)..."
$CC -std=c11 \
    $INCLUDES \
    -c ${EXT_DIR}/glad/src/glad.c \
    -o glad.o

echo "Compiling project (C++)..."
$CXX -std=c++20 \
    $INCLUDES \
    $SRC_DIR/main.cpp \
    dep/stb/stb_image.cpp \
    glad.o \
    $LIBS \
    -o gungale