#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "nyatomic.h"
#include "murth.h"

typedef unsigned char u8;

enum message_type_e {
  Reset,
  SetProgram,
  SetBPM,
  SetInstructionLimit
};

typedef struct {
  int type;
  int param;
  const void *ptr; // can be 64-bit
  int __dummy; // pad to 16 bytes on 32-bit platforms
} murth_control_message_t;

static int running = -1;
int samplerate;
int samples_in_tick;
static int instruction_limit = 1024;
static int current_sample;
static int current_sample_instructions;
static murth_core_t cores[CORES];
murth_var_t globals[GLOBALS];
murth_sampler_t samplers[SAMPLERS];
static nya_stream_t sin_midi;
static nya_stream_t sout_events;
static nya_stream_t sin_messages;
static int midi_cc_handler;
static int midi_note_handler;
const murth_op_t *program;

murth_core_t *spawn_core(int pcounter, int argc, const murth_var_t *argv) {
  for (int i = 0; i < CORES; ++i)
    if (cores[i].pcounter < 0) {
      murth_core_t *nc = cores + i;
      nc->samples_to_idle = 0;
      nc->ttl = 1;
      nc->pcounter = pcounter;
      nc->top = nc->stack + STACK_SIZE - argc;
      memcpy(nc->top, argv, sizeof(murth_var_t) * argc);
      return nc;
    }
  return NULL;
}

void emit_event(int type, murth_var_t value) {
  murth_event_t event;
  event.samplestamp = current_sample;
  event.value[0] = value;
  event.type = type;
  nya_stream_write(&sout_events, &event, 1);
}

static void run_core(murth_core_t *c) {
  if (c->pcounter >= 0) {
    while(c->samples_to_idle <= 0) {
      if (current_sample_instructions++ >= instruction_limit) return;
      c->samples_to_idle = 0;
      program[c->pcounter++].func(c);
    }
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
        const murth_program_t *p =(const murth_program_t*)(msg->ptr);
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

static void run_note(int sync, int channel, int note, int velocity) {
  if (sync == 0) return; /// \todo implement async midi
  if (midi_note_handler == -1) return;
  murth_var_t params[3];
  params[0].i = note; params[1].i = velocity; params[2].i = channel;
  spawn_core(midi_note_handler, 3, params);
}

static void run_control(int sync, int channel, int control, int value) {
  if (sync == 0) return; /// \todo implement async midi
  if (midi_cc_handler == -1) return;
  murth_var_t params[3];
  params[0].i = value; params[1].i = control; params[2].i = channel;
  spawn_core(midi_cc_handler, 3, params);
}

static void process_midi_stream() {
  //! \fixme DO process midi stream. Useless without it.
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

void murth_set_program(murth_program_t *prog) {
  send_message(SetProgram, 0, prog);
}

void murth_set_instruction_limit(int max_per_sample) {
  send_message(SetInstructionLimit, max_per_sample, NULL);
}

void murth_process_raw_midi(const void *packet, int bytes, int sync) {
  const u8 *p = (u8*)packet;
  for (int i = 0; i < bytes-2; ++i)
    switch(p[i] & 0xf0) {
      case 0x90:
	run_note(sync, p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // note on
      case 0xb0:
	run_control(sync, p[i] & 0x0f, p[i+1], p[i+2]); i += 2; break; // control change
    }

  /*  const int full_chunks = bytes / NYA_STREAM_CHUNK_SIZE;
    nya_stream_write(&sin_midi, packet, full_chunks);
    const int left_bytes = bytes % NYA_STREAM_CHUNK_SIZE;
    if (left_bytes > 0) {
      const char *p = (const char*)packet + full_chunks * NYA_STREAM_CHUNK_SIZE;
      nya_chunk_t chunk;
      memcpy(&chunk, p, left_bytes);
      nya_stream_write(&sin_midi, &chunk, 1);
    } */
}

int murth_get_event(murth_event_t *event) {
  return nya_stream_read(&sout_events, event, 1);
}

