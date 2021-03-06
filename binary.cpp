#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include <string>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <limits>

#include <mujs.h>

#include "mytypes.h"
#include "js-support.h"
#include "xerror.h"
#include "Vec3.h"
#include "Mat3.h"

const char *TAG_Binary      = "Binary";

//
// helpers
//
template<typename T>
inline void Append(Binary &b, T val) {
  std::copy((Binary::value_type*)&val, (Binary::value_type*)((&val)+1), std::back_inserter(b));
}

template<typename T>
inline void Put(js_State *J, Binary &b, unsigned off, T val) {
  if (off+sizeof(T) > b.size())
    JS_ERROR("Binary.put: offset=" << off << " is out-of-bounds: arraySize=" << b.size() << ", valueSize=" << sizeof(T))

  *(T*)&b[off] = val;
}

template<typename T>
inline T Get(js_State *J, Binary &b, unsigned off) {
  if (off+sizeof(T) > b.size())
    JS_ERROR("Binary.get: offset=" << off << " is out-of-bounds: arraySize=" << b.size() << ", valueSize=" << sizeof(T))
  return *(T*)&b[off];
}

template<typename Area>
struct ComparatorInc {
  bool operator()(Area &a, Area &b) const {return a.value < b.value;}
};

template<typename Area>
struct ComparatorDecr {
  bool operator()(Area &a, Area &b) const {return a.value > b.value;}
};

template<typename Area>
inline void SortOne(Binary &b, size_t sz, bool orderInc) {
  if (orderInc)
    std::sort((Area*)&b[0], (Area*)&b[sz], ComparatorInc<Area>());
  else
    std::sort((Area*)&b[0], (Area*)&b[sz], ComparatorDecr<Area>());
}

template<unsigned SzM1, typename T, unsigned SzM2>
struct Area {
#pragma pack(push, 1)
  uint8_t m1[SzM1];
  T       value;
  uint8_t m2[SzM2];
#pragma pack(pop)
}; // Area

template<typename T>
inline void Sort(js_State *J, Binary &b, unsigned areaSize, unsigned fldOffset, bool orderInc) {
  auto sz = b.size();

  if (sz % areaSize != 0)
    JS_ERROR("Binary.sortAteasXxx: size=" << sz << " is not a multiple of areaSize=" << areaSize)
  if (fldOffset + sizeof(T) > areaSize)
    JS_ERROR("Binary.sortAteasXxx: fldOffset=" << fldOffset << " + sizeof(T)=" << sizeof(T) << " doesn't fit in the area, areaSize=" << areaSize)

  switch (areaSize) {
  case sizeof(T):      SortOne<Area<0,T,0>>(b, sz, orderInc);   return;
  case sizeof(T)+1: {
    switch (fldOffset) {
    case 0:            SortOne<Area<0,T,1>>(b, sz, orderInc);   return;
    case 1:            SortOne<Area<1,T,0>>(b, sz, orderInc);   return;
    }
    break;
  } case sizeof(T)+2: {
    switch (fldOffset) {
    case 0:            SortOne<Area<0,T,2>>(b, sz, orderInc);   return;
    case 1:            SortOne<Area<1,T,1>>(b, sz, orderInc);   return;
    case 2:            SortOne<Area<2,T,0>>(b, sz, orderInc);   return;
    }
    break;
  } case sizeof(T)+3: {
    switch (fldOffset) {
    case 0:            SortOne<Area<0,T,3>>(b, sz, orderInc);   return;
    case 1:            SortOne<Area<1,T,2>>(b, sz, orderInc);   return;
    case 2:            SortOne<Area<2,T,1>>(b, sz, orderInc);   return;
    case 3:            SortOne<Area<3,T,0>>(b, sz, orderInc);   return;
    }
    break;
  } case sizeof(T)+4: {
    switch (fldOffset) {
    case 0:            SortOne<Area<0,T,4>>(b, sz, orderInc);   return;
    case 1:            SortOne<Area<1,T,3>>(b, sz, orderInc);   return;
    case 2:            SortOne<Area<2,T,2>>(b, sz, orderInc);   return;
    case 3:            SortOne<Area<3,T,1>>(b, sz, orderInc);   return;
    case 4:            SortOne<Area<4,T,0>>(b, sz, orderInc);   return;
    }
    break;
  } case sizeof(T)+8: {
    switch (fldOffset) {
    case 0:            SortOne<Area<0,T,8>>(b, sz, orderInc);   return;
    case 1:            SortOne<Area<1,T,7>>(b, sz, orderInc);   return;
    case 2:            SortOne<Area<2,T,6>>(b, sz, orderInc);   return;
    case 3:            SortOne<Area<3,T,5>>(b, sz, orderInc);   return;
    case 4:            SortOne<Area<4,T,4>>(b, sz, orderInc);   return;
    case 5:            SortOne<Area<5,T,3>>(b, sz, orderInc);   return;
    case 6:            SortOne<Area<6,T,2>>(b, sz, orderInc);   return;
    case 7:            SortOne<Area<7,T,1>>(b, sz, orderInc);   return;
    case 8:            SortOne<Area<8,T,0>>(b, sz, orderInc);   return;
    }
    break;
  }}

  ERROR("Binary::sort: unsupported sizes fldOffset=" << fldOffset << " and areaSize=" << areaSize)
}

