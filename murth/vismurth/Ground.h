#pragma once
#include <kapusha/ooo/Object.h>
using namespace kapusha;
class Ground : public Object {
public:
  Ground();
  ~Ground();
  void update(u32 t, float dt);
private:
  void generatePlane(int detail, float size);
  void calcNormals();
  struct vertex_t { vec3f p, n; };
  int nraw_vertices_;
  vertex_t *raw_vertices_;
  int ntriangles_;
  int *raw_indices_;
  int nvertices_;
  Buffer *vertices_;
};