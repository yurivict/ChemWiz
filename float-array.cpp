
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

const char *TAG_FloatArray         = "FloatArray"; // non-static: used externally

// FloatArray: "accelerated" array of double-size floating point numbers
class FloatArray : public std::vector<Float> {
public:
  FloatArray() { } // created with size 0
  FloatArray(const FloatArray &other) : std::vector<Float>(other) { } // copy constructor
  void muln(Float m) {
    //using namespace simdpp;
    //float64<2>
    for (auto &f : *this)
      f *= m;
  }
  void divn(Float m) {
    for (auto &f : *this)
      f /= m;
  }
  void mulMat3PlusVec3(const Mat3 &m, const Vec3 &v) {
    if (size() % 3 != 0)
      ERROR("FloatArray.mulMat3PlusVec3: size=" << size() << " isn't a multiple of 3")
    for (auto it = begin(), ite = end(); it != ite; it += 3) {
      union {Vec3 *vec; Float *f;};
      f = &*it;
      *vec = m*(*vec) + v;
    }
  }
  FloatArray* createMulMat3PlusVec3(const Mat3 &m, const Vec3 &v) {
    if (size() % 3 != 0)
      ERROR("FloatArray.mulMat3PlusVec3: size=" << size() << " isn't a multiple of 3")
    std::unique_ptr<FloatArray> output(new FloatArray);
    output->reserve(size());
    for (auto it = begin(), ite = end(); it != ite; it += 3) {
      union {Vec3 *vec; Float *f;};
      f = &*it;
      auto val = m*(*vec) + v;
      output->push_back(val(X));
      output->push_back(val(Y));
      output->push_back(val(Z));
    }
    std::cout << "FloatArray.mulMat3PlusVec3: output.size=" << output->size() << std::endl;
    return output.release();
  }
  void mulScalarPlusVec3(const Vec3 &scalar, const Vec3 &v) {
    if (size() % 3 != 0)
      ERROR("FloatArray.mulScalarPlusVec3: size=" << size() << " isn't a multiple of 3")
    for (auto it = begin(), ite = end(); it != ite; it += 3) {
      union {Vec3 *vec; Float *f;};
      f = &*it;
      *vec = (*vec).scale(scalar) + v;
    }
  }
  std::vector<std::pair<Float,Float>> bbox(unsigned dim) const {
    // checks
    if (empty())
      ERROR("FloatArray.bbox: computing the bbox of an empty arry doesn't make sense");
    if (size() % dim != 0)
      ERROR("FloatArray.bbox: size=" << size() << " isn't a multiple of a requested dim=" << dim)

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

static void xnewo(js_State *J, FloatArray *d) {
  js_getglobal(J, TAG_FloatArray);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_FloatArray, d, [](js_State *J, void *p) {
    delete (FloatArray*)p;
 });
}

static void xnew(js_State *J) {
  AssertNargs(0)
  ReturnObj(FloatArray, new FloatArray);
}

void init(js_State *J) {
  JsSupport::InitObjectRegistry(J, TAG_FloatArray);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(FloatArray)
  js_getglobal(J, TAG_FloatArray);
  js_getproperty(J, -1, "prototype");
  JsSupport::StackPopPrevious(J);
  { // methods
    ADD_METHOD_CPP(FloatArray, toString, {
      AssertNargs(0)
      std::ostringstream ss;
      ss << "[";
      bool fst = true;
      for (auto v : *GetArg(FloatArray, 0)) {
        if (!fst)
          ss << ",";
        ss << v;
        fst = false;
      }
      ss << "]";
      Return(J, ss.str());
    }, 0)
    ADD_METHOD_CPP(FloatArray, size, {
      AssertNargs(0)
      Return(J, (unsigned)GetArg(FloatArray, 0)->size());
    }, 0)
    ADD_METHOD_CPP(FloatArray, copy, {
      AssertNargs(0)
      ReturnObj(FloatArray, new FloatArray(*GetArg(FloatArray, 0)));
    }, 0)
    ADD_METHOD_CPP(FloatArray, resize, {
      AssertNargs(1)
      GetArg(FloatArray, 0)->resize(GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(FloatArray, append, {
      AssertNargs(1)
      GetArg(FloatArray, 0)->push_back(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(FloatArray, append2, {
      AssertNargs(2)
      auto a = GetArg(FloatArray, 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(FloatArray, append3, {
      AssertNargs(3)
      auto a = GetArg(FloatArray, 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      a->push_back(GetArgFloat(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(FloatArray, append4, {
      AssertNargs(4)
      auto a = GetArg(FloatArray, 0);
      a->push_back(GetArgFloat(1));
      a->push_back(GetArgFloat(2));
      a->push_back(GetArgFloat(3));
      a->push_back(GetArgFloat(4));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(FloatArray, get, {
      AssertNargs(1)
      Return(J, (*GetArg(FloatArray, 0))[GetArgUInt32(1)]);
    }, 1)
    ADD_METHOD_CPP(FloatArray, set, {
      AssertNargs(2)
      (*GetArg(FloatArray, 0))[GetArgUInt32(1)] = GetArgFloat(2);
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(FloatArray, muln, {
      AssertNargs(1)
      GetArg(FloatArray, 0)->muln(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(FloatArray, divn, {
      AssertNargs(1)
      GetArg(FloatArray, 0)->divn(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(FloatArray, mulMat3PlusVec3, {
      AssertNargs(2)
      GetArg(FloatArray, 0)->mulMat3PlusVec3(GetArgMat3x3(1), GetArgVec3(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(FloatArray, createMulMat3PlusVec3, {
      AssertNargs(2)
      ReturnObj(FloatArray, GetArg(FloatArray, 0)->createMulMat3PlusVec3(GetArgMat3x3(1), GetArgVec3(2)));
    }, 2)
    ADD_METHOD_CPP(FloatArray, mulScalarPlusVec3, {
      AssertNargs(2)
      GetArg(FloatArray, 0)->mulScalarPlusVec3(GetArgVec3(1), GetArgVec3(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(FloatArray, bbox, {
      AssertNargs(1)
      Return(J, GetArg(FloatArray, 0)->bbox(GetArgUInt32(1)));
    }, 1)
  }
  js_pop(J, 2);
}

} // JsFloatArray

} // JsBinding
