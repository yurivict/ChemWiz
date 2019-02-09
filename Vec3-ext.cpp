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
      return Mat3::unity();
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

