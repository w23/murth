CC=cc
LD=ld
#CFLAGS=-O0 -g -Wall -mfpmath=387 -std=c99 `pkg-config --cflags sdl`
CFLAGS=-O3 -g -Wall -std=c99 `pkg-config --cflags jack`
LDFLAGS=-lm `pkg-config --libs jack`

play_jack: play_jack.o jack.o synth.o program.o Makefile
	$(CC) $(LDFLAGS) play_jack.o jack.o synth.o program.o -o play_jack

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $<
