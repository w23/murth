#pragma once
#include "FlatShadedMesh.h"
class Ground : public FlatShadedMesh {
public:
  Ground(int detail, float size);
  ~Ground();
  void spike(vec3f posH);
  void update(u32 t, float dt);
private:
  void generatePlane();
  int detail_;
  float size_;
  float *raw_velocities_;
};