#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "jack.h"
#include "murth.h"

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
  char *source = malloc(progsize + 1);
  if (progsize != fread(source, 1, progsize, fprog)) {
    fprintf(stderr, "Failed to read sources\n");
    return EINVAL;
  }
  source[progsize] = 0;
  fclose(fprog);

  murth_program_t program = murth_program_create();
  if (murth_program_compile(program, source) != 0) {
    fprintf(stderr, "Failed to compile program\n");
    return EINVAL;
  }

  int samplerate;
  jack_audio_init(&samplerate);
  murth_init(samplerate, 120);
  murth_set_program(program);

  fprintf(stderr, "Press q<Enter> to quit.\n");
  jack_audio_start();
  while (getchar() != 'q');
  jack_audio_close();

  murth_program_destroy(program);
  return 0;
}
