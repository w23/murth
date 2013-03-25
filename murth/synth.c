#include <math.h>
#include "synth.h"

#define CORES 4
#define STACK_SIZE 128
#define MAX_GLOBALS 128
#define MAX_PROGRAM_LENGTH 1024
#define SAMPLERS 4
#define MAX_SAMPLER_SIZE_BITS 16
#define MAX_SAMPLER_SIZE (1 << MAX_SAMPLER_SIZE_BITS)
#define MAX_SAMPLER_SIZE_MASK (MAX_SAMPLER_SIZE - 1)

//static int samplerate = 0;
static int samples_in_tick; 

static unsigned program[MAX_PROGRAM_LENGTH];

typedef struct {
  float samples[MAX_SAMPLER_SIZE];
} sampler_t;

typedef struct {
  int samples_to_idle;
  int pcounter;
  ifu stack[STACK_SIZE], *top;
} core_t;

core_t cores[CORES];
ifu globals[MAX_GLOBALS];
sampler_t samplers[SAMPLERS];

typedef void(*instr)(core_t *c);

void i_load(core_t *c) { (--c->top)->i = program[++c->pcounter]; }
void i_idle(core_t *c) { c->samples_to_idle = (++c->top)->i * samples_in_tick; }
void i_jmp(core_t *c) { c->pcounter = (++c->top)->i; }
void i_loadglobal(core_t *c) { c->top->i = globals[c->top->i].i; }
void i_storeglobal(core_t *c) { globals[c->top->i].i = c->top[1].i; ++c->top; }
void i_dup(core_t *c) { --c->top; c->top->i = c->top[1].i; }
void i_ndup(core_t *c) { --c->top; c->top->i = c->top[c->top[1].i+1].i; }
void i_pop(core_t *c) { ++c->top; }
void i_fsgn(core_t *c) { c->top->f = c->top->f < 0 ? -1.f : 1.f; }
void i_fadd(core_t *c) { c->top[1].f += c->top[0].f; ++c->top; }
void i_fmul(core_t *c) { c->top[1].f *= c->top->f, ++c->top; }
void i_fsin(core_t *c) { c->top->f = sinf(c->top->f * 3.1415926f); }
void i_fclamp(core_t *c) {
  if (c->top->f < -1.f) c->top->f = -1.f;
  if (c->top->f > 1.f) c->top->f = 1.f;
}
void i_fphaserot(core_t *c) {
  c->top->f = fmodf(c->top->f + 1.f, 2.f) - 1.f;
}
void i_float2unsigned(core_t *c) { c->top->i = c->top->f * 127; }
void i_inote2fdeltaphase(core_t *c) {
  int n = c->top->i;
  float kht = 1.059463f; //2^(1/12)
  float f = 55.f / 44100.f;
  while(--n>=0) f *= kht; // cheap pow!
  c->top->f = f;
}
void i_unsigned2float(core_t *c) { c->top->f = c->top->i / 127.f; }
void i_iadd(core_t *c) { c->top[1].i += c->top->i, ++c->top; }
void i_imul(core_t *c) { c->top[1].i *= c->top->i; ++c->top; }

instr instruction_table[] = {
  i_load, i_idle, i_jmp, // control
  i_loadglobal, i_storeglobal, // globals
  i_dup, i_ndup, i_pop, // basic stack
  i_fsgn, i_fadd, i_fmul, i_fsin, i_fclamp, i_fphaserot // float ops
};

void murth_synthesize(float *ptr, int count) {
  for (int i = 0; i < count; ++i) {
    globals[0].i = 0;
    for (int j = CORES - 1; j >= 0; --j)
      if (cores[j].pcounter >= 0) {
        core_t *c = cores + j;
        while(c->samples_to_idle == 0) instruction_table[program[c->pcounter++]](c);
        --c->samples_to_idle;
      }
    ptr[i] = globals[0].f;
  }
}

void murth_process_raw_midi(void *packet, int bytes) {
}

int murth_assemble(const char *ptr) {
  return -1;
}
