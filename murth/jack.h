#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void jack_audio_init(int *samplerate);
void jack_audio_start();
void jack_audio_close();

#ifdef __cplusplus
{ // extern "C"
#endif
