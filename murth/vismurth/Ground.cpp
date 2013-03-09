#include "Ground.h"

static const char shader_vertex[] =
"attribute vec3 av3_pos, av3_normal;\n"
"uniform mat4 um4_mvp;\n"
"uniform vec3 uv3_l1_pos, uv3_l1_color;"
"varying vec3 vv3_color;\n"
"void main() {\n"
  "gl_Position = um4_mvp * vec4(av3_pos, 1.);\n"
  "vec3 l1dir = uv3_l1_pos - av3_pos;\n"
  "vv3_color = vec3(.2) + uv3_l1_color * (10. * dot(av3_normal,l1dir) / dot(l1dir,l1dir));\n"
"}\n";
static const char shader_fragment[] =
"varying vec3 vv3_color;\n"
"void main() {\n"
  "gl_FragColor = vec4(vv3_color, 1.);\n"
"}\n";

Ground::Ground(int detail, float size) : detail_(detail), size_(size) {
  Program *p = new Program(shader_vertex, shader_fragment);
  Material *m = new Material(p);
  m->setUniform("uv3_l1_pos", vec3f(0,0,8.f));
  m->setUniform("uv3_l1_color", vec3f(.8f, .7f, .2f));
  Batch *b = new Batch(m);
  vertices_ = new Buffer();
  generatePlane();
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
  delete raw_velocities_;
  delete raw_vertices_;
  delete raw_indices_;
}

void Ground::update(u32 t, float dt) {
  int stride = detail_ + 1;
  if (dt > .016) dt = .016;
  //dt = 0.016;
  for (int y = 1; y < detail_; ++y) {
    vertex_t *pvtx = raw_vertices_ + stride * y + 1;
    float *pvel = raw_velocities_ + stride * y + 1;
    for (int x = 1; x < detail_; ++x, ++pvtx, ++pvel) {
      float a = 0;
      a += pvtx[-stride].p.z - pvtx[0].p.z;
      a += pvtx[+stride].p.z - pvtx[0].p.z;
      a += pvtx[-1].p.z - pvtx[0].p.z;
      a += pvtx[+1].p.z - pvtx[0].p.z;
      pvel[0] += a * dt * 4.f;
      pvtx[0].p.z += pvel[0] * dt;
      pvel[0] *= .995f;
    }
  }
  
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

void Ground::generatePlane() {
  ntriangles_ = detail_ * detail_ * 2;
  nvertices_ = ntriangles_ * 3;
  nraw_vertices_ = (detail_ + 1) * (detail_ + 1);
  raw_vertices_ = new vertex_t[nraw_vertices_];
  raw_velocities_ = new float[nraw_vertices_];
  vertex_t *p = raw_vertices_;
  float *pv = raw_velocities_;
  const float offset = - detail_ * size_ * .5;
  for (int y = 0; y <= detail_; ++y) {
    float vy = offset + size_ * y;
    for (int x = 0; x <= detail_; ++x, ++p, ++pv) {
      p->p.x = offset + size_ * x;
      p->p.y = vy;
      p->p.z = (x!=0 && y!=0 && x!=detail_ && y!=detail_)?frand(-size_, size_):0;
      *pv = 0;
    }
  }
  
  int *pi = raw_indices_ = new int[ntriangles_ * 3];
  int index = 0;
  for (int y = 0; y < detail_; ++y, ++index)
    for (int x = 0; x < detail_; ++x, ++index) {
      *(pi++) = index, *(pi++) = index + 1, *(pi++) = index + detail_ + 2;
      *(pi++) = index, *(pi++) = index + detail_ + 2, *(pi++) = index + detail_ + 1;
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

void Ground::spike(vec3f posH) {
  vec2i pos = posH.xy() / size_ + vec2i(detail_ / 2);
  pos.x = clamp(pos.x, 0, detail_);
  pos.y = clamp(pos.y, 0, detail_);
  raw_vertices_[pos.x + pos.y * (detail_ + 1)].p.z
    = max(raw_vertices_[pos.x + pos.y * (detail_ + 1)].p.z, posH.z);
}