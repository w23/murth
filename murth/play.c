#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "jack.h"
#include "synth.h"

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
  char *program = malloc(progsize + 1);
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
  fprintf(stderr, "Press q<Enter> to quit.\n");
  jack_audio_start();
  while (getchar() != 'q');
  jack_audio_close();
  return 0;
}
