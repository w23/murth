#pragma once
#ifdef __cplusplus
extern "C" {
#endif

//! murth opaque program type
typedef void *murth_program_t;

//! unified float + int value
typedef struct {
  union {
    int i;
    float f;
  };
} var_t;

//! event event
typedef struct {
  //! sample number that this event was generated on
  int samplestamp;
  //! event type
  int type;
  //! event values
  var_t value[2];
} murth_event_t;

//! \attention murth_* functions are not thread-safe, except where stated otherwise

//! creates an unitialized program
extern murth_program_t murth_program_create();

//! resets program and initializes it by compiling the source
//! \returns 0 on success, any other value is a number of a line where the
//! first spotted error occured (beginning with 1, not 0, obviously)
extern int murth_program_compile(murth_program_t program, const char *source);

//! get an extended error description. no more chars than buffer_size are written
extern void murth_program_get_error(murth_program_t program,
                                    char *buffer, int buffer_size);

//! get a list of custom keywords/instructions/functions that were declared
//! in this program one by one.
//! \returns string name of a custom instruction, or 0 for index > count_keywords
extern const char *murth_program_get_custom_keyword(murth_program_t program, int index);
//! \todo: labels, label references, any references

//! destroy a program that is not needed anymore
extern void murth_program_destroy(murth_program_t program);


//! (static) get a list of compiled-in instructions
extern const char *murth_get_keyword(murth_program_t program, int index);


//! init murth to a clean state, with no program and no active cores
//! \param samplerate intended samplerate
extern void murth_init(int samplerate);

//! (message) set BPM
extern void murth_set_bpm(int bpm);

//! (message) set program and reset to one core running from the beginning
extern void murth_set_program(murth_program_t prog);

//! (message) set max number of instructions to run per sample
//! this is in order to avoid overload and forever loops
extern void murth_set_instruction_limit(int max_per_sample);

//! generate next samples block
//! does nothing if the program is not set
//! \returns samplestamp of the last written sample
extern int murth_synthesize(float* ptr, int count);

//! get next generated event
//! \param event pointer to event structure to be filled
//! \returns number of remaining events available (plus this one)
//! \attention this function is thread safe assuming you call it from just one thread simultaneously
extern int murth_get_event(murth_event_t *event);

//! process raw midi stream
//! \attention this function is assumed to be called not more from than one thread
//! simultaneously, but the thread is not required to be the same as for synthesizing
//! \attention midi stream data is assumed to be valid
extern void murth_process_raw_midi(const void *packet, int bytes);

#ifdef __cplusplus
}
#endif
