#include "FlatShadedMesh.h"

static const char shader_vertex[] =
"attribute vec3 av3_pos, av3_normal;\n"
"uniform mat4 um4_mvp;\n"
"uniform vec3 uv3_l1_pos, uv3_l1_color;"
"varying vec3 vv3_color;\n"
"void main() {\n"
"gl_Position = um4_mvp * vec4(av3_pos, 1.);\n"
//"vec3 normal = (um4_mvp*vec4(av3_normal,0.)).xyz;\n"
"vec3 normal = av3_normal;\n"
"vec3 l1dir = uv3_l1_pos - av3_pos;\n"
"vv3_color = vec3(.2) + uv3_l1_color * (10. * max(dot(normal,l1dir),0.) / dot(l1dir,l1dir));\n"
//"vv3_color = vec3(.2) + uv3_l1_color * max(dot(normal,normalize(uv3_l1_pos)), 0.);\n"
"}\n";
static const char shader_fragment[] =
"varying vec3 vv3_color;\n"
"void main() {\n"
"gl_FragColor = vec4(vv3_color, 1.);\n"
"}\n";

void FlatShadedMesh::init(int nraw_vertices, int ntriangles) {
  nraw_vertices_ = nraw_vertices;
  ntriangles_ = ntriangles;
  nvertices_ = ntriangles_ * 3;
  raw_vertices_ = new vertex_t[nraw_vertices_];
  raw_indices_ = new int[ntriangles_ * 3];
  Program *p = new Program(shader_vertex, shader_fragment);
  Material *m = new Material(p);
  m->setUniform("uv3_l1_pos", vec3f(-6.,7.,8.f));
  m->setUniform("uv3_l1_color", vec3f(.8f, .7f, .2f));
  Batch *b = new Batch(m);
  vertices_ = new Buffer();
  vertices_->alloc(nvertices_ * sizeof(vertex_t), Buffer::StreamDraw);
  b->setGeometry(Batch::GeometryTriangleList, 0, nvertices_);
  b->setAttribSource("av3_pos", vertices_,
                     3, offsetof(vertex_t, p), sizeof(vertex_t));
  b->setAttribSource("av3_normal", vertices_,
                     3, offsetof(vertex_t, n), sizeof(vertex_t));
  addBatch(b);
}

FlatShadedMesh::~FlatShadedMesh() {
  delete raw_vertices_;
  delete raw_indices_;
}

void FlatShadedMesh::calcNormalsAndUpload() { {
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
  } {
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
}