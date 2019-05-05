#pragma once

#include "Vec3.h"
#include "Mat3.h"

// Vec3Extra implements some functions that are too complex or advanced to be included in Vec3

class Vec3Extra {
public:
  // rotates the pair of orthonormal vectors m1/m2 to be parallel to the n1/n2 pair of orthonormal vectors
  // returns a rotation vector
  static Mat3 rotateCornerToCorner(const Vec3 &n1, const Vec3 &n2, const Vec3 &m1, const Vec3 &m2);
  // angle calculation (like in a Ramachandran plot, but with a degree of non-planarity present)
  static Float angleAxis2x1(const Vec3 &axis, const Vec3 &far1, const Vec3 &far2, const Vec3 &near);
}; // Vec3Extra
