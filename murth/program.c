#include "synth.h"

#define tick 10000

ifu probdata[4][4096];
probe probes[4] = {
  {0, 4096, probdata[0]},
  {0, 4096, probdata[1]},
  {0, 4096, probdata[2]},
  {0, 4096, probdata[3]},
};

u8 m0[8] = {17, 17, 15, 15, 12, 12, 10, 10};
unsigned short dm0[8] = {0, tick-2, 0, tick-2, 0, tick-2, 0, tick-2};

u8 e0[2] = {127, 0};
unsigned short de0[2] = {100, tick-102};

u8 m1[8] = {22, 22, 17, 17, 15, 15, 12, 12};

u8 m2[18] = {29, 29, 34, 34, 32, 32, 36, 36, 34, 34, 39, 39, 36, 36, 34, 34, 32, 32};
u8 e1[18] = {127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0};
unsigned short dm2[18] = {
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*3 - 2,
  0, tick - 2,
  0, tick - 2,
  0, tick - 2,
  0, tick*2 - 2,
};

paramtbl paramtbls[] = {
  {8, m0, dm0}, {2, e0, de0},
  {8, m1, dm0}, {18, m2, dm2},
  {18, e1, dm2}
};
int nparamtbls = 5;

const unsigned char program[] =
{
  // m0
  0, LDG, 0, LDP, FIU, 36, IADD, NDP, ADD, PHR, 0, STG,      1, LDP, PROBE(0), MUL,
  65, LDP, MUL,
  
  // m1
  1, LDG, 2, LDP, FIU, 24, IADD, NDP, ADD, PHR, 1, STG, SIN, 1, LDP, PROBE(1), MUL,
  66, LDP, MUL,
  
  // m0 + m1
  ADD,

  // delay (m0 + m1)
  LOAD_SHORT(tick*1.5),
  0, ROF, POP, 0, RRD, 68, LDP, MUL, ADD, 0, RWR,
  
  // m2
  2, LDG, 3, LDP, FIU, NDP, ADD, PHR, 2, STG, SGN, 4, LDP, PROBE(2), MUL,
  67, LDP, MUL,
  
  // delay(m2)
  LOAD_SHORT(tick*3),
  1, ROF, POP, 1, RRD, 69, LDP, MUL, ADD, 1, RWR,
  
  // sum all
  ADD,
  
  // postfx
  70, LDP, FIU, 71, LDP, FIU, IMUL,
  2, ROF, POP, 2, RRD, 72, LDP, MUL, ADD, 2, RWR,
  
  //2, LDG, 4, LDP, FIU, /*24, IADD,*/ NDP, ADD, PHR, 2, STG, SGN, /*1, LDP, MUL,*/
  //67, LDP, MUL,
  //ADD,
  
  63, IFU, MUL,
  
  CLAMP, PROBE(3), RET
};
