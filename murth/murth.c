#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "murth.h"

#define CORES 16
#define STACK_SIZE 32
#define GLOBALS 128
#define MAX_LABELS 128
#define SAMPLERS 4
#define MAX_SAMPLER_SIZE_BITS 16
#define MAX_SAMPLER_SIZE (1 << MAX_SAMPLER_SIZE_BITS)
#define MAX_SAMPLER_SIZE_MASK (MAX_SAMPLER_SIZE - 1)
#define MAX_PENDING_EVENTS 32

typedef unsigned char u8;

typedef struct {
  int cursor;
  float samples[MAX_SAMPLER_SIZE];
} sampler_t;

typedef struct {
  int samples_to_idle;
  int ttl;
  int pcounter;
  //! \todo two stacks (f-stack and i-stack ?)
  var_t stack[STACK_SIZE], *top;
} core_t;
typedef void(*instr_func_t)(core_t *c);

typedef struct {
  union {
    instr_func_t func;
    int param;
  };
} runtime_opcode_t;
runtime_opcode_t *program;

static int samplerate = 0;
static int samples_in_tick = 0; 
static core_t cores[CORES];
static var_t globals[GLOBALS];
static sampler_t samplers[SAMPLERS];
static struct {
  int writepos, readpos;
  murth_event_t events[MAX_PENDING_EVENTS];
} event_queue;
static int current_sample;

