#pragma once
#include <kapusha/ooo/Object.h>
using namespace kapusha;
class Ground : public Object {
public:
  Ground(int detail, float size);
  ~Ground();
  void spike(vec3f posH);
  void update(u32 t, float dt);
private:
  void generatePlane();
  void calcNormals();
  struct vertex_t { vec3f p, n; };
  int detail_;
  float size_;
  int nraw_vertices_;
  vertex_t *raw_vertices_;
  float *raw_velocities_;
  int ntriangles_;
  int *raw_indices_;
  int nvertices_;
  Buffer *vertices_;
};