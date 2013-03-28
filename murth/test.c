#include <stdio.h>
#include "synth.h"

const char *assembly =
"54 inote2fdeltaphase $delta storeglobal pop\n" // initial delta_phase
"00 :phasemod spawn\n" // spawn phasemod "thread"
"forever:\n" // main synthloop
  "$phase loadglobal $delta loadglobal fadd fphaserot $phase storeglobal fsin noise 0.5 fmul fadd 00 storeglobal pop\n"
  "00 idle :forever jmp\n"
"phasemod:\n" // second "thread"
  "$delta loadglobal 0.997 fmul $delta storeglobal pop\n"
  "04 idle :phasemod jmp\n"
"note:\n" // note handler
  "inote2fdeltaphase $delta storeglobal ret"
;

#define SAMPLES 8192
float samples[SAMPLES];

unsigned char midi[3] = { 0x90, 27, 127 };

int main(int argc, char *argv[])
{
  murth_init(assembly, 44100, 90);
  murth_set_midi_note_handler("note", -1, -1);
  murth_synthesize(samples, SAMPLES/2);
  murth_process_raw_midi(midi, 3);
  murth_synthesize(samples + SAMPLES/2, SAMPLES/2);

  FILE *f = fopen("synth.data", "w");
  for (int i = 0; i < SAMPLES; ++i)
    fprintf(f, "%f\n", samples[i]);
  fclose(f);

  return 0;
}
