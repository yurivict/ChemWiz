#pragma once

#include <boost/array.hpp>
#include <ostream>
#include <cmath>

typedef double Float;

/// Vector helper classes
enum VecCoords {X=1, Y=2, Z=3};

template<typename Float>
class TVec3 : public boost::array<Float, 3> {
public:
  typedef Float val_t;
  TVec3() { }
  TVec3(Float x, Float y, Float z) : boost::array<Float, 3>({{x, y, z}}) { }
  Float& operator()(unsigned idx) { // 1-based index access
    return (*this)[idx-1];
  }
  Float operator()(unsigned idx) const { // 1-based index access
    return (*this)[idx-1];
  }
  static TVec3 zero() {return TVec3(0,0,0);}
  static TVec3 one(unsigned idx, Float val) {TVec3 vec(0,0,0); vec(idx) = val; return vec;}
  static bool almostEquals(const TVec3 &v1, const TVec3 &v2, Float eps)
    {return almostEquals(v1(X),v2(X),eps) && almostEquals(v1(Y),v2(Y),eps) && almostEquals(v1(Z),v2(Z),eps);}
  static bool almostEquals(Float c1, Float c2, Float eps)
    {return std::fabs(c1 - c2) <= eps;}
  Float len2() const {return sq((*this)(X)) + sq((*this)(Y)) + sq((*this)(Z));}
  Float len() const {return sqrt(len2());}
  TVec3 normalize() const {
    return (*this)/len();
  }
  TVec3 normalizeZ(Float l) const { // expects the pre-computed length argument for efficiency
    if (l != 0)
      return (*this)/l;
    else
      return TVec3(0,0,0);
  }
  TVec3 normalizeZ() const {
    return normalizeZ(len());
  }
  TVec3 normalizeZl(Float outLen) const {
    auto l = len();
    outLen = l;
    if (l != 0)
      return (*this)/l;
    else
      return TVec3(0,0,0);
  }
  void snapToGrid(const TVec3 &grid) {
    TVec3 &i = *this;
    snapToGrid(i(X), grid(X));
    snapToGrid(i(Y), grid(Y));
    snapToGrid(i(Z), grid(Z));
  }
  static void snapToGrid(Float &c, Float g) {
    c = int(c/g)*g;
  }
  friend std::ostream& operator<<(std::ostream &os, const TVec3 &v) {
    os << "{" << v(X) << "," << v(Y) << "," << v(Z) << "}";
    return os;
  }
  TVec3 operator+() const {
    return *this;
  }
  TVec3 operator-() const {
    const TVec3 &i = *this;
    return TVec3(-i(X), -i(Y), -i(Z));
  }
  TVec3 operator*(Float m) const {
    const TVec3 &i = *this;
    return TVec3(i(X)*m, i(Y)*m, i(Z)*m);
  }
  Float operator*(const TVec3 &v) const { // scalar multiplication
    const TVec3 &i = *this;
    return i(X)*v(X) + i(Y)*v(Y) + i(Z)*v(Z);
  }
  TVec3 scale(const TVec3 &v) const { // scalar multiplication
    const TVec3 &i = *this;
    return {i(X)*v(X), i(Y)*v(Y), i(Z)*v(Z)};
  }
  TVec3 cross(const TVec3 &v) const { // cross (vector) multiplication
    const TVec3 &i = *this;
    return TVec3(-i(Z)*v(Y) + i(Y)*v(Z), i(Z)*v(X) - i(X)*v(Z), -i(Y)*v(X) + i(X)*v(Y)); // see https://en.wikipedia.org/wiki/Cross_product#Conversion_to_matrix_multiplication
  }
  Float angle(const TVec3 &v) const {
    return std::acos((*this)*v/(this->len()*v.len()));
  }
  TVec3 operator/(Float m) const {
    const TVec3 &i = *this;
    return TVec3(i(X)/m, i(Y)/m, i(Z)/m);
  }
  TVec3 operator+(const TVec3 &v) const {
    const TVec3 &i = *this;
    return TVec3(i(X)+v(X), i(Y)+v(Y), i(Z)+v(Z));
  }
  TVec3 operator-(const TVec3 &v) const {
    const TVec3 &i = *this;
    return TVec3(i(X)-v(X), i(Y)-v(Y), i(Z)-v(Z));
  }
  TVec3& operator+=(const TVec3 &v) {
    TVec3 &i = *this;
    i(X) += v(X);
    i(Y) += v(Y);
    i(Z) += v(Z);
    return *this;
  }
  TVec3& operator-=(const TVec3 &v) {
    TVec3 &i = *this;
    i(X) -= v(X);
    i(Y) -= v(Y);
    i(Z) -= v(Z);
    return *this;
  }
  TVec3& operator*=(Float c) {
    TVec3 &i = *this;
    i(X) *= c;
    i(Y) *= c;
    i(Z) *= c;
    return *this;
  }
  TVec3 project(const TVec3 &dir) const { // dir is assumed to be a normal vector or zero vector
    return dir*((*this)*dir);
  }
  TVec3 orthogonal(const TVec3 &dir) const { // dir is assumed to be a normal vector or zero vector
    return *this - project(dir);
  }
  bool isNormalized() const {return is(len2(), 1);} // to be used primarily in asserts, otherwie this check probably shouldn't be needed
  bool isParallel(const TVec3 &other) const {return is(normalize()*other.normalize(), 1);}
  bool isOrthogonal(const TVec3 &other) const {return is(cross(other).len2(), 1);}
  TVec3 divOneByOne(const TVec3 &d) const { // dir is assumed to be a normal vector or zero vector
    const TVec3 &i = *this;
    return TVec3(i(X)/d(X), i(Y)/d(Y), i(Z)/d(Z));
  }
  static bool is(Float f1, Float f2) {return std::abs(f1-f2) < 0.001;} // tests how close are numbers
  static Float radToDeg(Float r) {return r/M_PI*180;}
  static Float degToRad(Float d) {return d/180*M_PI;}
private:
  static Float sq(Float f) {return f*f;}
}; // TVec3

typedef TVec3<double> Vec3;
typedef TVec3<float> Vec3f;

template<typename FromFloat, typename ToFloat>
class Vec3ToType {
public:
  static const TVec3<ToFloat> convert(const TVec3<FromFloat> &v) {
    return TVec3<ToFloat>((ToFloat)v(X), (ToFloat)v(Y), (ToFloat)v(Z));
  }
}; // Vec3ToType

template<typename XFloat>
class Vec3ToType<XFloat,XFloat> {
public:
  static const TVec3<XFloat>& convert(const TVec3<XFloat> &v) {
    return v;
  }
};
