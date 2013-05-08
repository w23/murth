#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "nyatomic.h"
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
#define MAX_PROGRAM_LENGTH 16384
#define MAX_NAME_LENGTH 32
#define MAX_HANDLERS 16
#define MAX_FORWARDS (GLOBALS*4)

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
  char name[MAX_NAME_LENGTH];
} name_t;

typedef struct {
  char name[MAX_NAME_LENGTH];
  int position;
} label_t;

typedef struct {
  union {
#if DEBUG
    int instr_index;
#endif
    instr_func_t func;
    int param;
  };
} runtime_opcode_t;

typedef struct {
  int midi_cc_handler;
  int midi_note_handler;
  name_t global_names[GLOBALS-1];
  name_t sampler_names[SAMPLERS];
  label_t label_names[GLOBALS];
  label_t label_forwards[MAX_FORWARDS];
  runtime_opcode_t program[MAX_PROGRAM_LENGTH];
} program_t;

enum message_type_e {
  Reset,
  SetProgram,
  SetBPM,
  SetInstructionLimit
};

typedef struct {
  int type;
  int param;
  const void *ptr; // 64-bitness
  int dummy[2]; // not to be used
} murth_control_message_t;

static int running = -1;
static int samplerate;
static int samples_in_tick;
static int instruction_limit = 1024;
static int current_sample;
static int current_sample_instructions;
static core_t cores[CORES];
static var_t globals[GLOBALS];
static sampler_t samplers[SAMPLERS];
static nya_stream_t sin_midi;
static nya_stream_t sout_events;
static nya_stream_t sin_messages;
static int midi_cc_handler;
static int midi_note_handler;
static const runtime_opcode_t *program;

static core_t *spawn_core(int pcounter, int argc, const var_t *argv) {
  for (int i = 0; i < CORES; ++i)
    if (cores[i].pcounter < 0) {
      core_t *nc = cores + i;
      nc->samples_to_idle = 0;
      nc->ttl = 1;
      nc->pcounter = pcounter;
      nc->top = nc->stack + STACK_SIZE - argc;
      memcpy(nc->top, argv, sizeof(var_t) * argc);
      return nc;
    }
  return NULL;
}

void i_load(core_t *c) { (--c->top)->i = program[c->pcounter++].param; }
void i_load0(core_t *c) { (--c->top)->i = 0; }
void i_load1(core_t *c) { (--c->top)->i = 1; }
void i_fload1(core_t *c) { (--c->top)->f = 1.f; }
void i_idle(core_t *c) { c->samples_to_idle = (c->top++)->i + 1; }
void i_jmp(core_t *c) { c->pcounter = (c->top++)->i; }
void i_jmpnz(core_t *c) {
  if (c->top[1].i != 0) c->pcounter = c->top[0].i;
  c->top += 2;
}
void i_yield(core_t *c) { c->samples_to_idle = 1; }
void i_ret(core_t *c) { c->pcounter = -1, c->samples_to_idle = 1; }
void i_spawn(core_t *c) {
  const int args = c->top[1].i;
  spawn_core(c->top[0].i, args, c->top + 2);
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
// 'cheap' pow!
  while(n >= 12) { f *= 2; n -= 12; }
  while(--n>=0) f *= kht; 
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
  murth_event_t event;
  event.samplestamp = current_sample;
  event.value[0] = c->top[0];
  event.type = c->top[1].i;
  nya_stream_write(&sout_events, &event, 1);
  c->top += 2;
}
void i_storesampler(core_t *c) {
  sampler_t *s = samplers + c->top[0].i;
  s->samples[s->cursor] = c->top[1].f;
  s->cursor = (s->cursor + 1) & MAX_SAMPLER_SIZE_MASK;
  ++c->top;
}
void i_loadsampler(core_t *c) {
  sampler_t *s = samplers + c->top[0].i;
  c->top[1].f = s->samples[(s->cursor - c->top[1].i) & MAX_SAMPLER_SIZE_MASK];
  ++c->top;
}

//#if defined(DEBUG)
//#undef DEBUG
//#define DEBUG 0
//#endif
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
      if (current_sample_instructions++ >= instruction_limit) return;
      c->samples_to_idle = 0;
      program[c->pcounter++].func(c);
    }
    COREDUMP(c);
    --c->samples_to_idle;
    if (c->ttl == 0 || ((c->ttl > 0) && ((--c->ttl) == 0))) c->pcounter = -1;
  }
}

static void reset() {
  current_sample = 0;
  for (int i = 0; i < CORES; ++i) cores[i].pcounter = -1;
  if (program != NULL) spawn_core(0, 0, NULL);
  running = 0;
}

