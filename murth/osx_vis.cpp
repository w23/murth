#include "synth.h"
#include <kapusha/core/Core.h>
#include <kapusha/core/IViewport.h>
#include <kapusha/render/Render.h>

using namespace kapusha;

extern "C" {
  extern void audio_play();
  extern void audio_stop();
}

ifu probdata[4][4096];
probe probes[4] = {
  {0, 4096, probdata[0]},
  {0, 4096, probdata[1]},
  {0, 4096, probdata[2]},
  {0, 4096, probdata[3]},
};

class Visuport : public IViewport
{
public:
  virtual void init(IViewportController* system);
  virtual void resize(vec2i);
  virtual void draw(int ms, float dt);
  virtual void close();

private:
  IViewportController *controller_;
  Batch *batch_;
  bool is_playing_;
  
  Texture tex[4];
};

IViewport *makeViewport()
{
  return new Visuport;
}

void Visuport::init(IViewportController *system)
{
  is_playing_ = false;
  controller_ = system;
  batch_ = new Batch();
  
  const char* svtx =
  "uniform vec2 aspect;\n"
  "uniform float time;\n"
  "attribute vec4 vtx;\n"
  "varying vec2 p;\n"
  "varying vec3 pos, dir;\n"
  "varying mat3 cuberot;\n"
  "void main(){\n"
    "gl_Position = vtx;\n"
    "p = vtx.xy * aspect;\n"
    "vec2 sc = vec2(sin(-time*.5),cos(-time*.5));\n"
    //"vec2 sc = vec2(0., 1.);\n"
    //"mat3 m = mat3(sc.y, 0., sc.x, 0., 1., 0., -sc.x, 0., sc.y);\n"
    "mat3 m = mat3(1., 0., 0., 0., sc.y, sc.x, 0., -sc.x, sc.y);\n"
    "sc = vec2(sin(-time*.2),cos(-time*.2));\n"
    "cuberot = mat3(sc.y, sc.x, 0., -sc.x, sc.y, 0., 0., 0., 1.);\n"
    "pos = vec3(p, -.1) * m;\n"
    "dir = vec3(p, 1.) * m;\n"
  "}"
  ;
  const char* sfrg =
  "uniform float time;\n"
  "uniform sampler2D p0, p1, p2, p3;\n"
  "varying vec2 p;\n"
  "varying vec3 pos, dir;\n"
  "varying mat3 cuberot;\n"
  
  "float sphere(vec3 a, float r){\n"
    "return length(a) - r;\n"
  "}\n"
  
  "float vmax(vec3 v){\n"
    "return max(v.x, max(v.y, v.z));\n"
  "}\n"
  
  "float cube(vec3 a, vec3 s){\n"
    "return vmax(abs(a) - s);\n"
  "}\n"
  
  "vec3 repeat(vec3 a, vec3 p){\n"
    "return mod(a,p) - p*.5;\n"
  "}\n"
  
  "float world(vec3 a){\n"
    //"return min(max(-sphere(repeat(a,vec3(1.)), .6), cube(repeat(a,vec3(3.)), vec3(1.))), a.y + 1.);\n"
    "return max(-sphere(repeat(a+.2*texture2D(p3, vec2((a.z+time)*.1,0.)).wxx,vec3(1.)), .6), cube(repeat(a*cuberot,vec3(3.)), vec3(1.)));\n"
  "}\n"
  
  "vec3 normal(vec3 a){\n"
    "vec2 e = vec2(.01, 0.);\n"
    "return normalize(vec3(world(a+e.xyy),world(a+e.yxy),world(a+e.yyx)));\n"
  "}\n"
  
  "float diffuseLight(vec3 a, vec3 l){\n"
    "vec3 ld = l-a;\n"
    "return max(0.,dot(ld, normal(a))) / dot(ld,ld);\n"
  "}\n"
    
  "vec4 trace(vec3 p, vec3 d, float maxL){\n"
    "float L = 0.;\n"
    "d = normalize(d);\n"
    "for(int i = 0; i < 64; ++i){\n"
      "float w = world(p+d*L);\n"
      "if (w < .0001) break;\n"
      "L += w;\n"
      "if (L > maxL) break;\n"
    "}"
    "return vec4(p+d*L, L);\n"
  "}\n"
  
  "void main(){\n"
    "vec4 t = trace(pos, dir+.1*texture2D(p3,vec2(p.x*.1+time*.1,0.)).wxx, 10.);\n"
    "vec3 color = .2 * vec3(1. - t.w / 10.);\n"
    "vec2 sc = vec2(sin(time),cos(time));\n"
    "mat3 m = mat3(sc.y, 0., sc.x, 0., 1., 0., -sc.x, 0., sc.y);\n"
    "color += diffuseLight(t.xyz, vec3(2.)*m) * vec3(.8,.8,.4);\n"
    "color += diffuseLight(t.xyz, vec3(2.,2.,-3.)*(-m)) * vec3(.3,.8,.8);\n"
    "gl_FragColor = vec4(color, 1.)"
      "+ vec4(.3) * abs(1.-smoothstep(.19, .20, 10.*abs(texture2D(p3,vec2(p.x*.1+.1+time/4.41,0.)).w-p.y)))"
      "+ texture2D(p0, p) * 0."
      "+ texture2D(p1, p) * 0."
      "+ texture2D(p2, p) * 0."
      "+ texture2D(p3, p) * 0."
      ";\n"
    "gl_FragDepth = t.w;\n"
  "}"
  ;
  Material *mat = new Material(new Program(svtx, sfrg));
  batch_->setMaterial(mat);
  
  vec2f rect[4] = {
    vec2f(-1.f, -1.f),
    vec2f(-1.f,  1.f),
    vec2f( 1.f,  1.f),
    vec2f( 1.f, -1.f)
  };
  Buffer *fsrect = new Buffer();
  fsrect->load(rect, sizeof rect);
  batch_->setAttribSource("vtx", fsrect, 2);
  
  batch_->setGeometry(Batch::GeometryTriangleFan, 0, 4, 0);
}

void Visuport::resize(vec2i s)
{
  glViewport(0, 0, s.x, s.y);
  vec2f aspect((float)s.x / s.y, 1.f);
  batch_->getMaterial()->setUniform("aspect", aspect);
}

void Visuport::draw(int ms, float dt)
{
  if (!is_playing_)
  {
    audio_play();
    is_playing_ = true;
  }
  
  tex[0].upload(Texture::ImageDesc(64, 64, Texture::ImageDesc::Format_R32F), probdata[0]);
  tex[1].upload(Texture::ImageDesc(64, 64, Texture::ImageDesc::Format_R32F), probdata[1]);
  tex[2].upload(Texture::ImageDesc(64, 64, Texture::ImageDesc::Format_R32F), probdata[2]);
  tex[3].upload(Texture::ImageDesc(4096, 1, Texture::ImageDesc::Format_R32F), probdata[3]);
  
  float time = ms / 1000.f;
  batch_->getMaterial()->setUniform("time", time);
  batch_->getMaterial()->setTexture("p0", &tex[0]);
  batch_->getMaterial()->setTexture("p1", &tex[1]);
  batch_->getMaterial()->setTexture("p2", &tex[2]);
  batch_->getMaterial()->setTexture("p3", &tex[3]);
  batch_->draw();
  controller_->requestRedraw();
}

void Visuport::close()
{
  audio_stop();
  delete batch_;
}