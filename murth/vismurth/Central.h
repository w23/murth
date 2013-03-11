#pragma once
#include "FlatShadedMesh.h"
class Central : public FlatShadedMesh {
public:
  Central(int detail, float r);
  ~Central() {}
private:
  void generateMesh(int detail, float r);
  int detail_;
  float r_;
};