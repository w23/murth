#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef void *murth_program_t;

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

extern murth_program_t murth_program_create();
extern int murth_program_compile(murth_program_t program,
                                 const char *source);
extern const char *murth_program_get_status(murth_program_t program);
extern void murth_program_destroy(murth_program_t program);

extern void murth_init(int samplerate, int bpm);
extern void murth_set_program(murth_program_t prog);
extern void murth_set_instruction_limit(int max_per_sample);
extern void murth_synthesize(float* ptr, int count);
extern int murth_get_event(murth_event_t *event);

// broken atm
#if 0
//! must be called after init
//! -1 means for everything
//! prioritized -- first matched gets to handle
//! stack: (top)value, control, channel
extern int murth_set_midi_control_handler(const char *label, int channel, int control, int value);
//! stack: (top)note, velocity, channel
extern int murth_set_midi_note_handler(const char *label, int channel, int note);
#endif

//! must be called from teh same thread as synthesize
//! midi stream is assumed to be valid
extern void murth_process_raw_midi(const void *packet, int bytes);

#ifdef __cplusplus
}
#endif
