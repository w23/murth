#include <unistd.h>
#include <stdio.h>
#include "jack.h"
#include "synth.h"

const char assembly[] = 
"ret\n"
"midinote:\n"
  "inote2fdeltaphase 00 02 :noteplay spawn ret\n"
"noteplay:\n"
" faddnp fphaserot"
" dup"
" fsin 0.5 fmul"
" 00 loadglobal fadd"
" 00 storeglobal pop"
" 00 idle :noteplay jmp"
;

int main(int argc, char *argv[]) {
  int samplerate;
  jack_audio_init(&samplerate);
  murth_init(assembly, 44100, 90);
  murth_set_midi_note_handler("midinote", -1, -1);
  fprintf(stderr, "Press q<Enter> to quit.\n");
  jack_audio_start();
  while (getchar() != 'q');
  jack_audio_close();
  return 0;
}
