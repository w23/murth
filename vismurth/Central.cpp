#include <cstdlib>
#include "Central.h"
#include "Halfedge.h"

#define _A (.5)
#define _B (.8660254f)
const static vec3f sphereSeed[5] = { vec3f(0, 0, 1),
  vec3f(1, 0, 0), vec3f(-_A, _B, 0), vec3f(-_A, -_B, 0), vec3f(0, 0, -1) };
const static int sphereSeedIndices[18] = {
  1, 2, 0, 2, 3, 0, 3, 1, 0, 4, 2, 1, 4, 3, 2, 4, 1, 3 };

Central::Central(int detail, float r) { generateMesh(detail, r); }
void Central::generateMesh(int detail, float r) {
  Halfedge he(sphereSeed, 5, sphereSeedIndices, 18, detail);
  init(he.getVerticesCount(), he.getTrianglesCount());
  const vec3f *v = he.getVertices();
  for (int i = 0; i < he.getVerticesCount(); ++i) raw_vertices_[i].p = v[i].normalized() * r;
  he.writeIndices(raw_indices_);
}
void Central::spike(float r) {
  vertex_t &v = raw_vertices_[rand()%nraw_vertices_];
  v.p += v.p.normalized() * r;
}
void Central::update(float dt) {
  float dl = dt * 8.f;
  for (int i = 0; i < nraw_vertices_; ++i) {
    vertex_t &v = raw_vertices_[i];
    if ((v.p.length_sq() - dl*dl) > 1.f)
      v.p -= v.p.normalized() * dl;
    else
      v.p = v.p.normalized();
  }
  calcNormalsAndUpload();
}