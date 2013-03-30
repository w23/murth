#include <stdio.h>
typedef struct { float x, y, z; } point_t;
typedef struct {
  int next, reverse, reverse_half;
  int start, end;
} edge_t;
typedef struct { int edge_first; } triangle_t;

#define MAX_TRIANGLES 1024
#define MAX_EDGES (MAX_TRIANGLES*3)
#define MAX_POINTS 1024

triangle_t triangles[MAX_TRIANGLES];
edge_t edges[MAX_EDGES];
point_t points[MAX_POINTS];

int nedges = 0;
int ntriangles = 0;
int npoints = 0;

int point_add(float x, float y, float z) {
  points[npoints].x = x, points[npoints].y = y, points[npoints].z = z;
  return npoints++;
}
int point_add_middle(int p0, int p1) {
  return point_add((points[p0].x + points[p1].x) * .5f,
                   (points[p0].y + points[p1].y) * .5f,
                   (points[p0].z + points[p1].z) * .5f);
}
int edge_add_with_points(int start, int end) {
  int e = nedges;
  edges[e].next = edges[e].reverse = edges[e].reverse_half = -1;
  edges[e].start = start, edges[e].end = end;
  ++nedges;
  return e;
}
int edge_split(int edge) {
  edge_t *e = edges + edge;
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
int edge_add(int edge_prev, int edge_next) {
  int e = edge_add_with_points(edges[edge_prev].end, edges[edge_next].start);
  edges[edge_prev].next = e, edges[e].next = edge_next;
  return e;
}
int edge_reverse(int edge) {
  if (edges[edge].reverse == -1) {
    edges[edge].reverse = edge_add_with_points(edges[edge].end, edges[edge].start);
    edges[edges[edge].reverse].reverse = edge;
  }
  return edges[edge].reverse;
}
int triangle_add(int edge_first) {
  triangles[ntriangles].edge_first = edge_first;
  return ntriangles++;
}
int triangle_add_edges(int edge_first, int edge_second, int edge_third) {
  edges[edge_first].next = edge_second, edges[edge_second].next = edge_third, edges[edge_third].next = edge_first;
  return triangle_add(edge_first);
}
void triangle_subdivide(int triangle) {
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
void print_data() {
  printf("Points (%d):\n", npoints);
  for (int i = 0; i < npoints; ++i)
    printf(" %d: (%.2f, %.2f, %.2f)\n", i, points[i].x, points[i].y, points[i].z);
  printf("Edges (%d):\n", nedges);
  for (int i = 0; i < nedges; ++i) printf(" %d: (%d -> %d; N:%d; R:%d; H:%d)\n", i,
                                          edges[i].start, edges[i].end, edges[i].next,
                                          edges[i].reverse, edges[i].reverse_half);
  printf("Triangles (%d):\n", ntriangles);
  for (int i = 0; i < ntriangles; ++i) {
    printf(" %d: (%d -> %d -> %d) [", i,
           triangles[i].edge_first, edges[triangles[i].edge_first].next,
           edges[edges[triangles[i].edge_first].next].next);
    point_t *p = points + edges[triangles[i].edge_first].start;
    printf("(%.2f, %.2f, %.2f), ", p->x, p->y, p->z);
    p = points + edges[edges[triangles[i].edge_first].next].start;
    printf("(%.2f, %.2f, %.2f), ", p->x, p->y, p->z);
    p = points + edges[edges[edges[triangles[i].edge_first].next].next].start;
    printf("(%.2f, %.2f, %.2f)]\n", p->x, p->y, p->z);
  }
}
void subdivide() {
  int tris = ntriangles;
  for (int i = 0; i < tris; ++i)
    triangle_subdivide(i);
}
/*int main(int argc, char *argv[]) {
  int p[4], e[6], t[2];
  p[0] = point_add(-1,  0, 0);
  p[1] = point_add( 1,  0, 0);
  p[2] = point_add( 0,  1, 0);
  p[3] = point_add( 0, -1, 0);
  e[0] = edge_add_with_points(p[0], p[1]);
  e[1] = edge_add_with_points(p[1], p[2]);
  e[2] = edge_add_with_points(p[2], p[0]);
  e[3] = edge_reverse(e[0]);
  e[4] = edge_add_with_points(p[0], p[3]);
  e[5] = edge_add_with_points(p[3], p[1]);
  t[0] = triangle_add_edges(e[0], e[1], e[2]);
  t[0] = triangle_add_edges(e[3], e[4], e[5]);
  printf("--------BEFORE:\n");
  print_data();
  subdivide();
  subdivide();
  printf("\n--------AFTER:\n");
  print_data();
  return 0;
}*/
