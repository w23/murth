#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "synth.h"

#define CORES 4
#define STACK_SIZE 16
#define GLOBALS 16
#define MAX_LABELS 16
#define MAX_PROGRAM_LENGTH 256
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

void i_load(core_t *c) { (--c->top)->i = program[c->pcounter++]; }
void i_idle(core_t *c) { c->samples_to_idle = (c->top++)->i + 1; }
void i_jmp(core_t *c) { c->pcounter = (c->top++)->i; }
void i_ret(core_t *c) { c->pcounter = -1, c->samples_to_idle = 1; }
void i_loadglobal(core_t *c) { c->top->i = globals[c->top->i].i; }
void i_storeglobal(core_t *c) { globals[c->top->i].i = c->top[1].i; ++c->top; }
void i_dup(core_t *c) { --c->top; c->top->i = c->top[1].i; }
void i_ndup(core_t *c) { --c->top; c->top->i = c->top[c->top[1].i+1].i; }
void i_pop(core_t *c) { ++c->top; }
void i_fsgn(core_t *c) { c->top->f = c->top->f < 0 ? -1.f : 1.f; }
void i_fadd(core_t *c) { c->top[1].f += c->top[0].f; ++c->top; }
void i_fsub(core_t *c) { c->top[1].f -= c->top[0].f; ++c->top; }
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
void i_spawn(core_t *c) {
  const int args = c->top[1].i;
  for (int i = 0; i < CORES; ++i)
    if (cores[i].pcounter < 0) {
      core_t *nc = cores + i;
      nc->samples_to_idle = 0;
      nc->pcounter = c->top[0].i;
      nc->top = nc->stack + STACK_SIZE - args;
      memcpy(nc->top, c->top + 2, sizeof(var_t) * args);
      break;
    }
  c->top += 2 + args;
}
void i_ticks(core_t *c) { c->top->i *= samples_in_tick; }
void i_noise(core_t *c) {
  static int seed = 127;
  seed *= 16807;
  (--c->top)->i = ((unsigned)seed >> 9) | 0x3f800000u;
  c->top[0].f -= 1.0f;
}

typedef struct {
  const char *name;
  instr_func_t func;
} instruction_t;
instruction_t instructions[] = {
#define I(name) {#name, i_##name}
  I(load), I(idle), I(jmp), I(ret), I(spawn),
  I(loadglobal), I(storeglobal), I(ticks),
  I(dup), I(ndup), I(pop),
  I(fsgn), I(fadd), I(fsub), I(fmul), I(fsin), I(fclamp), I(fphaserot), I(noise),
  I(inote2fdeltaphase)
};

#if DEBUG
#define L(...) fprintf(stderr, __VA_ARGS__);
void coredump(core_t *c) {
  L("%d %08x s=%d[ ", c->samples_to_idle, c->pcounter, (int)(c->stack + STACK_SIZE - c->top));
  for(var_t *p = c->stack + STACK_SIZE - 1; p >= c->top; --p)
    L("%f(%d) ", p->f, p->i);
  L("]\n");
}
#define COREDUMP(c) coredump(c)
#else
#define COREDUMP(c)
#define L(...)
#endif

inline static void run_core(core_t *c) {
  if (c->pcounter >= 0) {
    L("core %p:\n", c);
    while(c->samples_to_idle <= 0) {
      COREDUMP(c);
      c->samples_to_idle = 0;
      instructions[program[c->pcounter++]].func(c);
    }
    COREDUMP(c);
    --c->samples_to_idle;
  }
}

void murth_synthesize(float *ptr, int count) {
  for (int i = 0; i < count; ++i) {
    globals[0].i = 0;
    for (int j = CORES - 1; j >= 0; --j) run_core(cores + j);
    ptr[i] = globals[0].f;
  }
}

#define MAX_NAME_LENGTH 32
#define MAX_HANDLERS 16
#define MAX_FORWARDS (GLOBALS*4)
typedef struct {
  char name[MAX_NAME_LENGTH];
} name_t;
typedef struct {
  char name[MAX_NAME_LENGTH];
  int position;
} label_t;
typedef struct {
  int channel, note;
  int address;
} midi_note_handler_t;
typedef struct {
  int channel, control, value;
  int address;
} midi_control_handler_t;
static name_t global_names[GLOBALS-1];
static name_t sampler_names[SAMPLERS];
static label_t label_names[GLOBALS];
static label_t label_forwards[MAX_FORWARDS];
static midi_note_handler_t note_handlers[MAX_HANDLERS];
static midi_control_handler_t control_handlers[MAX_HANDLERS];
static int note_handlers_count;
static int control_handlers_count;


static int get_name(const char *name, name_t *names, int max)
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

static int get_global(const char *name) {
  int ret = get_name(name, global_names, GLOBALS - 1) + 1; // 0 is output
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for global var '%s' -- too many globals\n", name);
    abort();
  }
  return ret;
}

static int get_sampler(const char *name) {
  int ret = get_name(name, sampler_names, SAMPLERS);
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for sampler '%s' -- too many samplers\n", name);
    abort();
  }
  return ret;
}

