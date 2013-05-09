#pragma once
#include "limits.h"

#ifdef __cplusplus
extern "C" {
#endif

//! unified 32-bit float/int value
typedef struct {
  union {
    int i;
    float f;
  };
} murth_var_t;

//! event event
typedef struct {
  //! sample number that this event was generated on
  int samplestamp;
  //! event type
  int type;
  //! event values
  murth_var_t value[2];
} murth_event_t;

typedef struct {
  int samples_to_idle;
  int ttl;
  int pcounter;
  murth_var_t stack[STACK_SIZE], *top;
} murth_core_t;

//! instruction function
//! \param core the core that this instruction must be ran on
typedef void(*murth_instr_func_t)(murth_core_t *core);

typedef struct {
  union {
    murth_instr_func_t func;
    int param;
  };
} murth_op_t;

//! \todo should this be visible?
typedef struct {
  int cursor;
  float samples[MAX_SAMPLER_SIZE];
} murth_sampler_t;

#ifdef __cplusplus
}
#endif

