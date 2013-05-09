#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "program.h"
#include "instructions.h"

static int get_name(const char *name, murth_name_t *names, int max)
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

static int get_global(murth_program_t *p, const char *name) {
  return get_name(name, p->global_names, GLOBALS - 1) + 1; // 0 is output
}
static int get_sampler(murth_program_t *p, const char *name) {
  return get_name(name, p->sampler_names, SAMPLERS);
}
static int get_label_address(murth_program_t *p, const char *name, int for_position) {
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

static int set_label(murth_program_t *p, const char *name, int position) {
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

int murth_program_compile(murth_program_t *p, const char *source) {
  memset(p, 0, sizeof(murth_program_t));
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

    if (program_counter >= (MAX_PROGRAM_LENGTH-2)) {
      fprintf(stderr, "Wow, dude, your program is HUGE!11\n");
      return -1;
    }

    const murth_instr_func_t i_load = murth_instr_get_func("load");

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
      p->program[program_counter++].func = murth_instr_get_func(token);
    } else if ((typehint&(DIGIT|DOT)) == (DIGIT|DOT)) { // float constant (no alpha, but digits with a dot)
      murth_var_t v;
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

  return 0;
}

