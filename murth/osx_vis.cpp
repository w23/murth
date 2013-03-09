#include "synth.h"
#include <kapusha/core/Core.h>
#include <kapusha/core/IViewport.h>
#include <kapusha/render/Render.h>
#include <kapusha/ooo/Camera.h>
#include "Ground.h"
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

class Visuport : public IViewport {
public:
  Visuport();
  virtual void init(IViewportController* system);
  virtual void resize(vec2i);
  virtual void draw(int ms, float dt);
  virtual void close();
private:
  IViewportController *controller_;
  bool is_playing_;
  Camera camera_;
  Ground *ground_;
  Render *render_;
  float sampleAngle_;
};
IViewport *makeViewport() { return new Visuport; }
Visuport::Visuport() : is_playing_(false), camera_(vec3f(10.f), vec3f(0), vec3f(0,0,1)), sampleAngle_(0) {}
void Visuport::init(IViewportController *system) {
  controller_ = system;
  render_ = new Render;
  ground_ = new Ground(64, 1.f);
  render_->cullFace().enable();
  render_->depthTest().enable();
  render_->commit();
}

void Visuport::resize(vec2i s) {
  glViewport(0, 0, s.x, s.y);
  camera_.setAspect((float)s.x / s.y);
  camera_.update();
}

void Visuport::draw(int ms, float dt) {
  if (!is_playing_) { audio_play(); is_playing_ = true; }
  
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  //float angle = ms * 1e-3f;
  //camera_.lookAt(vec3f(cos(angle), sin(angle), 1) * 10.f, vec3f(0), vec3f(0,0,1));
  //camera_.update();
  
  //tex[0].upload(Texture::ImageDesc(64, 64, Texture::ImageDesc::Format_R32F), probdata
  float sas = c_2pi / 4.f;
  float sad = sas * dt;
  for (int i = 0; i < 1024; i+=4) {
    float angle = sampleAngle_ + sad * i / 1024;
    ground_->spike(vec3f(cos(angle) * 16.f, sin(angle) * 16.f, 4.f * probdata[2][i].f));
  }
  sampleAngle_ = fmodf(sampleAngle_ + sad, c_2pi);
  ground_->update(ms, dt);
  ground_->draw(Render::currentRender(), camera_.getMvp());
  controller_->requestRedraw();
}

void Visuport::close()
{
  delete ground_;
  delete render_;
  audio_stop();
}