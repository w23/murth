#include "Ground.h"

static const char shader_vertex[] =
"attribute vec3 av3_pos, av3_normal;\n"
"uniform mat4 um4_mvp;\n"
"varying vec3 vv3_normal;\n"
"void main() {\n"
  "gl_Position = um4_mvp * vec4(av3_pos, 1.);\n"
  "vv3_normal = av3_normal;\n"
"}\n";
static const char shader_fragment[] =
"varying vec3 vv3_normal;\n"
"void main() {\n"
  "gl_FragColor = vec4(vv3_normal, 1.) + vec4(.5);\n"
"}\n";

Ground::Ground() {
  Program *p = new Program(shader_vertex, shader_fragment);
  Material *m = new Material(p);
  Batch *b = new Batch(m);
  vertices_ = new Buffer();
  generatePlane(64, 1.f);
  vertices_->alloc(nvertices_ * sizeof(vertex_t), Buffer::StreamDraw);
  b->setGeometry(Batch::GeometryTriangleList, 0, nvertices_);
  update(0, 0);
  b->setAttribSource("av3_pos", vertices_,
                     3, offsetof(vertex_t, p), sizeof(vertex_t));
  b->setAttribSource("av3_normal", vertices_,
                     3, offsetof(vertex_t, n), sizeof(vertex_t));
  addBatch(b);
}

Ground::~Ground() {
  delete raw_vertices_;
  delete raw_indices_;
}

void Ground::update(u32 t, float dt) {
  calcNormals();
  
  vertex_t *bv = static_cast<vertex_t*>(vertices_->map(Buffer::WriteOnly));
  int *p = raw_indices_;
  for (int i = ntriangles_; i > 0; --i, bv+=3) {
    vertex_t &v0 = raw_vertices_[*(p++)],
             &v1 = raw_vertices_[*(p++)],
             &v2 = raw_vertices_[*(p++)];
    vec3f normal((v0.n + v1.n + v2.n).normalize());
    bv[0].p = v0.p, bv[0].n = normal;
    bv[1].p = v1.p, bv[1].n = normal;
    bv[2].p = v2.p, bv[2].n = normal;
  }
  vertices_->unmap();
}

void Ground::generatePlane(int detail, float size)
{
  ntriangles_ = detail * detail * 2;
  nvertices_ = ntriangles_ * 3;
  nraw_vertices_ = (detail + 1) * (detail + 1);
  raw_vertices_ = new vertex_t[nraw_vertices_];
  vertex_t *p = raw_vertices_;
  const float offset = - detail * size * .5;
  for (int y = 0; y <= detail; ++y) {
    float vy = offset + size * y;
    for (int x = 0; x <= detail; ++x, ++p) {
      p->p.x = offset + size * x;
      p->p.y = vy;
      p->p.z = frand(-size, size);
    }
  }
  
  int *pi = raw_indices_ = new int[ntriangles_ * 3];
  int index = 0;
  for (int y = 0; y < detail; ++y, ++index)
    for (int x = 0; x < detail; ++x, ++index) {
      *(pi++) = index, *(pi++) = index + 1, *(pi++) = index + detail + 2;
      *(pi++) = index, *(pi++) = index + detail + 2, *(pi++) = index + detail + 1;
    }
}

void Ground::calcNormals() {
  for (int i = 0; i < nraw_vertices_; ++i) raw_vertices_[i].n = vec3f(0);
  int *p = raw_indices_;
  for (int i = ntriangles_; i > 0; --i) {
    vertex_t &v0 = raw_vertices_[*(p++)],
             &v1 = raw_vertices_[*(p++)],
             &v2 = raw_vertices_[*(p++)];
    vec3f normal = (v1.p - v0.p).cross(v2.p - v0.p);
    v0.n += normal, v1.n += normal, v2.n += normal;
  }
  //for (int i = 0; i < nraw_vertices_; ++i) raw_vertices_[i].n.normalize();
}