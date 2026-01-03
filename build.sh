#!/bin/bash
set -e

CC=clang

SRC_DIR=src
EXT_DIR=dep

INCLUDES="-I${EXT_DIR}/glad/include -I${EXT_DIR}/glfw/include -I${EXT_DIR}/stb/ -I${EXT_DIR}/"
LIBS="-lraylib -lpthread -ldl -lm"
$CC -std=c11 \
  main.c\
  $INCLUDES\
  $LIBS\
  -o gungale

echo "Compiling project (C++)..."
