#pragma once
#ifdef __cplusplus
extern "C" {
#endif
  void osx_audio_init(int *samplerate);
  void osx_audio_start();
  void osx_audio_close();
#ifdef __cplusplus
} // extern "C"
#endif
