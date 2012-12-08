#include <math.h>
#include "synth.h"

#define STACK_SIZE 128

typedef void(*instr)(void);

ifu stack[STACK_SIZE];
ifu *top;

int globals[128] = { 0 };
float params[128] = { 12 / 127., 17 / 127., 19 / 127. };

void i_ldg() {
  top->i = globals[top->i];
}

void i_stg() {
  globals[top->i] = top[1].i;
  ++top;
}

void i_ldp() {
  float p = params[top->i];
  top->f = p;
}

void i_add() {
  top[1].f += top[0].f;
  ++top;
}

void i_sin() {
  top->f = sinf(top->f * 3.1415926f);
}

void i_clamp() {
  if (top->f < -1.f)
    top->f = -1.f;
  if (top->f > 1.f)
    top->f = 1.f;
}

void i_phrot() {
  top->f = fmodf(top->f + 1.f, 2.f) - 1.f;
}

void i_f2iu() {
  top->i = top->f * 127;
}

void i_i2fu() {
  top->f = top->i / 127.f;
}

void i_mul() {
  top[1].f *= top->f;
  ++top;
}

void i_iadd() {
  top[1].i += top->i;
  ++top;
}

void i_n2dp() {
  int n = top->i;
  float kht = 1.059463f; //2^(1/12)
  float f = 55.f / 44100.f;
  while(--n>=0) f *= kht; // cheap pow!
  top->f = f;
}

instr instruction_table[64] = {
  i_ldg, i_stg, i_ldp, i_add, i_phrot, i_sin, i_f2iu, i_n2dp,
  i_i2fu, i_mul, i_iadd
};

//#define L(...) printf(__VA_ARGS__)
#define L(...) {}

void run(int offset)
{
  for(;; ++offset)
  {
    u8 icode = program[offset];
    L("[%03d] ", icode);
    if (icode < 128)
      (--top)->i = icode;
    else if (icode == 255)
      return;
    else
      instruction_table[icode&127]();
    L("top %f (%d)\n", top->f, top->i);
  }
}

void synth(short *ptr, int count)
{
  for (int i = 0; i < count; ++i)
  {
    top = stack + STACK_SIZE;
    run(0);
    ptr[i] = top->f * 32767.f;
  }
}