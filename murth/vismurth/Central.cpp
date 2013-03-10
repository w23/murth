#include "Central.h"
Central::Central(int detail, float r) : detail_(detail), r_(r) {
  /*init(6 * detail_ * detail_ + 4 * detail_,
       12 * detail_ * detail_);*/
}

void Central::generateMesh() {
/*  const int stride = detail_ * 3;
  const float dc = 2.f / detail_;
  vertex_t *p = raw_vertices_;
  for (int y = 0; y <= detail_; ++y, p += stride) {
    float vy = dc * y - 1.f;
    for (int x = 0; x < detail_; ++x, ++p) {
      float vx = dc * x - 1.f;
      p[0].p         = vec3f(  vx, vy,  1.f);
      p[detail_].p   = vec3f( 1.f, vy,  -vx);
      p[detail_*2].p = vec3f( -vx, vy, -1.f);
      p[detail_*3].p = vec3f(-1.f, vy,   vx);
    }
  }
  const int poffset = detail_ * detail_;
  for (int y = 1; y < detail_; ++y) {
    float vy = dc * y - 1.f;
    for (int x = 1; x < detail_; ++x, ++p) {
      float vx = dc * x - 1.f;
      p[0].p = vec3f();
    }
  }*/
}