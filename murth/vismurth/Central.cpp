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
  calcNormalsAndUpload();
}