template<typename T>
std::vector<std::pair<T,T>> BBox(js_State *J, const Binary &b, unsigned ndims, unsigned leading, unsigned trailing) {
  auto sz = b.size();

  if (sz % (leading + ndims*sizeof(T) + trailing) != 0)
    JS_ERROR("Binary.bboxXx: size=" << sz << " is not a multiple of areaSize=" << (leading + ndims*sizeof(T) + trailing))

  // initialize bb
  std::vector<std::pair<T,T>> bb;
  bb.reserve(ndims);
  while (bb.size() < ndims)
    bb.push_back(std::pair<T,T>(std::numeric_limits<T>::max(), std::numeric_limits<T>::min()));

  for (auto it = b.begin()+leading, ite = b.end(); it < ite;) {
    for (int d = 0; d < ndims; d++) {
      T c = *(T*)&(*it);
      if (c < bb[d].first)
        bb[d].first = c;
      if (c > bb[d].second)
        bb[d].second = c;
      it += sizeof(T);
    }
    it += trailing;
  }

  return bb;
}

template<typename Float>
Binary* CreateMulMat3PlusVec3(js_State *J, const Binary &b, unsigned leading, unsigned trailing, const TMat3<Float> &m, const TVec3<Float> &v) {
  typedef TVec3<Float> V;
  auto sz = b.size();

  if (sz % (leading + sizeof(V) + trailing) != 0)
    JS_ERROR("Binary.createMulMat3PlusVec3Xx: size=" << sz << " is not a multiple of areaSize=" << (leading + sizeof(V) + trailing))

  std::unique_ptr<Binary> output(new Binary);
  output->reserve(sz);
  for (auto it = b.begin(), ite = b.end(); it < ite;) {
    // leading area before coords (FIXME often zero-length, so this is a waste, have a template param for that?)
    for (unsigned i = 0; i < leading; i++)
      output->push_back(*it++);
    // matrix operation
    union {const V *vec; const Float *f;} src;
    uint8_t res[sizeof(V)];
    src.f = (Float*)&*it;
    *(V*)&res[0] = m*(*src.vec) + v;
    for (unsigned i = 0; i < sizeof(V); i++)
      output->push_back(res[i]);
    it += 3*sizeof(Float);
    // trailing bytes
    for (unsigned i = 0; i < trailing; i++)
      output->push_back(*it++);
  }
  return output.release();
}

template<typename Float>
void MulScalarPlusVec3(js_State *J, Binary &b, unsigned leading, unsigned trailing, const TVec3<Float> &scalar, const TVec3<Float> &v) {
  typedef TVec3<Float> V;
  auto sz = b.size();
  auto areaSize = leading + sizeof(V) + trailing;

  if (sz % areaSize != 0)
    JS_ERROR("Binary.bboxXx: size=" << sz << " is not a multiple of areaSize=" << areaSize)

  for (auto it = b.begin() + leading, ite = b.end(); it < ite; it += areaSize) {
    V *vec = (V*)&*it;
    *vec = (*vec).scale(scalar) + v;
  }
}

