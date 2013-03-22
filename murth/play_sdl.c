#include <unistd.h>
#include <SDL/SDL.h>
#include "synth.h"

void SDLCALL sdl_play(void *userdata, Uint8 *stream, int len) {
  murth_synthesize(stream, len >> 1);
}
int main(int argc, char *argv[]) {
  for (int i = 0; i < MAX_PARAMS; ++i) params[i].f = .5f; 
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec spec = {44100, AUDIO_S16LSB, 1, 0, 4096, 0, 0, sdl_play, 0};
  SDL_OpenAudio(&spec, NULL);
  SDL_PauseAudio(0);
  for(;;) sleep(1000);
  SDL_Quit();
  return 0;
}
