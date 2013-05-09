#include <math.h>
#include <string.h>
#include "instructions.h"
#include "vm.h"

#define MAX_INSTRUCTIONS 256

//! \todo get rid of this nonlocality
int samplerate;
int samples_in_tick;
murth_var_t globals[GLOBALS];
murth_sampler_t samplers[SAMPLERS];
extern const murth_op_t *program;
murth_core_t *spawn_core(int pcounter, int argc, const murth_var_t *argv);
void emit_event(int type, murth_var_t value);

typedef struct {
  const char *name;
  murth_instr_func_t func;
} instruction_t;

static void i_load(murth_core_t *c) { (--c->top)->i = program[c->pcounter++].param; }
static void i_load0(murth_core_t *c) { (--c->top)->i = 0; }
static void i_load1(murth_core_t *c) { (--c->top)->i = 1; }
static void i_fload1(murth_core_t *c) { (--c->top)->f = 1.f; }
static void i_idle(murth_core_t *c) { c->samples_to_idle = (c->top++)->i + 1; }
static void i_jmp(murth_core_t *c) { c->pcounter = (c->top++)->i; }
static void i_jmpnz(murth_core_t *c) {
  if (c->top[1].i != 0) c->pcounter = c->top[0].i;
  c->top += 2;
}
static void i_yield(murth_core_t *c) { c->samples_to_idle = 1; }
static void i_ret(murth_core_t *c) { c->pcounter = -1, c->samples_to_idle = 1; }
static void i_spawn(murth_core_t *c) {
  const int args = c->top[1].i;
  spawn_core(c->top[0].i, args, c->top + 2);
  c->top += 2 + args;
}
static void i_storettl(murth_core_t *c) { c->ttl = c->top[0].i, ++c->top; }
static void i_loadglobal(murth_core_t *c) { c->top->i = globals[c->top->i].i; }
static void i_storeglobal(murth_core_t *c) { globals[c->top->i].i = c->top[1].i; ++c->top; }
static void i_clearglobal(murth_core_t *c) { globals[c->top[0].i].i = 0; ++c->top; }
static void i_faddglobal(murth_core_t *c) { globals[c->top[0].i].f += c->top[1].f; ++c->top; }
static void i_dup(murth_core_t *c) { --c->top; c->top->i = c->top[1].i; }
static void i_get(murth_core_t *c) { *c->top = c->top[c->top->i]; }
static void i_put(murth_core_t *c) { c->top[c->top->i] = c->top[1]; ++c->top; }
static void i_pop(murth_core_t *c) { ++c->top; }
static void i_swap(murth_core_t *c) { murth_var_t v = c->top[0]; c->top[0] = c->top[1]; c->top[1] = v; }
static void i_swap2(murth_core_t *c) {
  murth_var_t v = c->top[0]; c->top[0] = c->top[2]; c->top[2] = v;
  v = c->top[1]; c->top[1] = c->top[3]; c->top[3] = v;
}
static void i_fsign(murth_core_t *c) { c->top->f = c->top->f < 0 ? -1.f : 1.f; }
static void i_fadd(murth_core_t *c) { c->top[1].f += c->top[0].f; ++c->top; }
static void i_faddnp(murth_core_t *c) { c->top[0].f += c->top[1].f; }
static void i_fsub(murth_core_t *c) { c->top[1].f -= c->top[0].f; ++c->top; }
static void i_fmul(murth_core_t *c) { c->top[1].f *= c->top->f, ++c->top; }
static void i_fdiv(murth_core_t *c) { c->top[1].f /= c->top->f, ++c->top; }
static void i_fsin(murth_core_t *c) { c->top->f = sinf(c->top->f * 3.1415926f); }
static void i_fclamp(murth_core_t *c) {
  if (c->top->f < -1.f) c->top->f = -1.f;
  if (c->top->f > 1.f) c->top->f = 1.f;
}
static void i_fphase(murth_core_t *c) {
  c->top->f = fmodf(c->top->f + 1.f, 2.f) - 1.f;
}
static void i_int127(murth_core_t *c) { c->top->i = c->top->f * 127; }
static void i_int(murth_core_t *c) { c->top->i = c->top->f; }
static void i_in2dp(murth_core_t *c) {
  int n = c->top->i;
  float kht = 1.059463f; //2^(1/12)
  float f = 55.f / (float)samplerate;
// 'cheap' pow!
  while(n >= 12) { f *= 2; n -= 12; }
  while(--n>=0) f *= kht; 
  c->top->f = f;
}
static void i_float127(murth_core_t *c) { c->top->f = c->top->i / 127.f; }
static void i_float(murth_core_t *c) { c->top->f = c->top->i; }
static void i_iadd(murth_core_t *c) { c->top[1].i += c->top->i, ++c->top; }
static void i_iinc(murth_core_t *c) { c->top[0].i++; }
static void i_idec(murth_core_t *c) { c->top[0].i--; }
static void i_imul(murth_core_t *c) { c->top[1].i *= c->top->i; ++c->top; }
static void i_imulticks(murth_core_t *c) { c->top->i *= samples_in_tick; }
static void i_fmulticks(murth_core_t *c) { c->top->f *= samples_in_tick; }
static void i_noise(murth_core_t *c) {
  static int seed = 127; seed *= 16807;
  (--c->top)->i = ((unsigned)seed >> 9) | 0x3f800000u;
  c->top[0].f -= 1.0f;
}
static void i_abs(murth_core_t *c) { c->top->i &= 0x7fffffff; }
static void i_emit(murth_core_t *c) {
  emit_event(c->top[1].i, c->top[0]);
  c->top += 2;
}
static void i_storesampler(murth_core_t *c) {
  murth_sampler_t *s = samplers + c->top[0].i;
  s->samples[s->cursor] = c->top[1].f;
  s->cursor = (s->cursor + 1) & MAX_SAMPLER_SIZE_MASK;
  ++c->top;
}
static void i_loadsampler(murth_core_t *c) {
  murth_sampler_t *s = samplers + c->top[0].i;
  c->top[1].f = s->samples[(s->cursor - c->top[1].i) & MAX_SAMPLER_SIZE_MASK];
  ++c->top;
}

static instruction_t instructions[] = {
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
const int builtin_count = sizeof(instructions)/sizeof(*instructions);

murth_instr_func_t murth_instr_get_func(const char *name) {
  for (int i = 0; i < builtin_count; ++i)
    if (strcmp(name, instructions[i].name) == 0) return instructions[i].func;
  return NULL;
}

const char *murth_instr_get_name(int index) {
  if (index >= 0 && index < builtin_count) return instructions[index].name;
  return NULL;
}

int murth_instr_register(const char *name, murth_instr_func_t func) {
  //! \todo implement adding instructions
  return 0;
}
