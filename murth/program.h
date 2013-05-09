#pragma once
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

//! some entity name
typedef struct {
  char name[MAX_NAME_LENGTH];
} murth_name_t;

//! label that we can jump to
typedef struct {
  //! name
  char name[MAX_NAME_LENGTH];
  //! bytecode address
  int position;
} murth_label_t;

typedef struct {
  //! global variables
  murth_name_t global_names[GLOBALS-1];
  //! samplers
  murth_name_t sampler_names[SAMPLERS];
  //! label declarations
  murth_label_t label_names[GLOBALS];
  //! labels references before declaration
  //! \todo merge with ordinary labels
  murth_label_t label_forwards[MAX_FORWARDS];
  //! compiled program
  murth_op_t program[MAX_PROGRAM_LENGTH];
  //! program length
  int length;
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

//! resets program and initializes it by compiling the source
//! \param program program to initialize and compile to
//! \param source source assembly code
//! \returns 0 on success,
//! \returns any other value is an error code and program->error contains descritpion
//! \todo define error codes
int murth_program_compile(murth_program_t *program, const char *source);

#ifdef __cplusplus
}
#endif

