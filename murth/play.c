#include <unistd.h>
#include <stdio.h>
#include "jack.h"
#include "synth.h"

const char assembly[] = 
  "36\n"
"note:\n"
  "inote2fdeltaphase $delta storeglobal 00 idle\n"
"mainloop:\n"
  "$phase loadglobal\n"
  "$delta loadglobal\n"
  "fadd fphaserot\n"
  "$phase storeglobal\n"
  "fsin\n"
  "00 storeglobal pop\n"
  "00 idle :mainloop jmp\n"
;

int main(int argc, char *argv[]) {
  int samplerate;
  jack_audio_init(&samplerate);
  murth_init(assembly, 44100, 90);
  murth_set_midi_note_handler("note", -1, -1);
  fprintf(stderr, "Press q<Enter> to quit.\n");
  jack_audio_start();
  while (getchar() != 'q');
  jack_audio_close();
  return 0;
}
