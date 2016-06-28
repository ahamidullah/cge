OBJS = *.cpp
CC = g++
CFLAGS = -O0 -g -std=c++14 -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow -Wwrite-strings
LFLAGS = -L/usr/X11R6/lib/ -lGL -lGLU -lGLEW -lSDL2 -lSDL2_image
OBJ_NAME = cge

run : all
	./cge

all: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LFLAGS) -o $(OBJ_NAME)

clean: rm -f $(OBJ_NAME) *.o
