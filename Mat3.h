#pragma once

#include <boost/array.hpp>
#include <ostream>
#include <math.h>
#include "Vec3.h"

typedef double Float;

class Mat3 : public boost::array<Vec3, 3> {
public:
  Mat3() { }
  Mat3(const Vec3 &v) : boost::array<Vec3, 3>({{Vec3(v(X),0,0), Vec3(0,v(Y),0), Vec3(0,0,v(Z))}}) { } // Vec3 -> diagonal matrix Mat3 constructor
  Mat3(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3) : boost::array<Vec3, 3>({{v1, v2, v3}}) { }
  Float& operator()(unsigned idx1, unsigned idx2) { // 1-based index access
    return (*this)[idx1-1](idx2);
  }
  Mat3 t() const {
    auto swap = [](Vec3::val_t &f1, Vec3::val_t &f2) {
      auto tmp = f1;
      f1 = f2;
      f2 = tmp;
    };
    auto tmp = *this;
    swap(tmp(1,2), tmp(2,1));
    swap(tmp(1,3), tmp(3,1));
    swap(tmp(2,3), tmp(3,2));
    return tmp;
  }
  static bool almostEquals(const Mat3 &m1, const Mat3 &m2, Float eps)
    {return Vec3::almostEquals(m1[0], m2[0], eps) && Vec3::almostEquals(m1[1], m2[1], eps) && Vec3::almostEquals(m1[2], m2[2], eps);}
  static Mat3 zero() {return Mat3(Vec3(0,0,0), Vec3(0,0,0), Vec3(0,0,0));}
  static Mat3 identity() {return Mat3(Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1));}
  Mat3 transpose() const {
    const auto &i = *this;
    return Mat3(Vec3(i(1,1),i(2,1),i(3,1)), Vec3(i(1,2),i(2,2),i(3,2)), Vec3(i(1,3),i(2,3),i(3,3)));
  }
  static Mat3 rotate(const Vec3 &r) {
    Float rLen = r.len();
    if (rLen == 0)
      return Mat3::identity();
    return rotate(r/rLen, rLen);
  }
  static Mat3 rotate(const Vec3 &u, Float theta) {
    Float sinTh;
    Float cosTh;
    sincos(theta, &sinTh, &cosTh);
    return Mat3(
      Vec3(cosTh+u(1)*u(1)*(1-cosTh),      u(1)*u(2)*(1-cosTh)-u(3)*sinTh, u(1)*u(3)*(1-cosTh)+u(2)*sinTh),
      Vec3(u(2)*u(1)*(1-cosTh)+u(3)*sinTh, cosTh+u(2)*u(2)*(1-cosTh),      u(2)*u(3)*(1-cosTh)-u(1)*sinTh),
      Vec3(u(3)*u(1)*(1-cosTh)-u(2)*sinTh, u(3)*u(2)*(1-cosTh)+u(1)*sinTh, cosTh+u(3)*u(3)*(1-cosTh))
    );
  }
  Vec3 toRotationVector() const {
    assert(0); // TODO
  }
  Float operator()(unsigned idx1, unsigned idx2) const { // 1-based index access
    return (*this)[idx1-1](idx2);
  }
  friend std::ostream& operator<<(std::ostream &os, const Mat3 &m) {
    os << "{" << m[0] << "," << m[1] << "," << m[2] << "}";
    return os;
  }
  Mat3 operator+(const Mat3 &m) const {
    return Mat3((*this)[0]+m[0], (*this)[1]+m[1], (*this)[2]+m[2]);
  }
  Mat3 operator-(const Mat3 &m) const {
    return Mat3((*this)[0]-m[0], (*this)[1]-m[1], (*this)[2]-m[2]);
  }
  Mat3 operator*(Float m) const {
    return Mat3((*this)[0]*m, (*this)[1]*m, (*this)[2]*m);
  }
  Mat3 operator*(const Mat3 &m) const {
    const auto &i = *this;
    return Mat3(Vec3(i(1,1)*m(1,1)+i(1,2)*m(2,1)+i(1,3)*m(3,1), i(1,1)*m(1,2)+i(1,2)*m(2,2)+i(1,3)*m(3,2), i(1,1)*m(1,3)+i(1,2)*m(2,3)+i(1,3)*m(3,3)),
                Vec3(i(2,1)*m(1,1)+i(2,2)*m(2,1)+i(2,3)*m(3,1), i(2,1)*m(1,2)+i(2,2)*m(2,2)+i(2,3)*m(3,2), i(2,1)*m(1,3)+i(2,2)*m(2,3)+i(2,3)*m(3,3)),
                Vec3(i(3,1)*m(1,1)+i(3,2)*m(2,1)+i(3,3)*m(3,1), i(3,1)*m(1,2)+i(3,2)*m(2,2)+i(3,3)*m(3,2), i(3,1)*m(1,3)+i(3,2)*m(2,3)+i(3,3)*m(3,3)));
  }
  Vec3 operator*(const Vec3 &v) const {
    const auto &i = *this;
    return Vec3(i(1,1)*v(1) + i(1,2)*v(2) + i(1,3)*v(3),
                i(2,1)*v(1) + i(2,2)*v(2) + i(2,3)*v(3),
                i(3,1)*v(1) + i(3,2)*v(2) + i(3,3)*v(3));
  }
}; // Mat3
