#pragma once
#include <kapusha/ooo/Object.h>
using namespace kapusha;
class FlatShadedMesh : public Object {
public:
  ~FlatShadedMesh();
  void init(int nraw_vertices, int ntriangles);
protected:
  struct vertex_t { vec3f p, n, v; };
  int nraw_vertices_;
  vertex_t *raw_vertices_;
  int ntriangles_;
  int *raw_indices_;
  void calcNormalsAndUpload();
  int nvertices_;
  Buffer *vertices_;
};