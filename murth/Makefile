CC=cc
LD=ld
#CFLAGS=-O0 -g -Wall -mfpmath=387 -std=c99 `pkg-config --cflags sdl`
CFLAGS=-O3 -g -Wall -std=c99 `pkg-config --cflags sdl`
LDFLAGS=-lm `pkg-config --libs sdl`

play_sdl: play_sdl.o synth.o program.o Makefile
	$(CC) $(LDFLAGS) play_sdl.o synth.o program.o -o play_sdl 

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $<
