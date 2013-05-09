#pragma once
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Some entity name.
typedef struct {
  char name[MAX_NAME_LENGTH];
} murth_name_t;

//! Label that can be jumped to.
typedef struct {
  char name[MAX_NAME_LENGTH]; //!< Label name
  int position; //!< Bytecode address, in basic ops
} murth_label_t;

//! Program description.
//! \attention Not intended for external manipulation! Read-only!
//! \attention The internals are made public only for live-editing aid.
typedef struct {
  murth_name_t global_names[GLOBALS-1]; //!< Array of allocated global variables. Index equals to actual global slot.
  murth_name_t sampler_names[SAMPLERS]; //!< Array of allocated samplers. Index equals to actual sampler unit.
  murth_label_t label_names[GLOBALS]; //!< Array of declared labels.
  murth_label_t label_forwards[MAX_FORWARDS]; //!< Array of accessed, but not yet declared labels. \todo Get rid of this kludge.
  murth_op_t program[MAX_PROGRAM_LENGTH]; //!< Compiled program "bytecode"
  int length; //!< Count of operations in compiled bytecode
  //! address of a midi control handler routine
  //! determined automatically: set to label named "midi_cc:"
  int midi_cc_handler;
  //! address of a midi note handler routine
  //! determined automatically: set to label named "midi_note:"
  int midi_note_handler;

  //! error description
  struct {
    //! human-readable text of why
    char desc[MAX_ERROR_DESC_LENGTH];
    //! pointers to the origina source place that caused error
    const char *source_begin, *source_end;
  } error;
} murth_program_t;

//! Resets program and initializes it by compiling the source.
//! \param program Program object to initialize and compile to.
//! \param source Source assembly code.
//! \returns 0 on success,
//! \returns Any other value is an error code and program->error contains descritpion
//! \todo Define program compilation error codes.
int murth_program_compile(murth_program_t *program, const char *source);

#ifdef __cplusplus
}
#endif

