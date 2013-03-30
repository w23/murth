#include "../murth/murth.h"
#include <kapusha/core/Core.h>
#include <kapusha/core/IViewport.h>
#include <kapusha/render/Render.h>
#include <kapusha/ooo/Camera.h>
#include "Ground.h"
#include "Central.h"
using namespace kapusha;

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
  Central *central_;
  Render *render_;
  float sampleAngle_;
};
IViewport *makeViewport() { return new Visuport; }
Visuport::Visuport() : is_playing_(false), camera_(vec3f(10.f,10.f,5.f), vec3f(0), vec3f(0,0,1)), sampleAngle_(0) {}
void Visuport::init(IViewportController *system) {
  controller_ = system;
  render_ = new Render;
  ground_ = new Ground(32, 2.f);
  central_ = new Central(2, 1.f);
  central_->frame().move(vec3f(0,0,4));
  central_->frame().update();
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
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  //float angle = c_pi4 + .2*sin(ms * 1e-3f);
  //camera_.lookAt(vec3f(cos(angle), sin(angle), .5f) * 10.f, vec3f(0), vec3f(0,0,1));
  camera_.update();
  
  murth_event_t mevent;
  while (murth_get_event(&mevent) != 0) {
    L("%d: %d %f", mevent.event, mevent.value.i, mevent.value.f);
    switch (mevent.event) {
    case 0:
      central_->spike(9.f);
      ground_->spike(vec3f(cos(sampleAngle_) * 16.f,
                           sin(sampleAngle_) * 16.f, 6.f));
      break;
    case 2:
        camera_.translate(vec3f(20.f * (mevent.value.i / 127.f - .5f), 6.f, 7.f));
        camera_.update();
      break;
    }
  }
  sampleAngle_ = fmodf(sampleAngle_ + dt * c_2pi / 4.f, c_2pi);
  
  ground_->update(ms, dt);
  ground_->draw(Render::currentRender(), camera_.getMvp());
  //central_->frame().rotateRoll(dt);
  //central_->frame().update();
  central_->update(dt);
  central_->draw(Render::currentRender(), camera_.getMvp());
  controller_->requestRedraw();
}

void Visuport::close()
{
  delete ground_;
  delete render_;
}