namespace JsBinding {

namespace JsBinary {

void xnewo(js_State *J, Binary *b) {
  js_getglobal(J, TAG_Binary);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Binary, b, [](js_State *J, void *p) {
    delete (Binary*)p;
  });
}

void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_Binary, [](js_State *J) {
    AssertNargsRange(0,1)
    switch (GetNArgs()) {
    case 0: { // create the empty binary
      ReturnObj(new Binary);
      break;
    } case 1: { // create a binary with a string
      auto b = new Binary;
      auto str = GetArgStringCptr(1);
      auto strLen = ::strlen(str);
      b->resize(strLen);
      ::memcpy(&(*b)[0], str, strLen); // FIXME inefficient copying char* -> Binary, should do this in one step
      ReturnObj(b);
      break;
    }}
  });
  { // methods
    ADD_METHOD_CPP(Binary, dupl, {
      AssertNargs(0)
      ReturnObj(new Binary(*GetArg(Binary, 0)));
    }, 0)
    ADD_METHOD_CPP(Binary, size, {
      AssertNargs(0)
      Return(J, (unsigned)GetArg(Binary, 0)->size());
    }, 0)
    ADD_METHOD_CPP(Binary, resize, {
      AssertNargs(1)
      GetArg(Binary, 0)->resize(GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, clear, {
      AssertNargs(0)
      GetArg(Binary, 0)->clear();
      ReturnVoid(J);
    }, 0)
    // appendXx methods
    ADD_METHOD_CPP(Binary, append, {
      AssertNargs(1)
      auto b = GetArg(Binary, 0);
      auto b1 = GetArg(Binary, 1);
      b->insert(b->end(), b1->begin(), b1->end());
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendRange, { // CAVEAT doesn't check argument #2 (offBegin) and argument #3 (offEnd) for the range to be in the argument 
      AssertNargs(3)
      auto b = GetArg(Binary, 0);
      auto b1 = GetArg(Binary, 1);
      b->insert(b->end(), b1->begin()+GetArgUInt32(2), b1->begin()+GetArgUInt32(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(Binary, appendByte, { // appends the 'int' value as bytes
      AssertNargs(1)
      GetArg(Binary, 0)->push_back((uint8_t)GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendString, {
      AssertNargs(1)
      auto b = GetArg(Binary, 0);
      auto str = GetArgString(1);
      b->insert(b->end(), str.begin(), str.end());
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendInt, { // appends the 'int' value as bytes
      AssertNargs(1)
      Append(*GetArg(Binary, 0), GetArgInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendUInt, { // appends the 'unsigned' value as bytes
      AssertNargs(1)
      Append(*GetArg(Binary, 0), GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendFloat4, { // appends the 'float' value as bytes
      AssertNargs(1)
      Append(*GetArg(Binary, 0), (float)GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendFloat8, { // appends the 'double' value as bytes
      AssertNargs(1)
      Append(*GetArg(Binary, 0), (double)GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    // putXx methods
    ADD_METHOD_CPP(Binary, putByte, {
      AssertNargs(2)
      Put(J, *GetArg(Binary, 0), GetArgUInt32(1), (uint8_t)GetArgUInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putInt, {
      AssertNargs(2)
      Put(J, *GetArg(Binary, 0), GetArgUInt32(1), GetArgInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putUInt, {
      AssertNargs(2)
      Put(J, *GetArg(Binary, 0), GetArgUInt32(1), GetArgUInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putFloat4, {
      AssertNargs(2)
      Put(J, *GetArg(Binary, 0), GetArgUInt32(1), (float)GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putFloat8, {
      AssertNargs(2)
      Put(J, *GetArg(Binary, 0), GetArgUInt32(1), (double)GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    // getXx methods
    ADD_METHOD_CPP(Binary, getByte, {
      AssertNargs(1)
      Return(J, Get<uint8_t>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getChar, {
      AssertNargs(1)
      Return(J, Get<char>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getInt, {
      AssertNargs(1)
      Return(J, Get<int>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getUInt, {
      AssertNargs(1)
      Return(J, Get<unsigned>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getFloat4, {
      AssertNargs(1)
      Return(J, Get<float>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getFloat8, {
      AssertNargs(1)
      Return(J, Get<double>(J, *GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    // find
    ADD_METHOD_CPP(Binary, findChar, { // XXX does not check argument correctness
      AssertNargs(2)
      auto b = GetArg(Binary, 0);
      auto off = GetArgUInt32(1);
      auto p = memchr(&(*b)[off], GetArgChar(2), b->size() - off);
      Return(J, p ? (int)((uint8_t*)p - &(*b)[0]) : -1);
    }, 2)
    // sort
    ADD_METHOD_CPP(Binary, sortAreasByFloat4Field, {
      AssertNargs(3)
      Sort<float>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // areaSize
        GetArgUInt32(2), // fldOffset
        GetArgBoolean(3) // orderInc
      );
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(Binary, sortAreasByFloat8Field, {
      AssertNargs(3)
      Sort<double>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // areaSize
        GetArgUInt32(2), // fldOffset
        GetArgBoolean(3) // orderInc
      );
      ReturnVoid(J);
    }, 3)
    //
    ADD_METHOD_CPP(Binary, concatenate, {
      AssertNargs(1)
      auto b = new Binary(*GetArg(Binary, 0));
      auto b1 = GetArg(Binary, 1);
      b->insert(b->end(), b1->begin(), b1->end());
      ReturnObj(b);
    }, 1)
    ADD_METHOD_CPP(Binary, toString, {
      AssertNargs(0)
      auto b = GetArg(Binary, 0);
      Return(J, std::string(b->begin(), b->end()));
    }, 0)
    ADD_METHOD_CPP(Binary, getSubString, {
      AssertNargs(2)
      auto b = GetArg(Binary, 0);
      Return(J, std::string(b->begin()+GetArgUInt32(1), b->begin()+GetArgUInt32(2)));
    }, 2)
    ADD_METHOD_CPP(Binary, toFile, {
      AssertNargs(1)
      std::ofstream file(GetArgString(1), std::ios::out | std::ios::binary);
      auto b = GetArg(Binary, 0);
      file.write((char*)&(*b)[0], b->size());
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, bboxFloat4, {
      AssertNargs(3)
      Return(J, BBox<float>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // ndims
        GetArgUInt32(2), // leading
        GetArgUInt32(3)  // trailing
      ));
    }, 3)
    ADD_METHOD_CPP(Binary, bboxFloat8, {
      AssertNargs(3)
      Return(J, BBox<double>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // ndims
        GetArgUInt32(2), // leading
        GetArgUInt32(3)  // trailing
      ));
    }, 3)
    ADD_METHOD_CPP(Binary, createMulMat3PlusVec3Float4, {
      AssertNargs(4)
      ReturnObj(CreateMulMat3PlusVec3<float>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1),      // leading
        GetArgUInt32(2),      // trailing
        Mat3ToType<double, float>::convert(GetArgMat3x3(3)), // M
        Vec3ToType<double, float>::convert(GetArgVec3(4))    // V
      ));
    }, 4)
    ADD_METHOD_CPP(Binary, createMulMat3PlusVec3Float8, {
      AssertNargs(4)
      ReturnObj(CreateMulMat3PlusVec3<double>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // leading
        GetArgUInt32(2), // trailing
        GetArgMat3x3(3), // M
        GetArgVec3(4)    // V
      ));
    }, 4)
    ADD_METHOD_CPP(Binary, mulScalarPlusVec3Float4, {
      AssertNargs(4)
      MulScalarPlusVec3<float>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1),                                  // leading
        GetArgUInt32(2),                                  // trailing
        Vec3ToType<double,float>::convert(GetArgVec3(3)), // Scalar
        Vec3ToType<double,float>::convert(GetArgVec3(4))  // Vec
      );
      ReturnVoid(J);
    }, 4)
    ADD_METHOD_CPP(Binary, mulScalarPlusVec3Float8, {
      AssertNargs(4)
      MulScalarPlusVec3<double>(J,
        *GetArg(Binary, 0),
        GetArgUInt32(1), // leading
        GetArgUInt32(2), // trailing
        GetArgVec3(3),   // Scalar
        GetArgVec3(4)    // Vec
      );
      ReturnVoid(J);
    }, 4)
  }
  JsSupport::endDefineClass(J);
}

} // JsBinary

} // JsBinding
