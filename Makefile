OBJS = *.cpp
CC = g++
CFLAGS = -O0 -g -std=c++14 -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow -Wwrite-strings
LFLAGS = -L/usr/X11R6/lib/ -lGL -lGLU -lGLEW -lglut
OBJ_NAME = cge

all: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LFLAGS) -o $(OBJ_NAME)

clean: rm -f $(OBJ_NAME) *.o