void i_load(core_t *c) { (--c->top)->i = program[c->pcounter++].param; }
void i_load0(core_t *c) { (--c->top)->i = 0; }
void i_load1(core_t *c) { (--c->top)->i = 1; }
void i_fload1(core_t *c) { (--c->top)->f = 1.f; }
void i_idle(core_t *c) { c->samples_to_idle = (c->top++)->i + 1; }
void i_jmp(core_t *c) { c->pcounter = (c->top++)->i; }
void i_jmpnz(core_t *c) {
  if (c->top[1].i != 0) {
    c->pcounter = c->top[0].i;
  }
  c->top += 2;
}
void i_yield(core_t *c) { c->samples_to_idle = 1; }
void i_ret(core_t *c) { c->pcounter = -1, c->samples_to_idle = 1; }
void i_spawn(core_t *c) {
  const int args = c->top[1].i;
  for (int i = 0; i < CORES; ++i)
    if (cores[i].pcounter < 0) {
      core_t *nc = cores + i;
      nc->samples_to_idle = 0;
      nc->ttl = 1;
      nc->pcounter = c->top[0].i;
      nc->top = nc->stack + STACK_SIZE - args;
      memcpy(nc->top, c->top + 2, sizeof(var_t) * args);
      break;
    }
  c->top += 2 + args;
}
void i_storettl(core_t *c) { c->ttl = c->top[0].i, ++c->top; }
void i_loadglobal(core_t *c) { c->top->i = globals[c->top->i].i; }
void i_storeglobal(core_t *c) { globals[c->top->i].i = c->top[1].i; ++c->top; }
void i_clearglobal(core_t *c) { globals[c->top[0].i].i = 0; ++c->top; }
void i_faddglobal(core_t *c) { globals[c->top[0].i].f += c->top[1].f; ++c->top; }
void i_dup(core_t *c) { --c->top; c->top->i = c->top[1].i; }
void i_get(core_t *c) { *c->top = c->top[c->top->i]; }
void i_put(core_t *c) { c->top[c->top->i] = c->top[1]; ++c->top; }
void i_pop(core_t *c) { ++c->top; }
void i_swap(core_t *c) { var_t v = c->top[0]; c->top[0] = c->top[1]; c->top[1] = v; }
void i_swap2(core_t *c) {
  var_t v = c->top[0]; c->top[0] = c->top[2]; c->top[2] = v;
  v = c->top[1]; c->top[1] = c->top[3]; c->top[3] = v;
}
void i_fsign(core_t *c) { c->top->f = c->top->f < 0 ? -1.f : 1.f; }
void i_fadd(core_t *c) { c->top[1].f += c->top[0].f; ++c->top; }
void i_faddnp(core_t *c) { c->top[0].f += c->top[1].f; }
void i_fsub(core_t *c) { c->top[1].f -= c->top[0].f; ++c->top; }
void i_fmul(core_t *c) { c->top[1].f *= c->top->f, ++c->top; }
void i_fdiv(core_t *c) { c->top[1].f /= c->top->f, ++c->top; }
void i_fsin(core_t *c) { c->top->f = sinf(c->top->f * 3.1415926f); }
void i_fclamp(core_t *c) {
  if (c->top->f < -1.f) c->top->f = -1.f;
  if (c->top->f > 1.f) c->top->f = 1.f;
}
void i_fphase(core_t *c) {
  c->top->f = fmodf(c->top->f + 1.f, 2.f) - 1.f;
}
void i_int127(core_t *c) { c->top->i = c->top->f * 127; }
void i_int(core_t *c) { c->top->i = c->top->f; }
void i_in2dp(core_t *c) {
  int n = c->top->i;
  float kht = 1.059463f; //2^(1/12)
  float f = 55.f / (float)samplerate;
  while(--n>=0) f *= kht; // cheap pow!
  c->top->f = f;
}
void i_float127(core_t *c) { c->top->f = c->top->i / 127.f; }
void i_float(core_t *c) { c->top->f = c->top->i; }
void i_iadd(core_t *c) { c->top[1].i += c->top->i, ++c->top; }
void i_iinc(core_t *c) { c->top[0].i++; }
void i_idec(core_t *c) { c->top[0].i--; }
void i_imul(core_t *c) { c->top[1].i *= c->top->i; ++c->top; }
void i_imulticks(core_t *c) { c->top->i *= samples_in_tick; }
void i_fmulticks(core_t *c) { c->top->f *= samples_in_tick; }
void i_noise(core_t *c) {
  static int seed = 127; seed *= 16807;
  (--c->top)->i = ((unsigned)seed >> 9) | 0x3f800000u;
  c->top[0].f -= 1.0f;
}
void i_abs(core_t *c) { c->top->i &= 0x7fffffff; }
void i_emit(core_t *c) {
  if (event_queue.readpos == event_queue.readpos) {
    event_queue.writepos = 0;
    //OSMemoryBarrier();
    event_queue.readpos = 0;
  }
  if (event_queue.writepos < MAX_PENDING_EVENTS) {
    event_queue.events[event_queue.writepos].samplestamp = current_sample;
    event_queue.events[event_queue.writepos].value = c->top[0];
    event_queue.events[event_queue.writepos].event = c->top[1].i;
    //OSAtomicIncrement32Barrier(&event_queue.writepos);
    //OSMemoryBarrier();
    ++event_queue.writepos;
  }
  c->top += 2;
}
void i_storesampler(core_t *c) {
  sampler_t *s = samplers + c->top[0].i;
  s->samples[s->cursor] = c->top[1].f;
  s->cursor = (s->cursor + 1) & MAX_SAMPLER_SIZE_MASK;
  ++c->top;
}
//void i_loadsampler(core_t *c) {
//}
void i_loadsamplerd(core_t *c) {
  sampler_t *s = samplers + c->top[0].i;
  c->top[1].f = s->samples[(s->cursor - c->top[1].i) & MAX_SAMPLER_SIZE_MASK];
  ++c->top;
}

#if defined(DEBUG)
#undef DEBUG
#define DEBUG 0
#endif
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

static void run_core(core_t *c) {
  if (c->pcounter >= 0) {
    L("core %p:\n", c);
    while(c->samples_to_idle <= 0) {
      COREDUMP(c);
      c->samples_to_idle = 0;
      program[c->pcounter++].func(c);
    }
    COREDUMP(c);
    --c->samples_to_idle;
    if (c->ttl == 0 || ((c->ttl > 0) && ((--c->ttl) == 0))) c->pcounter = -1;
  }
}

void murth_synthesize(float *ptr, int count) {
  for (int i = 0; i < count; ++i) {
    globals[0].i = 0;
    for (int j = CORES - 1; j >= 0; --j) run_core(cores + j);
    ptr[i] = globals[0].f;
    ++current_sample;
  }
}

