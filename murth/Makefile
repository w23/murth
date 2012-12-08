CC=cc
LD=ld
CFLAGS=-g -Wall -mfpmath=387 -std=c99 `pkg-config --cflags sdl`
LDFLAGS=-lm `pkg-config --libs sdl`

play_sdl: play_sdl.o synth.o Makefile
	$(CC) $(LDFLAGS) play_sdl.o synth.o -o play_sdl 

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $<