static int process_message(murth_control_message_t *msg) {
  switch(msg->type) {
    case SetProgram:
      if (msg->ptr != NULL) {
        const program_t *p =(const program_t*)(msg->ptr);
        program = p->program;
        midi_cc_handler = p->midi_cc_handler;
        midi_note_handler = p->midi_note_handler;
        reset();
      } else {
        program = NULL;
        midi_cc_handler = midi_note_handler = -1;
      }
    case Reset:
      reset();
      break;
    case SetBPM:
      samples_in_tick = (samplerate * 60) / (16 * msg->param);
      break;
    case SetInstructionLimit:
      instruction_limit = msg->param;
      break;
    default:
      return 0;
  }
  return 1;
}

static int send_message(int type, int param, void *ptr) {
  murth_control_message_t msg;
  msg.type = type;
  msg.param = param;
  msg.ptr = ptr;
  if (running == 1) return nya_stream_write(&sin_messages, &msg, 1);
  else return process_message(&msg);
}

static void process_messages() {
  murth_control_message_t msg;
  while (1 == nya_stream_read(&sin_messages, &msg, 1)) process_message(&msg);
}

/* \todo
static void run_note(int channel, int note, int velocity) {
  if (midi_note_handler == -1) return;
  var_t params[3];
  params[0].i = note; params[1].i = velocity; params[2].i = channel;
  spawn_core(midi_note_handler, 3, params);
}

static void run_control(int channel, int control, int value) {
  if (midi_cc_handler == -1) return;
  var_t params[3];
  params[0].i = value; params[1].i = control; params[2].i = channel;
  spawn_core(midi_note_handler, 3, params);
}
*/

static void process_midi_stream() {
  /* \todo const u8 *p = (u8*)packet;
  for (int i = 0; i < bytes-2; ++i)
    switch(p[i] & 0xf0) {
      case 0x90: run_note(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // note on
      case 0xb0: run_control(p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // control change
    }*/
}

int murth_synthesize(float *ptr, int count) {
  if (running == -1) return 0;
  running = 1;
  process_messages();
  process_midi_stream();
  for (int i = 0; i < count; ++i) {
    current_sample_instructions = 0;
    globals[0].i = 0;
    for (int j = CORES - 1; j >= 0; --j) run_core(cores + j);
    ptr[i] = globals[0].f;
    ++current_sample;
  }
  return current_sample;
}

void murth_init(int in_samplerate) {
  samplerate = in_samplerate;
  program = 0;
  nya_stream_init(&sin_midi);
  nya_stream_init(&sout_events);
  nya_stream_init(&sin_messages);
  reset();
}

void murth_set_bpm(int bpm) {
  send_message(SetBPM, bpm, NULL);
}

void murth_set_program(murth_program_t prog) {
  send_message(SetProgram, 0, prog);
}

void murth_set_instruction_limit(int max_per_sample) {
  send_message(SetInstructionLimit, max_per_sample, NULL);
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
#endif


void murth_process_raw_midi(const void *packet, int bytes) {
  const int full_chunks = bytes / NYA_STREAM_CHUNK_SIZE;
  nya_stream_write(&sin_midi, packet, full_chunks);
  const int left_bytes = bytes % NYA_STREAM_CHUNK_SIZE;
  if (left_bytes > 0) {
    const char *p = (const char*)packet + full_chunks * NYA_STREAM_CHUNK_SIZE;
    nya_chunk_t chunk;
    memcpy(&chunk, p, left_bytes);
    nya_stream_write(&sin_midi, &chunk, 1);
  }
}

int murth_get_event(murth_event_t *event) {
  return nya_stream_read(&sout_events, event, 1);
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
  I(storesampler), I(loadsampler)
#undef I
};


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
#if DEBUG
      p->program[program_counter].instr_index = get_instruction(token);
#endif
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

#if DEBUG
  for (int i = 0; i < program_counter; ++i) {
    var_t v; v.i = p->program[i].instr_index;
    const char *fname = (v.i >= 0 &&
                         v.i < sizeof(instructions)/sizeof(*instructions)) ?
                        instructions[v.i].name : "%%invalid%%";
    fprintf(stderr, "0x%08x: %08x (%d, %f, <%s>)\n", i, v.i, v.i, v.f, fname);
  }
#endif

  return 0;
}

const char *murth_program_get_status(murth_program_t program) {
  return "ERROR: NOT IMPLEMENTED";
}

void murth_program_destroy(murth_program_t program) {
  free(program);
}

