#include <cstring>
#include <kapusha/core/Core.h>
#include "Halfedge.h"
Halfedge::Halfedge(const vec3f *vertices, int nvertices,
                   const int *indices, int nindices,
                   int subdivisions)
  : nvertices_(0), nedges_(0), ntriangles_(0) {
  KP_ASSERT(nindices%3 == 0);
  allocated_triangles_ = (nindices / 3) << (4 * subdivisions); // exact
  allocated_edges_ = allocated_triangles_ * 3; // exact
  allocated_vertices_ = allocated_triangles_ * 3; // worst case
  vertices_ = new vec3f[allocated_vertices_];
  edges_ = new edge_t[allocated_edges_];
  triangles_ = new int[allocated_triangles_];
  initWithGeometry(vertices, nvertices, indices, nindices);
  while (subdivisions--) calcSubdivision();
}
Halfedge::~Halfedge() {
  delete[] triangles_;
  delete[] edges_;
  delete[] vertices_;
}
void Halfedge::initWithGeometry(const vec3f *vertices, int nvertices,
                                const int *indices, int nindices) {
  nvertices_ = nvertices, nedges_ = 0, ntriangles_ = 0;
  memcpy(vertices_, vertices, sizeof(*vertices_) * nvertices); // copy vertices
  const int *pi = indices;
  for (int i = 0; i < nindices; i+=3, pi+=3) // add triangles
    triangleAdd(edgeAdd(pi[0], pi[1]),
                edgeAdd(pi[1], pi[2]),
                edgeAdd(pi[2], pi[0]));
  for (int i = 0; i < nedges_; ++i) { // link reverses
    edge_t &e = edges_[i];
    if (e.reverse == -1) {
      for (int j = i; j < nedges_; ++j) {
        edge_t &r = edges_[j];
        if (e.start == r.end && r.start == e.end) {
          KP_ASSERT(r.reverse == -1);
          r.reverse = i, e.reverse = j; break;
        }
      }
    }
  }
}
int Halfedge::vertexAdd(vec3f vertex) {
  vertices_[nvertices_] = vertex;
  return nvertices_++;
}
int Halfedge::edgeAdd(int vertex_begin, int vertex_end) {
  edge_t &e = edges_[nedges_];
  e.next = e.reverse = e.reverse_half = -1;
  e.start = vertex_begin, e.end = vertex_end;
  return nedges_++;
}
int Halfedge::edgeInsert(int edge_prev, int edge_next) {
  int e = edgeAdd(edges_[edge_prev].end, edges_[edge_next].start);
  edges_[edge_prev].next = e, edges_[e].next = edge_next;
  return e;
}
int Halfedge::edgeReversed(int edge) {
  edge_t &e = edges_[edge];
  if (e.reverse == -1) {
    e.reverse = edgeAdd(e.end, e.start);
    edges_[e.reverse].reverse = edge;
  }
  return e.reverse;
}
int Halfedge::triangleAdd(int edge_first) {
  triangles_[ntriangles_] = edge_first;
  return ntriangles_++;
}
int Halfedge::triangleAdd(int edge0, int edge1, int edge2) {
  edges_[edge0].next = edge1, edges_[edge1].next = edge2, edges_[edge2].next = edge0;
  return triangleAdd(edge0);
}
int Halfedge::edgeSplit(int edge) {
  edge_t &e = edges_[edge];
  if (e.reverse_half == -1) { // reverse is not split yet
    int pmiddle = vertexAdd((vertices_[e.start] + vertices_[e.end]) * .5);
    int enext = edgeAdd(pmiddle, e.end);
    edges_[enext].next = e.next, e.next = enext;
    e.end = pmiddle;
    if (e.reverse != -1) {
      edges_[e.reverse].reverse_half = enext, edges_[enext].reverse = e.reverse;
    }
  } else {
    int pmiddle = edges_[e.reverse].end;
    int enext = edgeAdd(pmiddle, e.end);
    e.end = pmiddle;
    edges_[enext].next = e.next, e.next = enext;
    edges_[enext].reverse = e.reverse, edges_[e.reverse].reverse = enext;
    e.reverse = e.reverse_half, e.reverse_half = -1;
  }
  return e.next;
}
void Halfedge::triangleSubdivide(int triangle) {
  int e[3], esplit[3], enew[3];
  e[0] = triangles_[triangle], e[1] = edges_[e[0]].next, e[2] = edges_[e[1]].next;
  esplit[0] = edgeSplit(e[0]);
  esplit[1] = edgeSplit(e[1]);
  esplit[2] = edgeSplit(e[2]);
  enew[0] = edgeInsert(e[0], esplit[2]);
  enew[1] = edgeInsert(e[1], esplit[0]);
  enew[2] = edgeInsert(e[2], esplit[1]);
  triangleAdd(e[1]);
  triangleAdd(e[2]);
  triangleAdd(edgeReversed(enew[0]), edgeReversed(enew[1]), edgeReversed(enew[2]));
}
void Halfedge::calcSubdivision() {
  const int tris = ntriangles_;
  for (int i = 0; i < tris; ++i) triangleSubdivide(i);
}
void Halfedge::writeIndices(int *out_indices) const {
  for (int i = 0; i < ntriangles_; ++i) {
    edge_t &e = edges_[triangles_[i]];
    *(out_indices++) = e.start, e = edges_[e.next];
    *(out_indices++) = e.start, e = edges_[e.next];
    *(out_indices++) = e.start;
  }
}
void Halfedge::dump() const {
  printf("Vertices (%d/%d):\n", nvertices_, allocated_vertices_);
  for (int i = 0; i < nvertices_; ++i)
    printf(" %d: (%.2f, %.2f, %.2f)\n", i,
           vertices_[i].x, vertices_[i].y, vertices_[i].z);
  printf("Edges (%d/%d):\n", nedges_, allocated_edges_);
  for (int i = 0; i < nedges_; ++i) printf(" %d: (%d -> %d; N:%d; R:%d; H:%d)\n", i,
                                          edges_[i].start, edges_[i].end, edges_[i].next,
                                          edges_[i].reverse, edges_[i].reverse_half);
  printf("Triangles (%d/%d):\n", ntriangles_, allocated_triangles_);
  for (int i = 0; i < ntriangles_; ++i) {
    printf(" %d: (%d -> %d -> %d) [", i,
           triangles_[i], edges_[triangles_[i]].next,
           edges_[edges_[triangles_[i]].next].next);
    vec3f *p = vertices_ + edges_[triangles_[i]].start;
    printf("(%.2f, %.2f, %.2f), ", p->x, p->y, p->z);
    p = vertices_ + edges_[edges_[triangles_[i]].next].start;
    printf("(%.2f, %.2f, %.2f), ", p->x, p->y, p->z);
    p = vertices_ + edges_[edges_[edges_[triangles_[i]].next].next].start;
    printf("(%.2f, %.2f, %.2f)]\n", p->x, p->y, p->z);
  }
}