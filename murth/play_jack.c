#include <unistd.h>
#include <stdio.h>
#include "jack.h"
#include "synth.h"

int main(int argc, char *argv[]) {
  for (int i = 0; i < MAX_PARAMS; ++i) params[i].f = .5f; 
  int samplerate;
  jack_audio_init(&samplerate);
  fprintf(stderr, "Press q<Enter> to quit.\n");
  jack_audio_start();
  while (getchar() != 'q');
  jack_audio_close();
  return 0;
}
