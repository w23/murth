#pragma once
#include "vm.h"
#include "program.h"
#include "instructions.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Init murth to a clean state, with no program and no active cores.
//! \param samplerate Intended samplerate.
void murth_init(int samplerate);

//! \brief Set new BPM.
//! Sets internal tick value to a new one according to new BPM.
//! If murth is running, sends an asynchronous message, so the change actually
//! happens in the beginning of a closest murth_synthesize() call
//! \param bpm New BPM value.
void murth_set_bpm(int bpm);

//! (message) set program and reset to one core running from the beginning
void murth_set_program(murth_program_t *prog);

//! (message) set max number of instructions to run per sample
//! this is in order to avoid overload and forever loops
void murth_set_instruction_limit(int max_per_sample);

//! generate next samples block
//! does nothing if the program is not set
//! \returns samplestamp of the last written sample
int murth_synthesize(float* ptr, int count);

//! get next generated event
//! \param event pointer to event structure to be filled
//! \returns number of remaining events available (plus this one)
//! \attention this function is thread safe assuming you call it from just one thread simultaneously
int murth_get_event(murth_event_t *event);

//! process raw midi stream
//! \attention this function is assumed to be called not more from than one thread
//! simultaneously, but the thread is not required to be the same as for synthesizing
//! \attention midi stream data is assumed to be valid
void murth_process_raw_midi(const void *packet, int bytes);

//! \todo Debug connection to read technical events, errors and warnings.
//! \todo Tap connection to get raw samples from user-specified points.

#ifdef __cplusplus
}
#endif
