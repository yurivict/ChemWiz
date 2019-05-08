#include "Vec3.h"
#include "Vec3-ext.h"
#include "Mat3.h"

#include <cmath>
#include <iostream>

Mat3 Vec3Extra::rotateCornerToCorner(const Vec3 &n1, const Vec3 &n2, const Vec3 &m1, const Vec3 &m2) {
  //auto genPlane = [](const Vec3 &v1, const Vec3 &v2) -> std::array<Vec3, 2> { // plane where all rotations between v1 and v2 lie
  //  return {v1+v2, v1.cross(v2)};
  //};
  //auto plane1 = genPlane(n1, m1);
  //auto plane2 = genPlane(n2, m2);

  //std::cout << "> Vec3Extra::rotateCornerToCorner: n1=" << n1 << " n2=" << n2 << " m1=" << m1 << " m2=" << m2 << std::endl;

  //// ad-hoc algorithm (a better algorithm might exist, but we don't know it)
  auto rotateBetweenVectors = [](const Vec3 &v1, const Vec3 &v2, const Vec3 &v2o) { // rotate v1 -> v2
    auto v = v1.cross(v2);
    auto s = v1*v2;
    auto vLen = v.len();
    if (!Vec3::is(vLen,0)) { // not parallel: rotate around it
      return Mat3::rotate(v/vLen, std::acos(s));
    } else if (Vec3::is(s,1)) { // parallel
      return Mat3::identity();
    } else { // anti-parallel: rotate around some orthogonal vector
      return Mat3::rotate(v2o, M_PI);
    }
  };
  // rotate m2 -> n2
  Mat3 R2 = rotateBetweenVectors(m2,n2,n1);
  auto m1prime = R2*m1; // rotated m1

  // rotate m1prime into n1
  //std::cout << "< Vec3Extra::rotateCornerToCorner: M=" << rotateBetweenVectors(m1prime,n1,n2)*R2 << std::endl;
  return rotateBetweenVectors(m1prime,n1,n2)*R2;
}

Float Vec3Extra::angleAxis1x1(const Vec3 &axis, const Vec3 &far, const Vec3 &near) { // ASSUME 'axis' is normalized
  // axis:       the axis of rotation
  // far:        one vector outgoing from the far end of the 'axis'
  // near:       one vector outgoing from the near end of the 'axis'

  // checks
  assert(axis.isNormalized());
  assert(!far.isParallel(axis));
  assert(!near.isParallel(axis));

  // values
  auto farOrth =  far.orthogonal(axis).normalize();
  auto nearOrth = near.orthogonal(axis).normalize();

  // compute the angle
  auto sin = farOrth.cross(nearOrth)*axis;
  auto cos = nearOrth*farOrth;
  auto a = std::atan(sin/cos);
  if (cos < 0) {
    if (a < 0)
      a += M_PI;
    else
      a -= M_PI;
  }
  return -Vec3::radToDeg(a);
}

Float Vec3Extra::angleAxis2x1(const Vec3 &axis, const Vec3 &far1, const Vec3 &far2, const Vec3 &near) { // probably not useful - 1x1 should be more appropriate
  // axis:       the axis of rotation
  // far1, far2: two vector outgoing from the far end of the 'axis'
  // near:       one vector outgoing from the near end of the 'axis', originally towards the right direction

  // Returns: the angle of rotation across the axis that leads to the configuration given by the arguments

  // compute values
  auto axisNorm = axis.normalize();
  auto farCross = far2.cross(far1);

  // checks
  assert(!far1.isParallel(far2));
  assert(!farCross.isParallel(axis)); // orthogonal "far" plane can't define the rotation direction
  assert(!near.isParallel(axis));

  // normalized orthogonal projection of the "near" vector
  auto nearRight = near.orthogonal(axisNorm).normalize();

  // normalized orthogonal projection of the "far" plane
  auto farUp = farCross.orthogonal(axisNorm).normalize();

  // the angle is zero when nearRight points "right" and farUp points "up", compute what the angle is
  return Vec3::radToDeg(std::asin(nearRight*farUp));
}

