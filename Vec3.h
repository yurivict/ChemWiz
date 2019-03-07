#pragma once

#include <boost/array.hpp>
#include <ostream>
#include <cmath>

typedef double Float;

/// Vector helper classes
enum VecCoords {X=1, Y=2, Z=3};

class Vec3 : public boost::array<Float, 3> {
public:
  typedef Float val_t;
  Vec3() { }
  Vec3(Float x, Float y, Float z) : boost::array<Float, 3>({{x, y, z}}) { }
  Float& operator()(unsigned idx) { // 1-based index access
    return (*this)[idx-1];
  }
  Float operator()(unsigned idx) const { // 1-based index access
    return (*this)[idx-1];
  }
  static Vec3 zero() {return Vec3(0,0,0);}
  static Vec3 one(unsigned idx, Float val) {Vec3 vec(0,0,0); vec(idx) = val; return vec;}
  static bool almostEquals(const Vec3 &v1, const Vec3 &v2, Float eps)
    {return almostEquals(v1(X),v2(X),eps) && almostEquals(v1(Y),v2(Y),eps) && almostEquals(v1(Z),v2(Z),eps);}
  static bool almostEquals(Float c1, Float c2, Float eps)
    {return std::fabs(c1 - c2) <= eps;}
  Float len2() const {return sq((*this)(X)) + sq((*this)(Y)) + sq((*this)(Z));}
  Float len() const {return sqrt(len2());}
  Vec3 normalize() const {
    return (*this)/len();
  }
  Vec3 normalizeZ(Float l) const { // expects the pre-computed length argument for efficiency
    if (l != 0)
      return (*this)/l;
    else
      return Vec3(0,0,0);
  }
  Vec3 normalizeZ() const {
    return normalizeZ(len());
  }
  Vec3 normalizeZl(Float outLen) const {
    auto l = len();
    outLen = l;
    if (l != 0)
      return (*this)/l;
    else
      return Vec3(0,0,0);
  }
  void snapToGrid(const Vec3 &grid) {
    Vec3 &i = *this;
    snapToGrid(i(X), grid(X));
    snapToGrid(i(Y), grid(Y));
    snapToGrid(i(Z), grid(Z));
  }
  static void snapToGrid(Float &c, Float g) {
    c = int(c/g)*g;
  }
  friend std::ostream& operator<<(std::ostream &os, const Vec3 &v) {
    os << "{" << v(X) << "," << v(Y) << "," << v(Z) << "}";
    return os;
  }
  Vec3 operator-() const {
    const Vec3 &i = *this;
    return Vec3(-i(X), -i(Y), -i(Z));
  }
  Vec3 operator*(Float m) const {
    const Vec3 &i = *this;
    return Vec3(i(X)*m, i(Y)*m, i(Z)*m);
  }
  Float operator*(const Vec3 &v) const { // scalar multiplication
    const Vec3 &i = *this;
    return i(X)*v(X) + i(Y)*v(Y) + i(Z)*v(Z);
  }
  Vec3 cross(const Vec3 &v) const { // cross (vector) multiplication
    const Vec3 &i = *this;
    return Vec3(-i(Z)*v(Y) + i(Y)*v(Z), i(Z)*v(X) - i(X)*v(Z), -i(Y)*v(X) + i(X)*v(Y)); // see https://en.wikipedia.org/wiki/Cross_product#Conversion_to_matrix_multiplication
  }
  Float angle(const Vec3 &v) const {
    return std::acos((*this)*v/(this->len()*v.len()));
  }
  Vec3 operator/(Float m) const {
    const Vec3 &i = *this;
    return Vec3(i(X)/m, i(Y)/m, i(Z)/m);
  }
  Vec3 operator+(const Vec3 &v) const {
    const Vec3 &i = *this;
    return Vec3(i(X)+v(X), i(Y)+v(Y), i(Z)+v(Z));
  }
  Vec3 operator-(const Vec3 &v) const {
    const Vec3 &i = *this;
    return Vec3(i(X)-v(X), i(Y)-v(Y), i(Z)-v(Z));
  }
  Vec3& operator-=(const Vec3 &v) {
    Vec3 &i = *this;
    i(X) -= v(X);
    i(Y) -= v(Y);
    i(Z) -= v(Z);
    return *this;
  }
  Vec3 project(const Vec3 &dir) const { // dir is assimed to be a normal vector or zero vector
    return dir*((*this)*dir);
  }
  Vec3 orthogonal(const Vec3 &dir) const { // dir is assimed to be a normal vector or zero vector
    return *this - project(dir);
  }
  bool isParallel(const Vec3 &other) const {return is(normalize()*other.normalize(), 1);}
  bool isOrthogonal(const Vec3 &other) const {return is(cross(other).len2(), 1);}
  Vec3 divOneByOne(const Vec3 &d) const { // dir is assimed to be a normal vector or zero vector
    const Vec3 &i = *this;
    return Vec3(i(X)/d(X), i(Y)/d(Y), i(Z)/d(Z));
  }
  Vec3 orthoComponentTo(const Vec3 &v) const { // v is assumed to be normalized
    return *this - this->project(v);
  }
  static bool is(Float f1, Float f2) {return std::abs(f1-f2) < 0.001;} // tests how close are numbers
private:
  static Float sq(Float f) {return f*f;}
};
