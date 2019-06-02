
#include "js-support.h"
#include "mytypes.h"
#include "xerror.h"
#include "Mat3.h"
#include "Vec3.h"

#include <iostream>
#include <vector>
#include <limits>
#include <sstream>
#include <memory>

#include <mujs.h>

const char *TAG_FloatArray4      = "FloatArray4"; // non-static: used externally
const char *TAG_FloatArray8      = "FloatArray8"; // non-static: used externally

/*
// CastVec3To/CastMat3To
template<typename ToFloat>
const Vec3<ToFloat>& CastVec3To(const Vec3 &v);
template<>
const Vec3<double>& CastVec3To(const Vec3 &v) {return v;}
template<>
const Vec3<float>& CastVec3To(const Vec3 &v) {return v;}
*/

#define CastGetArgVec3(n) Vec3ToType<double,Float>::convert(GetArgVec3(n))
#define CastGetArgMat3x3(n) Mat3ToType<double,Float>::convert(GetArgMat3x3(n))

// FloatArray: "accelerated" array of double-size floating point numbers
template<typename Float>
class FloatArray : public std::vector<Float> {
  typedef std::vector<Float> V;
public:
  FloatArray() { } // created with size 0
  FloatArray(const FloatArray &other) : V(other) { } // copy constructor
  void muln(Float m) {
    for (auto &f : *this)
      f *= m;
  }
  void divn(Float m) {
    for (auto &f : *this)
      f /= m;
  }
  void mulMat3PlusVec3(const TMat3<Float> &m, const TVec3<Float> &v) {
    if (V::size() % 3 != 0)
      ERROR("FloatArray.mulMat3PlusVec3: size=" << V::size() << " isn't a multiple of 3")
    for (auto it = V::begin(), ite = V::end(); it != ite; it += 3) {
      union {TVec3<Float> *vec; Float *f;};
      f = &*it;
      *vec = m*(*vec) + v;
    }
  }
  FloatArray* createMulMat3PlusVec3(const TMat3<Float> &m, const TVec3<Float> &v) {
    if (V::size() % 3 != 0)
      ERROR("FloatArray.mulMat3PlusVec3: size=" << V::size() << " isn't a multiple of 3")
    std::unique_ptr<FloatArray> output(new FloatArray);
    output->reserve(V::size());
    for (auto it = V::begin(), ite = V::end(); it != ite; it += 3) {
      union {TVec3<Float> *vec; Float *f;};
      f = &*it;
      auto val = m*(*vec) + v;
      output->push_back(val(X));
      output->push_back(val(Y));
      output->push_back(val(Z));
    }
    return output.release();
  }
  void mulScalarPlusVec3(const TVec3<Float> &scalar, const TVec3<Float> &v) {
    if (V::size() % 3 != 0)
      ERROR("FloatArray.mulScalarPlusVec3: size=" << V::size() << " isn't a multiple of 3")
    for (auto it = V::begin(), ite = V::end(); it != ite; it += 3) {
      union {TVec3<Float> *vec; Float *f;};
      f = &*it;
      *vec = (*vec).scale(scalar) + v;
    }
  }
  std::vector<std::pair<Float,Float>> bbox(unsigned dim) const {
    // checks
    if (V::empty())
      ERROR("FloatArray.bbox: computing the bbox of an empty arry doesn't make sense");
    if (V::size() % dim != 0)
      ERROR("FloatArray.bbox: size=" << V::size() << " isn't a multiple of a requested dim=" << dim)

    // initialize bb
    std::vector<std::pair<Float,Float>> bb;
    bb.reserve(dim);
    while (bb.size() < dim)
      bb.push_back(std::pair<Float,Float>(std::numeric_limits<Float>::max(), std::numeric_limits<Float>::min()));

    // compute bbox
    auto ib = bb.begin(), ie = bb.end(), i = ib;
    for (auto v : *this) {
      if (v < i->first)
        i->first = v;
      if (v > i->second)
        i->second = v;
      if (++i == ie)
        i = ib;
    }
      
    return bb;
  }
}; // FloatArray


