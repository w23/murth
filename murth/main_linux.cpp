#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#if KAPUSHA_RPI
#include <kapusha/sys/rpi/RPi.h>
#else
#include <kapusha/sys/sdl/KPSDL.h>
#endif
#include "jack.h"
#include "murth.h"

extern kapusha::IViewport *makeViewport();
int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s program.mur\n", argv[0]);
    return EINVAL;
  }

  FILE *fprog = fopen(argv[1], "r");
  if (fprog == NULL) {
    fprintf(stderr, "Cannot open file '%s'\n", argv[1]);
    return EINVAL;
  }
  fseek(fprog, 0, SEEK_END);
  long progsize = ftell(fprog);
  fseek(fprog, 0, SEEK_SET);
  char *program = new char[progsize + 1];
  fread(program, 1, progsize, fprog);
  program[progsize] = 0;
  fclose(fprog);

  int samplerate;
  jack_audio_init(&samplerate);
  if (murth_init(program, samplerate, 90) != 0) {
    fprintf(stderr, "Failed to compile program\n");
    jack_audio_close();
    return EINVAL;
  }
  murth_set_midi_note_handler("midinote", -1, -1);
  jack_audio_start();
#if KAPUSHA_RPI
  int ret = RunRaspberryRun(makeViewport());
#else
  int ret = KPSDL(makeViewport(), 1280, 720);
#endif
  jack_audio_close();
  return ret;
}
