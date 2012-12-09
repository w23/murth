#pragma once

#define LOAD_SHORT(v) LOND, (((int)(v))>>8), (((int)(v))&0xff)
#define MAX_PARAMS 128
#define TICK 10000 /* makes gcc happy */

typedef unsigned char u8;

enum {
  LDG = 128, STG, LDP, ADD, PHR, SIN, CLAMP, FIU, NDP,
  IFU, MUL, IADD, DUP, NDUP, SGN,
  RWR, RRD, ROF, POP, IMUL,
  LOND = 254, RET = 255
};

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

extern const unsigned short tick;

extern void synth(short* ptr, int count);
