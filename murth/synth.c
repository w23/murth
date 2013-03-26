#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "synth.h"

#define CORES 16
#define STACK_SIZE 128
#define GLOBALS 128
#define MAX_LABELS 128
#define MAX_PROGRAM_LENGTH 1024
#define MAX_NAME_LENGTH 32
#define SAMPLERS 4
#define MAX_SAMPLER_SIZE_BITS 16
#define MAX_SAMPLER_SIZE (1 << MAX_SAMPLER_SIZE_BITS)
#define MAX_SAMPLER_SIZE_MASK (MAX_SAMPLER_SIZE - 1)

typedef unsigned char u8;
typedef struct {
  union {
    int i;
    float f;
  };
} var_t;
typedef struct {
  float samples[MAX_SAMPLER_SIZE];
} sampler_t;
typedef struct {
  int samples_to_idle;
  int pcounter;
  var_t stack[STACK_SIZE], *top;
} core_t;
typedef void(*instr_func_t)(core_t *c);

static int program[MAX_PROGRAM_LENGTH];
static int samplerate = 0;
static int samples_in_tick = 0; 
static core_t cores[CORES];
static var_t globals[GLOBALS];
//static sampler_t samplers[SAMPLERS];

void i_load(core_t *c) { (--c->top)->i = program[++c->pcounter]; }
void i_idle(core_t *c) { c->samples_to_idle = (++c->top)->i * samples_in_tick + 1; }
void i_jmp(core_t *c) { c->pcounter = (++c->top)->i; }
void i_ret(core_t *c) { c->pcounter = -1; }
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

/*instr instruction_table[] = {
  i_load, i_idle, i_jmp, i_ret, // control
  i_loadglobal, i_storeglobal, // globals
  i_dup, i_ndup, i_pop, // basic stack
  i_fsgn, i_fadd, i_fmul, i_fsin, i_fclamp, i_fphaserot // float ops
};*/

typedef struct {
  const char *name;
  instr_func_t func;
} instruction_t;
instruction_t instructions[] = {
#define I(name) {#name, i_##name}
  I(load), I(idle), I(jmp), I(ret),
  I(loadglobal), I(storeglobal),
  I(dup), I(ndup), I(pop),
  I(fsgn), I(fadd), I(fmul), I(fsin), I(fclamp), I(fphaserot),
  I(inote2fdeltaphase)
};

void murth_synthesize(float *ptr, int count) {
  for (int i = 0; i < count; ++i) {
    globals[0].i = 0;
    for (int j = CORES - 1; j >= 0; --j)
      if (cores[j].pcounter >= 0) {
        core_t *c = cores + j;
        while(c->samples_to_idle == 0) instructions[program[c->pcounter++]].func(c);
        --c->samples_to_idle;
      }
    ptr[i] = globals[0].f;
  }
}

void murth_process_raw_midi(void *packet, int bytes) {
}

typedef struct {
  char name[MAX_NAME_LENGTH];
} name_t;
typedef struct {
  char name[MAX_NAME_LENGTH];
  int position;
} label_t;
name_t global_names[GLOBALS-1];
name_t sampler_names[SAMPLERS];
label_t label_names[GLOBALS];

int get_name(const char *name, name_t *names, int max)
{
  for (int i = 0; i < max; ++i) {
    if (strncmp(name, names[i].name, MAX_NAME_LENGTH) == 0)
      return i;
    if (names[i].name[0] == 0) {
      strcpy(names[i].name, name);
      return i;
    }
  }
  return -1;
}

int get_global(const char *name) {
  int ret = get_name(name, global_names, GLOBALS - 1) + 1; // 0 is output
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for global var '%s' -- too many globals\n", name);
    abort();
  }
  return ret;
}

int get_sampler(const char *name) {
  int ret = get_name(name, sampler_names, SAMPLERS);
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for sampler '%s' -- too many samplers\n", name);
    abort();
  }
  return ret;
}

int get_label(const char *name) {
  for (int i = 0; i < MAX_LABELS; ++i) {
    if (strncmp(name, label_names[i].name, MAX_NAME_LENGTH) == 0)
      return i;
  }
  fprintf(stderr, "label '%s' not found\n", name);
  abort();
  return -1;
}

