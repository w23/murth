#include "synth.h"
#include <kapusha/core/Core.h>
#include <kapusha/core/IViewport.h>
#include <kapusha/render/Render.h>

using namespace kapusha;

extern "C" {
  extern void audio_play();
  extern void audio_stop();
}

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
  "varying vec3 pos, dir;\n"
  "void main(){\n"
  "gl_Position = vtx;\n"
    "vec2 p = vtx.xy * aspect;\n"
    "vec2 sc = vec2(sin(time),cos(time));\n"
    "mat3 m = mat3(sc.y, 0., sc.x, 0., 1., 0., -sc.x, 0., sc.y);\n"
    "pos = vec3(p, -1.) * m;\n"
    "dir = vec3(p, 1.) * m;\n"
  "}"
  ;
  const char* sfrg =
  "varying vec3 pos, dir;\n"
  
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
    "return min(max(-sphere(repeat(a,vec3(1.)), .6), cube(repeat(a,vec3(3.)), vec3(1.))), a.y + 1.);\n"
  "}\n"
  
  "vec3 normal(vec3 a){\n"
    "vec2 e = vec2(.01, 0.);\n"
    "return normalize(vec3(world(a+e.xyy),world(a+e.yxy),world(a+e.yyx)));\n"
  "}\n"
  
  "float diffuseLight(vec3 a, vec3 l){\n"
    "vec3 ld = l-a;\n"
    "return max(0.,dot(ld, normal(a))) / dot(ld,ld);\n"
  "}\n"
  
  "float occlusion(vec3 a){\n"
  ""
  
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
    "vec4 t = trace(pos, dir, 10.);\n"
    "vec3 color = .2 * vec3(1. - t.w / 10.);\n"
    "color += diffuseLight(t.xyz, vec3(2.)) * vec3(.8,.8,.4);\n"
    "color += diffuseLight(t.xyz, vec3(2.,2.,-3.)) * vec3(.3,.8,.8);\n"
    "gl_FragColor = vec4(color, 1.);\n"
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
  
  float time = ms / 1000.f;
  batch_->getMaterial()->setUniform("time", time);
  batch_->draw();
  controller_->requestRedraw();
}

void Visuport::close()
{
  audio_stop();
  delete batch_;
}