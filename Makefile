CC = g++
CFLAGS = -O0 -g -std=c++14 -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow -Wwrite-strings
CGE_LFLAGS = -I/usr/include/freetype2 -lX11 -lGL -lfreetype
AP_LFLAGS = -I/usr/include/freetype2 -lX11 -lGL -lassimp -lfreetype -lSDL2_image -lSDL2

all: linux_cge.cpp
	time --format="build time: %E" $(CC) linux_cge.cpp $(CFLAGS) $(CGE_LFLAGS) -o cge

assets: asset_packer.cpp
	time --format="build time: %E" $(CC) asset_packer.cpp $(CFLAGS) $(AP_LFLAGS) -o asset_packer
	time --format="asset pack time: %E" ./asset_packer

.PHONY: all assets