int set_label(const char *name, int position) {
  for (int i = 0; i < MAX_LABELS; ++i) {
    if (strncmp(name, label_names[i].name, MAX_NAME_LENGTH) == 0) {
      fprintf(stderr, "label '%s' already exists\n", name);
      abort();
      return -1;
    }
    if (label_names[i].name[0] == 0) {
      strcpy(label_names[i].name, name);
      label_names[i].position = position;
      return i;
    }
  }
  return -1;
}

int get_instruction(const char *name) {
  for (int i = 0; i < sizeof(instructions)/sizeof(*instructions); ++i)
    if (strcmp(name, instructions[i].name) == 0)
      return i;
  fprintf(stderr, "instruction '%s' is not known\n", name);
  abort();
  return -1;
}


int murth_init(const char *assembly, int in_samplerate, int bpm) {
  samplerate = in_samplerate;
  samples_in_tick = (samplerate * 60) / (16 * bpm);

  // assemble
  const int InstrLoad = 0;
  memset(global_names, 0, sizeof(global_names));
  memset(sampler_names, 0, sizeof(sampler_names));
  memset(label_names, 0, sizeof(label_names));
  int program_counter = 0;
  const char *tok = assembly, *tokend;
  for(;*tok != 0;) {
    while (isspace(*tok)) ++tok;
    tokend = tok;
    int typehint = 0;
#define ALPHA 1
#define DIGIT 2
#define DOT 3
    for (;*tokend != 0;) {
      if (isspace(*tokend) || *tokend == ';') break;
      if (isalpha(*tokend)) typehint |= ALPHA;
      if (isdigit(*tokend)) typehint |= DIGIT;
      if ('.' == *tokend) typehint |= DOT;
      ++tokend;
    }
    
    if (*tokend == ';') { // comment
      while (*tokend != '\n' && *tokend != 0) tokend++; // skip whole remaining line
      tok = tokend;
      continue;
    }

    const int len = tokend - tok;
    char token[MAX_NAME_LENGTH];
    if (len >= MAX_NAME_LENGTH)
    {
      memcpy(token, tok, MAX_NAME_LENGTH-1);
      token[MAX_NAME_LENGTH-1] = 0;
      fprintf(stderr, "token '%s...' is too long\n", token);
      return -1;
    }
    memcpy(token, tok, len);
    token[len] = 0;

    if (len < 2) {
      fprintf(stderr, "token '%s' is too short\n", token);
      return -1;
    }

    if (token[0] == '$') { // global var reference
      program[program_counter++] = InstrLoad;
      program[program_counter++] = get_global(token);
    } else if (token[0] == '@') { // sampler reference
      program[program_counter++] = InstrLoad;
      program[program_counter++] = get_sampler(token);
    } else if (token[len-1] == ':') { // label definition
      token[len-1] = 0;
      set_label(token, program_counter);
    } else if (token[0] == ':') { // label reference
      program[program_counter++] = InstrLoad;
      program[program_counter++] = get_label(token+1);
    } else if ((typehint&ALPHA) != 0) { // some kind of string reference
      program[program_counter++] = get_instruction(token);
    } else if ((typehint&(DIGIT|DOT)) == (DIGIT|DOT)) { // float constant (no alpha, but digits with a dot)
      var_t v;
      v.f = atof(token);
      program[program_counter++] = InstrLoad;
      program[program_counter++] = v.i;
    } else if ((typehint&DIGIT) != 0) { // integer constant (no alpha and no dot, but digits)
      program[program_counter++] = InstrLoad;
      program[program_counter++] = atoi(token);
    } else { // something invalid
      fprintf(stderr, "token '%s' makes no sense\n", token);
      return -1;
    }
    tok = tokend;
  }

  for (int i = 1; i < CORES; ++i)
    cores[i].pcounter = -1;
  cores[0].samples_to_idle = 0;
  cores[0].pcounter = 0;
  cores[0].top = cores[0].stack + STACK_SIZE;

  for (int i = 0; i < program_counter; ++i) {
    var_t v; v.i = program[i];
    const char *fname = (program[i] >= 0 && 
                         program[i] < sizeof(instructions)/sizeof(*instructions)) ?
                        instructions[program[i]].name : "%%invalid%";
    fprintf(stderr, "0x%08x: %08x (%d, %f, <%s>)\n", i, program[i], v.i, v.f, fname);
  }

  return 0;
}
