OBJS = *.cpp
CC = g++
CFLAGS = -O0 -g -std=c++14 -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow -Wwrite-strings
LFLAGS = -L/usr/X11R6/lib/ -I/usr/include/freetype2 -lGL -lGLU -lGLEW -lSDL2 -lSDL2_image -lassimp -lfreetype
OBJ_NAME = cge

all: $(OBJS)
	time --format="build time: %E" $(CC) $(OBJS) $(CFLAGS) $(LFLAGS) -o $(OBJ_NAME)

.PHONY: all run clean

clean: rm -f $(OBJ_NAME) *.o

