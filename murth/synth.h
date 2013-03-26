#pragma once
#ifdef __cplusplus
extern "C" {
#endif

extern int murth_init(const char *assembly, int samplerate, int bpm);

//! must be called from teh same thread as synthesize
extern void murth_process_raw_midi(void *packet, int bytes);
extern void murth_synthesize(float* ptr, int count);

#ifdef __cplusplus
}
#endif