static int get_label_address(const char *name, int for_position) {
  for (int i = 0; i < MAX_LABELS; ++i)
    if (strncmp(name, label_names[i].name, MAX_NAME_LENGTH) == 0)
      return label_names[i].position;
  if (for_position == -1) {
      fprintf(stderr, "forwarded label '%s' not found\n", name);
      abort();
      return -1;
  }
  for (int i = 0; i < MAX_FORWARDS; ++i)
    if (label_forwards[i].name[0] == 0) {
      strcpy(label_forwards[i].name, name);
      label_forwards[i].position = for_position;
      return for_position;
    }
  fprintf(stderr, "label '%s' forward @ %d -- too many forwards\n", name, for_position);
  abort();
  return -1;
}

static int set_label(const char *name, int position) {
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

static int get_instruction(const char *name) {
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
  // prepare
  memset(global_names, 0, sizeof(global_names));
  memset(sampler_names, 0, sizeof(sampler_names));
  memset(label_names, 0, sizeof(label_names));
  memset(label_forwards, 0, sizeof(label_forwards));
  note_handlers_count = control_handlers_count = 0;

  const int InstrLoad = get_instruction("load");
  int program_counter = 0;
  const char *tok = assembly, *tokend;
  for(;*tok != 0;) {
    while (isspace(*tok)) ++tok;
    tokend = tok;
    int typehint = 0;
#define ALPHA 1
#define DIGIT 2
#define DOT 4
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
    if (len == 0) break;

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

    L("'%s': %02x\n", token, typehint);

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
      int label_position = program_counter;
      program[program_counter++] = get_label_address(token+1, label_position);
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

  // patch forwarded labels
  for (int i = 0; i < MAX_FORWARDS; ++i)
    if (label_forwards[i].name[0] != 0)
      program[label_forwards[i].position] = get_label_address(label_forwards[i].name, -1);

  for (int i = 1; i < CORES; ++i)
    cores[i].pcounter = -1;
  cores[0].samples_to_idle = 0;
  cores[0].pcounter = 0;
  cores[0].top = cores[0].stack + STACK_SIZE - 1;

  for (int i = 0; i < program_counter; ++i) {
    var_t v; v.i = program[i];
    const char *fname = (program[i] >= 0 && 
                         program[i] < sizeof(instructions)/sizeof(*instructions)) ?
                        instructions[program[i]].name : "%%invalid%";
    fprintf(stderr, "0x%08x: %08x (%d, %f, <%s>)\n", i, program[i], v.i, v.f, fname);
  }

  return 0;
}

int murth_set_midi_control_handler(const char *label, int channel, int control, int value) {
  if (control_handlers_count == MAX_HANDLERS) {
    fprintf(stderr, "Too many MIDI control handlers\n");
    return -1;
  }
  midi_control_handler_t *h = control_handlers + control_handlers_count;
  h->address = get_label_address(label, -1);
  if (h->address == -1) {
    fprintf(stderr, "label '%s' required for control handler not found\n", label);
    return -1;
  }
  h->channel = channel; h->control = control; h->value = value;
  return control_handlers_count++;
}

int murth_set_midi_note_handler(const char *label, int channel, int note) {
  if (note_handlers_count == MAX_HANDLERS) {
    fprintf(stderr, "Too many MIDI control handlers\n");
    return -1;
  }
  midi_note_handler_t *h = note_handlers + note_handlers_count;
  h->address = get_label_address(label, -1);
  if (h->address == -1) {
    fprintf(stderr, "label '%s' required for note handler not found\n", label);
    return -1;
  }
  h->channel = channel; h->note = note;
  return note_handlers_count++;
}

static void run_note(int channel, int note, int velocity) {
  for (int i = 0; i < note_handlers_count; ++i)
    if (note_handlers[i].address >= 0 &&
      (note_handlers[i].channel == -1 || note_handlers[i].channel == channel) &&
      (note_handlers[i].note == -1 || note_handlers[i].note == note)) {
      core_t c;
      c.samples_to_idle = 0;
      c.pcounter = note_handlers[i].address;
      c.top = c.stack + STACK_SIZE - 4;
      c.top[0].i = note, c.top[1].i = velocity, c.top[2].i = channel;
      run_core(&c);
    }
}

static void run_control(int channel, int control, int value) {
  for (int i = 0; i < control_handlers_count; ++i)
    if (control_handlers[i].address >= 0 &&
      (control_handlers[i].channel == -1 || control_handlers[i].channel == channel) &&
      (control_handlers[i].control == -1 || control_handlers[i].control == control) &&
      (control_handlers[i].value == -1 || control_handlers[i].value == value)) {
      core_t c;
      c.samples_to_idle = 0;
      c.pcounter = control_handlers[i].address;
      c.top = c.stack + STACK_SIZE - 4;
      c.top[0].i = value, c.top[1].i = control, c.top[2].i = channel;
      run_core(&c);
    }
}

void murth_process_raw_midi(void *packet, int bytes) {
  const u8 *p = (u8*)packet;
  for (int i = 0; i < bytes; ++i)
    switch(p[i] & 0xf0) {
      case 0x90: run_note(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // note on
      case 0xb0: run_control(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // control change
    }
}

