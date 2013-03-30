#pragma once
#include "FlatShadedMesh.h"
class Central : public FlatShadedMesh {
public:
  Central(int detail, float r);
  ~Central() {}
  void spike(float r);
  void update(float dt);
private:
  void generateMesh(int detail, float r);
  int detail_;
  float r_;
};