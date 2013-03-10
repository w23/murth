#include "Halfedge.h"
Halfedge::Halfedge(int triangles, int subdivisions)
: subdivision_(subdivisions), allocated_triangles_(triangles) {
  allocated_edges_ = allocated_triangles_ * 3;
  allocated_vertices_ = allocated_triangles_;
  vertices_ = new vec3f[allocated_vertices_];
  edges_ = new edge_t[allocated_edges_];
  triangles_ = new int[allocated_triangles_];
}
Halfedge::~Halfedge() {
  delete[] triangles_;
  delete[] edges_;
  delete[] vertices_;
}
void Halfedge::setVertices(const vec3f *in_vertices, int count) {
}
int Halfedge::addVertex(vec3f vertex) {
  vertices_[nvertices_] = vertex;
  return nvertices_++;
}
int Halfedge::addHalfEdge(int vertex_begin, int vertex_end) {
  edge_t &e = edges_[nedges_];
  e.next = e.reverse = e.reverse_half = -1;
  e.start = vertex_begin, e.end = vertex_end;
  return nedges_++;
}
int Halfedge::edgeReversed(int edge) {
  edge_t &e = edges_[edge];
  if (e.reverse == -1) {
    e.reverse = addHalfEdge(e.end, e.start);
    edges_[e.reverse].reverse = edge;
  }
  return e.reverse;
}
int Halfedge::addTriangle(int edge0, int edge1, int edge2) {
  edges_[edge0].next = edge1, edges_[edge1].next = edge2, edges_[edge3].next = edge0;
  return triangleAdd(edge0);
}
int Halfedge::edgeSplit(int edge) {
  edge_t *e = edges_ + edge;
  if (e->reverse_half == -1) { // reverse is not split yet
    int middle = point_add_middle(e->start, e->end);
    int next = edge_add_with_points(middle, e->end);
    edges[next].next = e->next, e->next = next;
    e->end = middle;
    if (e->reverse != -1) {
      edges[e->reverse].reverse_half = next;
      edges[next].reverse = e->reverse;
    }
  } else {
    int middle = edges[e->reverse].end;
    int next = edge_add_with_points(middle, e->end);
    e->end = middle;
    edges[next].next = e->next, e->next = next;
    edges[next].reverse = e->reverse;
    edges[e->reverse].reverse = next;
    e->reverse = e->reverse_half;
    e->reverse_half = -1;
  }
  return edges[edge].next;
}
int Halfedge::edgeAdd(int edge_prev, int edge_next) {
  int e = edge_add_with_points(edges[edge_prev].end, edges[edge_next].start);
  edges[edge_prev].next = e, edges[e].next = edge_next;
  return e;
}
int Halfedge::triangleAdd(int edge_first) {
  triangles[ntriangles].edge_first = edge_first;
  return ntriangles++;
}
void Halfedge::triangleSubdivide(int triangle) {
  int e[3], esplit[3], enew[3];
  e[0] = triangles[triangle].edge_first, e[1] = edges[e[0]].next, e[2] = edges[e[1]].next;
  triangle_t *tri = triangles + triangle;
  esplit[0] = edge_split(e[0]);
  esplit[1] = edge_split(e[1]);
  esplit[2] = edge_split(e[2]);
  enew[0] = edge_add(e[0], esplit[2]);
  enew[1] = edge_add(e[1], esplit[0]);
  enew[2] = edge_add(e[2], esplit[1]);
  triangle_add(e[1]);
  triangle_add(e[2]);
  triangle_add_edges(edge_reverse(enew[0]), edge_reverse(enew[1]), edge_reverse(enew[2]));
}
void Halfedge::calcSubdivision() {
  for (int i = 0; i < subdivision_; ++i) {
    const int tris = ntriangles_;
    for (int i = 0; i < tris; ++i)
      triangleSubdivide(i);
  }
}
void Halfedge::writeIndices(int *out_indices) const {
}

