#pragma once
#ifdef __cplusplus
extern "C" {
#endif

//! must be called before synthesizing
extern int murth_init(const char *assembly, int samplerate, int bpm);

//! must be called after init
//! -1 means for everything
//! prioritized -- first matched gets to handle
//! stack: (top)value, control, channel
extern int murth_set_midi_control_handler(const char *label, int channel, int control, int value);
//! stack: (top)note, velocity, channel
extern int murth_set_midi_note_handler(const char *label, int channel, int note);

//! must be called from teh same thread as synthesize
//! midi stream is assumed to be valid
extern void murth_process_raw_midi(const void *packet, int bytes);
extern void murth_synthesize(float* ptr, int count);

//!
typedef struct {
  union {
    int i;
    float f;
  };
} var_t;
typedef struct {
  int samplestamp;
  int event;
  var_t value;
} murth_event_t;
extern int murth_get_event(murth_event_t *event);

#ifdef __cplusplus
}
#endif
