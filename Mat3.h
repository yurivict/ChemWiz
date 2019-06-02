#pragma once

#include <boost/array.hpp>
#include <ostream>
#include <math.h>
#include "Vec3.h"

typedef double Float;

template<typename Float>
class TMat3 : public boost::array<TVec3<Float>, 3> {
public:
  TMat3() { }
  TMat3(const TVec3<Float> &v) : boost::array<Vec3, 3>({{Vec3(v(X),0,0), Vec3(0,v(Y),0), Vec3(0,0,v(Z))}}) { } // Vec3 -> diagonal matrix TMat3 constructor
  TMat3(const TVec3<Float> &v1, const Vec3 &v2, const Vec3 &v3) : boost::array<Vec3, 3>({{v1, v2, v3}}) { }
  Float& operator()(unsigned idx1, unsigned idx2) { // 1-based index access
    return (*this)[idx1-1](idx2);
  }
  TMat3 t() const {
    auto swap = [](typename TVec3<Float>::val_t &f1, typename Vec3::val_t &f2) {
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
  static bool almostEquals(const TMat3 &m1, const TMat3 &m2, Float eps)
    {return TVec3<Float>::almostEquals(m1[0], m2[0], eps) && Vec3::almostEquals(m1[1], m2[1], eps) && Vec3::almostEquals(m1[2], m2[2], eps);}
  static TMat3 zero() {return TMat3(TVec3<Float>(0,0,0), Vec3(0,0,0), Vec3(0,0,0));}
  static TMat3 identity() {return TMat3(TVec3<Float>(1,0,0), Vec3(0,1,0), Vec3(0,0,1));}
  TMat3 transpose() const {
    const auto &i = *this;
    return TMat3(TVec3<Float>(i(1,1),i(2,1),i(3,1)), Vec3(i(1,2),i(2,2),i(3,2)), Vec3(i(1,3),i(2,3),i(3,3)));
  }
  static TMat3 rotate(const TVec3<Float> &r) {
    Float rLen = r.len();
    if (rLen == 0)
      return TMat3::identity();
    return rotate(r/rLen, rLen);
  }
  static TMat3 rotate(const TVec3<Float> &u, Float theta) {
    Float sinTh;
    Float cosTh;
    sincos(theta, &sinTh, &cosTh);
    return TMat3(
      TVec3<Float>(cosTh+u(1)*u(1)*(1-cosTh),      u(1)*u(2)*(1-cosTh)-u(3)*sinTh, u(1)*u(3)*(1-cosTh)+u(2)*sinTh),
      TVec3<Float>(u(2)*u(1)*(1-cosTh)+u(3)*sinTh, cosTh+u(2)*u(2)*(1-cosTh),      u(2)*u(3)*(1-cosTh)-u(1)*sinTh),
      TVec3<Float>(u(3)*u(1)*(1-cosTh)-u(2)*sinTh, u(3)*u(2)*(1-cosTh)+u(1)*sinTh, cosTh+u(3)*u(3)*(1-cosTh))
    );
  }
  TVec3<Float> toRotationVector() const {
    assert(0); // TODO
  }
  Float operator()(unsigned idx1, unsigned idx2) const { // 1-based index access
    return (*this)[idx1-1](idx2);
  }
  friend std::ostream& operator<<(std::ostream &os, const TMat3 &m) {
    os << "{" << m[0] << "," << m[1] << "," << m[2] << "}";
    return os;
  }
  TMat3 operator+(const TMat3 &m) const {
    return TMat3((*this)[0]+m[0], (*this)[1]+m[1], (*this)[2]+m[2]);
  }
  TMat3 operator-(const TMat3 &m) const {
    return TMat3((*this)[0]-m[0], (*this)[1]-m[1], (*this)[2]-m[2]);
  }
  TMat3 operator*(Float m) const {
    return TMat3((*this)[0]*m, (*this)[1]*m, (*this)[2]*m);
  }
  TMat3 operator*(const TMat3 &m) const {
    const auto &i = *this;
    return TMat3(TVec3<Float>(i(1,1)*m(1,1)+i(1,2)*m(2,1)+i(1,3)*m(3,1), i(1,1)*m(1,2)+i(1,2)*m(2,2)+i(1,3)*m(3,2), i(1,1)*m(1,3)+i(1,2)*m(2,3)+i(1,3)*m(3,3)),
                 TVec3<Float>(i(2,1)*m(1,1)+i(2,2)*m(2,1)+i(2,3)*m(3,1), i(2,1)*m(1,2)+i(2,2)*m(2,2)+i(2,3)*m(3,2), i(2,1)*m(1,3)+i(2,2)*m(2,3)+i(2,3)*m(3,3)),
                 TVec3<Float>(i(3,1)*m(1,1)+i(3,2)*m(2,1)+i(3,3)*m(3,1), i(3,1)*m(1,2)+i(3,2)*m(2,2)+i(3,3)*m(3,2), i(3,1)*m(1,3)+i(3,2)*m(2,3)+i(3,3)*m(3,3)));
  }
  TVec3<Float> operator*(const Vec3 &v) const {
    const auto &i = *this;
    return TVec3<Float>(i(1,1)*v(1) + i(1,2)*v(2) + i(1,3)*v(3),
                        i(2,1)*v(1) + i(2,2)*v(2) + i(2,3)*v(3),
                        i(3,1)*v(1) + i(3,2)*v(2) + i(3,3)*v(3));
  }
  TMat3 operator/(Float m) const {
    return TMat3((*this)[0]/m, (*this)[1]/m, (*this)[2]/m);
  }
}; // TMat3

typedef TMat3<double> Mat3;
typedef TMat3<float> Mat3f;
