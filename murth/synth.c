#include <math.h>
#include "synth.h"
#include "program.h"

#define STACK_SIZE 128
#define MAX_GLOBALS 128
#define MAX_RINGBUFFERS 8
#define RINGBUFFER_SIZE 65536
#define RINGBUFFER_SIZEMASK 65535

ifu globals[MAX_GLOBALS]; /* = { {.i = 0} }; */
ifu params[MAX_PARAMS]; /* = { {.i = 0} }; */

const unsigned short tick = 10000;

struct {
  unsigned writepos;
  unsigned readpos;
  float samples[RINGBUFFER_SIZE];
} ringbuffers[MAX_RINGBUFFERS]; /* = { {0, 0, {0.0}} }; */

typedef void(*instr)(void);
ifu stack[STACK_SIZE];
ifu *top;

void i_ldg() {
  top->i = globals[top->i].i;
}

void i_stg() {
  globals[top->i].i = top[1].i;
  ++top;
}

void i_ldp() {
  top->f = params[top->i].f;
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

void i_n2dp() {
  int n = top->i;
  float kht = 1.059463f; //2^(1/12)
  float f = 55.f / 44100.f;
  while(--n>=0) f *= kht; // cheap pow!
  top->f = f;
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

void i_dup() {
  --top;
  top->i = top[1].i;
}

void i_ndup() {
  --top;
  top->i = top[top[1].i+1].i;
}

void i_sgn() {
  top->f = top->f < 0 ? -1.f : 1.f;
}

void i_rwr() {
  ringbuffers[top->i].samples[ringbuffers[top->i].writepos] = top[1].f;
  ++ringbuffers[top->i].writepos;
  ringbuffers[top->i].writepos &= RINGBUFFER_SIZEMASK;
  ++top;
}

void i_rrd() {
  float v = ringbuffers[top->i].samples[ringbuffers[top->i].readpos];
//  ++ringbuffers[top->i].readpos;
//  ringbuffers[top->i].readpos &= RINGBUFFER_SIZEMASK;
  top->f = v;
}

void i_rof() {
  ringbuffers[top->i].readpos = ringbuffers[top->i].writepos - top[1].i;
  ringbuffers[top->i].readpos &= RINGBUFFER_SIZEMASK;
  ++top;
}

void i_pop() {
  ++top;
}

void i_imul() {
  top[1].i *= top->i;
  ++top;
}

instr instruction_table[64] = {
  i_ldg, i_stg, i_ldp, i_add, i_phrot, i_sin, i_clamp, i_f2iu, i_n2dp,
  i_i2fu, i_mul, i_iadd, i_dup, i_ndup, i_sgn,
  i_rwr, i_rrd, i_rof, i_pop, i_imul
};

//#define L(...) printf(__VA_ARGS__)
#ifndef L
#define L(...) {}
#endif

void run(int offset)
{
  for(;; ++offset)
  {
    u8 icode = program[offset];
    L("[%03d] ", icode);
    if (icode < 128)
      (--top)->i = icode;
    else if (icode == LOND)
    {
      (--top)->i = (((int)program[offset+1]) << 8) | (int)(program[offset+2]);
      offset += 2;
    } else if (icode == RET)
      return;
    else
      instruction_table[icode&127]();
    L("top %f (%d)\n", top->f, top->i);
  }
}

struct {
  int pos;
  float dv;
  unsigned short sampleft;
} paramstates[MAX_PARAMS] = { {0, 0.0, 0} };

void synth(short *ptr, int count)
{
  for (int i = 0; i < count; ++i)
  {
    for (int j = 0; j < nparamtbls; ++j)
    {
      if (paramstates[j].sampleft == 0)
      {
        params[j].f = paramtbls[j].values[paramstates[j].pos] / 127.f;
        ++paramstates[j].pos;
        paramstates[j].pos %= paramtbls[j].count;
        paramstates[j].sampleft = paramtbls[j].dsamples[paramstates[j].pos];
        paramstates[j].dv = (paramtbls[j].values[paramstates[j].pos] / 127.f - params[j].f) / (float)(paramtbls[j].dsamples[paramstates[j].pos] + 1);
      } else {
        params[j].f += paramstates[j].dv;
        --paramstates[j].sampleft;
      }
    }

    top = stack + STACK_SIZE;
    run(0);
    ptr[i] = top->f * 32767.f;
  }
}
