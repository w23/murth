#pragma once

extern void synth(short* ptr, int count);

enum {
  LDG = 128, STG, LDP, ADD, PHR, SIN, CLAMP, FIU, NDP,
  IFU, MUL, IADD, DUP, NDUP, SGN,
  RWR, RRD, ROF, POP, IMUL,
  LOND = 254, RET = 255
};

#define LOAD_SHORT(v) LOND, (((int)(v))>>8), (((int)(v))&0xff)

typedef unsigned char u8;
extern const u8 program[];

typedef struct {
  union {
    int i;
    float f;
  };
} ifu;

#define MAX_PARAMS 128

typedef struct {
  int count;
  u8 *values;
  unsigned short *dsamples;
} paramtbl;

extern paramtbl paramtbls[];
extern int nparamtbls;

extern ifu params[MAX_PARAMS];