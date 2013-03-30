#pragma once
#include <kapusha/math/types.h>
using kapusha::vec3f;
class Halfedge {
public:
  Halfedge(const vec3f *vertices, int nvertices,
           const int *indices, int nindices,
           int subdivisions);
  ~Halfedge();
  inline int getVerticesCount() const { return nvertices_; }
  inline int getTrianglesCount() const { return ntriangles_; }
  inline int getIndicesCount() const { return ntriangles_ * 3; }
  inline const vec3f *getVertices() const { return vertices_; }
  void writeIndices(int *out_indices) const;
private:
  struct edge_t {
    int next, reverse, reverse_half;
    int start, end;
  };
  vec3f *vertices_;
  edge_t *edges_;
  int *triangles_;
  int nvertices_, allocated_vertices_;
  int nedges_, allocated_edges_;
  int ntriangles_, allocated_triangles_;
  int edgeSplit(int edge);
  int edgeInsert(int edge_prev, int edge_next);
  int triangleAdd(int edge_first);
  void triangleSubdivide(int triangle);
  int vertexAdd(vec3f vertex);
  int edgeAdd(int vertex_begin, int vertex_end);
  int edgeReversed(int edge);
  int triangleAdd(int edge0, int edge1, int edge2);
  //! \todo bool checkValidity();
  void initWithGeometry(const vec3f *vertices, int nvertices,
                        const int *indices, int nindices);
  void calcSubdivision();
  void dump() const;
};