void murth_init(int in_samplerate, int bpm) {
  current_sample = 0;
  samplerate = in_samplerate;
  samples_in_tick = (samplerate * 60) / (16 * bpm);

  // prepare
  memset(&event_queue, 0, sizeof(event_queue));

  for (int i = 1; i < CORES; ++i)
    cores[i].pcounter = -1;
  cores[0].samples_to_idle = 0;
  cores[0].pcounter = 0;
  cores[0].ttl = -1;
  cores[0].top = cores[0].stack + STACK_SIZE - 1;
}

void murth_set_instruction_limit(int max_per_sample) {
}

#if 0
typedef struct {
  int channel, note;
  int address;
} midi_note_handler_t;
typedef struct {
  int channel, control, value;
  int address;
} midi_control_handler_t;
static midi_note_handler_t note_handlers[MAX_HANDLERS];
static midi_control_handler_t control_handlers[MAX_HANDLERS];
static int note_handlers_count;
static int control_handlers_count;

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
      (control_handlers[i].channel == -1
       || control_handlers[i].channel == channel) &&
      (control_handlers[i].control == -1
       || control_handlers[i].control == control) &&
      (control_handlers[i].value == -1
       || control_handlers[i].value == value)) {
      core_t c;
      c.samples_to_idle = 0;
      c.pcounter = control_handlers[i].address;
      c.top = c.stack + STACK_SIZE - 4;
      c.top[0].i = value, c.top[1].i = control, c.top[2].i = channel;
      run_core(&c);
    }
}
#endif

void murth_process_raw_midi(const void *packet, int bytes) {
  /*const u8 *p = (u8*)packet;
  for (int i = 0; i < bytes; ++i)
    switch(p[i] & 0xf0) {
      case 0x90: run_note(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // note on
      case 0xb0: run_control(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // control change
    }*/
}

int murth_get_event(murth_event_t *event) {
  if (event_queue.readpos >= event_queue.writepos)
    return 0;
  memcpy(event, event_queue.events + event_queue.readpos, sizeof(*event));
  ++event_queue.readpos;
  return 1;
}

/******************************************************************************
 * Program section
 ******************************************************************************/

typedef struct {
  const char *name;
  instr_func_t func;
} instruction_t;
instruction_t instructions[] = {
#define I(name) {#name, i_##name}
  I(load), I(load0), I(load1), I(fload1), I(idle), I(jmp), I(jmpnz), I(yield),
  I(ret), I(spawn), I(storettl), I(emit),
  I(loadglobal), I(storeglobal), I(clearglobal), I(faddglobal),
  I(imulticks), I(fmulticks),
  I(dup), I(get), I(put), I(pop), I(swap), I(swap2),
  I(fsign), I(fadd), I(faddnp), I(fsub), I(fmul), I(fdiv), I(fsin), I(fclamp),
  I(fphase), I(noise),
  I(in2dp), I(abs),
  I(int), I(int127), I(float127), I(float), I(iadd), I(iinc), I(idec), I(imul),
  I(storesampler), I(loadsamplerd)
#undef I
};

#define MAX_PROGRAM_LENGTH 16384
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
  name_t global_names[GLOBALS-1];
  name_t sampler_names[SAMPLERS];
  label_t label_names[GLOBALS];
  label_t label_forwards[MAX_FORWARDS];
  runtime_opcode_t program[MAX_PROGRAM_LENGTH];
} program_t;

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

static int get_global(program_t *p, const char *name) {
  int ret = get_name(name, p->global_names, GLOBALS - 1) + 1; // 0 is output
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for global var '%s' -- too many globals\n", name);
    abort();
  }
  return ret;
}

static int get_sampler(program_t *p, const char *name) {
  int ret = get_name(name, p->sampler_names, SAMPLERS);
  if (ret == -1) {
    fprintf(stderr, "cannot allocate slot for sampler '%s' -- too many samplers\n", name);
    abort();
  }
  return ret;
}

