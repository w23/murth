#include "Ground.h"

Ground::Ground(int detail, float size) : detail_(detail), size_(size) {
  init((detail_ + 1) * (detail_ + 1), detail_ * detail_ * 2);
  generatePlane();
}

Ground::~Ground() {
  delete raw_velocities_;
}

void Ground::update(u32 t, float dt) {
  int stride = detail_ + 1;
  if (dt > .016) dt = .016;
  for (int y = 1; y < detail_; ++y) {
    vertex_t *pvtx = raw_vertices_ + stride * y + 1;
    float *pvel = raw_velocities_ + stride * y + 1;
    for (int x = 1; x < detail_; ++x, ++pvtx, ++pvel) {
      float a = 0;
      a += pvtx[-stride].p.z - pvtx[0].p.z;
      a += pvtx[+stride].p.z - pvtx[0].p.z;
      a += pvtx[-1].p.z - pvtx[0].p.z;
      a += pvtx[+1].p.z - pvtx[0].p.z;
      pvel[0] += a * dt * 4.f;
      pvtx[0].p.z += pvel[0] * dt;
      pvel[0] *= .995f;
    }
  }  
  calcNormalsAndUpload();
}

void Ground::generatePlane() {
  raw_velocities_ = new float[nraw_vertices_];
  vertex_t *p = raw_vertices_;
  float *pv = raw_velocities_;
  const float offset = - detail_ * size_ * .5;
  for (int y = 0; y <= detail_; ++y) {
    float vy = offset + size_ * y;
    for (int x = 0; x <= detail_; ++x, ++p, ++pv) {
      p->p.x = offset + size_ * x;
      p->p.y = vy;
      p->p.z = (x!=0 && y!=0 && x!=detail_ && y!=detail_)?frand(-size_, size_):0;
      *pv = 0;
    }
  }
  int *pi = raw_indices_;
  int index = 0;
  for (int y = 0; y < detail_; ++y, ++index)
    for (int x = 0; x < detail_; ++x, ++index) {
      *(pi++) = index, *(pi++) = index + 1, *(pi++) = index + detail_ + 2;
      *(pi++) = index, *(pi++) = index + detail_ + 2, *(pi++) = index + detail_ + 1;
    }
}

void Ground::spike(vec3f posH) {
  vec2i pos = posH.xy() / size_ + vec2i(detail_ / 2);
  pos.x = clamp(pos.x, 0, detail_);
  pos.y = clamp(pos.y, 0, detail_);
  raw_vertices_[pos.x + pos.y * (detail_ + 1)].p.z
    = max(raw_vertices_[pos.x + pos.y * (detail_ + 1)].p.z, posH.z);
}