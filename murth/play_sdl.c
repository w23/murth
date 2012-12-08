#include <unistd.h>
#include <SDL/SDL.h>

void synth(void*, int);

void SDLCALL sdl_play(void *userdata, Uint8 *stream, int len)
{
	synth(stream, len >> 1);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec spec = {44100, AUDIO_S16LSB, 1, 0, 4096, 0, 0, sdl_play, 0};
	SDL_OpenAudio(&spec, 0);
	SDL_PauseAudio(0);
	for(;;) sleep(1);
	SDL_Quit();
	return 0;
}