static int get_label_address(program_t *p, const char *name, int for_position) {
  for (int i = 0; i < MAX_LABELS; ++i)
    if (strncmp(name, p->label_names[i].name, MAX_NAME_LENGTH) == 0)
      return p->label_names[i].position;
  if (for_position == -1) {
      fprintf(stderr, "forwarded label '%s' not found\n", name);
      abort();
      return -1;
  }
  for (int i = 0; i < MAX_FORWARDS; ++i)
    if (p->label_forwards[i].name[0] == 0) {
      strcpy(p->label_forwards[i].name, name);
      p->label_forwards[i].position = for_position;
      return for_position;
    }
  fprintf(stderr, "label '%s' forward @ %d -- too many forwards\n", name, for_position);
  abort();
  return -1;
}

static int set_label(program_t *p, const char *name, int position) {
  for (int i = 0; i < MAX_LABELS; ++i) {
    if (strncmp(name, p->label_names[i].name, MAX_NAME_LENGTH) == 0) {
      fprintf(stderr, "label '%s' already exists\n", name);
      abort();
      return -1;
    }
    if (p->label_names[i].name[0] == 0) {
      strcpy(p->label_names[i].name, name);
      p->label_names[i].position = position;
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

murth_program_t murth_program_create() {
  murth_program_t program = malloc(sizeof(program_t));
  return program;
}

int murth_program_compile(murth_program_t program, const char *source) {
  program_t *p = (program_t*)program;
  memset(program, 0, sizeof(program_t));
  int program_counter = 0;
  const char *tok = source, *tokend;
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

    const long len = tokend - tok;
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

    if (program_counter >= (MAX_PROGRAM_LENGTH-2)) {
      fprintf(stderr, "Wow, dude, your program is HUGE!11\n");
      return -1;
    }

    if (token[0] == ';') { // comment
      tokend += strcspn(tokend, "\n");
    } else if (token[0] == '$') { // global var reference
      p->program[program_counter++].func = i_load;
      p->program[program_counter++].param= get_global(p, token);
    } else if (token[0] == '@') { // sampler reference
      p->program[program_counter++].func = i_load;
      p->program[program_counter++].param = get_sampler(p, token);
    } else if (token[len-1] == ':') { // label definition
      token[len-1] = 0;
      set_label(p, token, program_counter);
    } else if (token[0] == ':') { // label reference
      p->program[program_counter++].func = i_load;
      int label_position = program_counter;
      p->program[program_counter++].param = get_label_address(p, token+1, label_position);
    } else if ((typehint&ALPHA) != 0) { // some kind of string reference
      p->program[program_counter++].func = instructions[get_instruction(token)].func;
    } else if ((typehint&(DIGIT|DOT)) == (DIGIT|DOT)) { // float constant (no alpha, but digits with a dot)
      var_t v;
      v.f = atof(token);
      p->program[program_counter++].func = i_load;
      p->program[program_counter++].param = v.i;
    } else if ((typehint&DIGIT) != 0) { // integer constant (no alpha and no dot, but digits)
      p->program[program_counter++].func = i_load;
      p->program[program_counter++].param = atoi(token);
    } else { // something invalid
      fprintf(stderr, "token '%s' makes no sense\n", token);
      return -1;
    }
    tok = tokend;
  }

  // patch forwarded labels
  for (int i = 0; i < MAX_FORWARDS; ++i)
    if (p->label_forwards[i].name[0] != 0)
      p->program[p->label_forwards[i].position].param = get_label_address(p, p->label_forwards[i].name, -1);


/*  for (int i = 0; i < program_counter; ++i) {
    var_t v; v.i = program[i];
    const char *fname = (program[i] >= 0 &&
                         program[i] < sizeof(instructions)/sizeof(*instructions)) ?
                        instructions[program[i]].name : "%%invalid%";
    fprintf(stderr, "0x%08x: %08x (%d, %f, <%s>)\n", i, program[i], v.i, v.f, fname);
  }*/

  return 0;
}

const char *murth_program_get_status(murth_program_t program) {
  return "ERROR: NOT IMPLEMENTED";
}

void murth_program_destroy(murth_program_t program) {
  free(program);
}

void murth_set_program(murth_program_t prog) {
  program = ((program_t*)prog)->program;
}