namespace JsBinding {

namespace JsFloatArray {

template<typename Float> const char* tag();
template<> const char* tag<float>() {return TAG_FloatArray4;}
template<> const char* tag<double>() {return TAG_FloatArray8;}

template<typename Float> const char* cls();
template<> const char* cls<float>() {return "FloatArray4";}
template<> const char* cls<double>() {return "FloatArray8";}

template<typename Float>
static void xnewo(js_State *J, FloatArray<Float> *d) {
  js_getglobal(J, tag<Float>());
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, tag<Float>(), d, [](js_State *J, void *p) {
    delete (FloatArray<Float>*)p;
 });
}

template<typename Float>
void init(js_State *J) {
  JsSupport::beginDefineClass(J, tag<Float>(), [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new FloatArray<Float>);
  });
  { // methods
    ADD_METHOD_CPPc(cls<Float>(), toString, {
      AssertNargs(0)
      std::ostringstream ss;
      ss << "[";
      bool fst = true;
      for (auto v : *GetArgExt(FloatArray<Float>, tag<Float>(), 0)) {
        if (!fst)
          ss << ",";
        ss << v;
        fst = false;
      }
      ss << "]";
      Return(J, ss.str());
    }, 0)
    ADD_METHOD_CPPc(cls<Float>(), size, {
      AssertNargs(0)
      Return(J, (unsigned)GetArgExt(FloatArray<Float>, tag<Float>(), 0)->size());
    }, 0)
    ADD_METHOD_CPPc(cls<Float>(), copy, {
      AssertNargs(0)
      ReturnObj(new FloatArray<Float>(*GetArgExt(FloatArray<Float>, tag<Float>(), 0)));
    }, 0)
    ADD_METHOD_CPPc(cls<Float>(), resize, {
      AssertNargs(1)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->resize(GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPPc(cls<Float>(), append, {
      AssertNargs(1)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->push_back(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPPc(cls<Float>(), append2, {
      AssertNargs(2)
      auto a = GetArgExt(FloatArray<Float>, tag<Float>(), 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPPc(cls<Float>(), append3, {
      AssertNargs(3)
      auto a = GetArgExt(FloatArray<Float>, tag<Float>(), 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      a->push_back(GetArgFloat(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPPc(cls<Float>(), append4, {
      AssertNargs(4)
      auto a = GetArgExt(FloatArray<Float>, tag<Float>(), 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      a->push_back(GetArgFloat(3));
      a->push_back(GetArgFloat(4));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPPc(cls<Float>(), get, {
      AssertNargs(1)
      Return(J, (*GetArgExt(FloatArray<Float>, tag<Float>(), 0))[GetArgUInt32(1)]);
    }, 1)
    ADD_METHOD_CPPc(cls<Float>(), set, {
      AssertNargs(2)
      (*GetArgExt(FloatArray<Float>, tag<Float>(), 0))[GetArgUInt32(1)] = GetArgFloat(2);
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPPc(cls<Float>(), muln, {
      AssertNargs(1)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->muln(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPPc(cls<Float>(), divn, {
      AssertNargs(1)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->divn(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPPc(cls<Float>(), mulMat3PlusVec3, {
      AssertNargs(2)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->mulMat3PlusVec3(CastGetArgMat3x3(1), CastGetArgVec3(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPPc(cls<Float>(), createMulMat3PlusVec3, {
      AssertNargs(2)
      ReturnObj(GetArgExt(FloatArray<Float>, tag<Float>(), 0)->createMulMat3PlusVec3(CastGetArgMat3x3(1), CastGetArgVec3(2)));
    }, 2)
    ADD_METHOD_CPPc(cls<Float>(), mulScalarPlusVec3, {
      AssertNargs(2)
      GetArgExt(FloatArray<Float>, tag<Float>(), 0)->mulScalarPlusVec3(CastGetArgVec3(1), CastGetArgVec3(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPPc(cls<Float>(), bbox, {
      AssertNargs(1)
      Return(J, GetArgExt(FloatArray<Float>, tag<Float>(), 0)->bbox(GetArgUInt32(1)));
    }, 1)
  }
  JsSupport::endDefineClass(J);
}

void initFloat4(js_State *J) {
  init<float>(J);
}
void initFloat8(js_State *J) {
  init<double>(J);
}

} // JsFloatArray

} // JsBinding
