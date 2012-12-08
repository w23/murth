#pragma once

extern void synth(short* ptr, int count);

enum {
  LDG = 128, STG, LDP, ADD, PHR, SIN, FIU, NDP,
  IFU, MUL, IADD,
  RET = 255
};

typedef unsigned char u8;
extern const u8 program[];

typedef struct {
  union {
    int i;
    float f;
  };
} ifu;