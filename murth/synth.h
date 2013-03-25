#pragma once

#define MAX_PARAMS 128

#ifdef __cplusplus
extern "C" {
#endif

extern int murth_assemble(const char *ptr);
//! must be called from teh same thread as synthesize
extern void murth_process_raw_midi(void *packet, int bytes);
extern void murth_synthesize(float* ptr, int count);


typedef unsigned char u8;

typedef struct {
  union {
    int i;
    float f;
  };
} ifu;

typedef struct {
  int count;
  u8 *values;
  unsigned short *dsamples;
} paramtbl;

  
extern paramtbl paramtbls[];
extern int nparamtbls;


extern ifu params[MAX_PARAMS];

typedef struct {
  int pos;
  int size;
  ifu *values;
} probe;

extern probe probes[];

#ifdef __cplusplus
}
#endif
