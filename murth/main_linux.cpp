#include <unistd.h>
#include <stdio.h>
#include <kapusha/sys/sdl/KPSDL.h>
#include "jack.h"
#include "synth.h"

extern kapusha::IViewport *makeViewport();
int main(int argc, char *argv[]) {
  for (int i = 0; i < MAX_PARAMS; ++i) params[i].f = .5f; 
  int samplerate;
  jack_audio_init(&samplerate);
  int ret = KPSDL(makeViewport(), 1280, 720);
  jack_audio_close();
  return ret;
}
