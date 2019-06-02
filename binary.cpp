#include <string.h>
#include <assert.h>

#include <string>
#include <fstream>
#include <algorithm>

#include <mujs.h>

#include "mytypes.h"
#include "js-support.h"
#include "xerror.h"

const char *TAG_Binary      = "Binary";

//
// helpers
//
template<typename T>
inline void Append(Binary &b, T val) {
  auto sz = b.size();
  b.resize(sz + sizeof(T));
  *(T*)&b[sz] = val;
}

template<typename T>
inline void Put(Binary &b, unsigned off, T val) {
  if (off+sizeof(T) > b.size())
    ERROR("Binary.put offset=" << off << " is out-of-bounds: arraySize=" << b.size() << ", valueSize=" << sizeof(T))
  *(T*)&b[off] = val;
}

template<typename T>
inline T Get(Binary &b, unsigned off) {
  if (off+sizeof(T) > b.size())
    ERROR("Binary.get offset=" << off << " is out-of-bounds: arraySize=" << b.size() << ", valueSize=" << sizeof(T))
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
  uint8_t m1[SzM1];
  T value;
  uint8_t m2[SzM2];
}; // Area

template<typename T>
inline void Sort(Binary &b, unsigned areaSize, unsigned fldOffset, bool orderInc) {
  auto sz = b.size();
  assert(sz % areaSize == 0 && fldOffset + sizeof(T) <= areaSize);

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
    // appendXx methods
    ADD_METHOD_CPP(Binary, append, {
      AssertNargs(1)
      auto b = GetArg(Binary, 0);
      auto b1 = GetArg(Binary, 1);
      b->insert(b->end(), b1->begin(), b1->end());
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
      Put(*GetArg(Binary, 0), GetArgUInt32(1), (uint8_t)GetArgUInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putInt, {
      AssertNargs(2)
      Put(*GetArg(Binary, 0), GetArgUInt32(1), GetArgInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putUInt, {
      AssertNargs(2)
      Put(*GetArg(Binary, 0), GetArgUInt32(1), GetArgUInt32(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putFloat4, {
      AssertNargs(2)
      Put(*GetArg(Binary, 0), GetArgUInt32(1), (float)GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Binary, putFloat8, {
      AssertNargs(2)
      Put(*GetArg(Binary, 0), GetArgUInt32(1), (double)GetArgFloat(2));
      ReturnVoid(J);
    }, 2)
    // getXx methods
    ADD_METHOD_CPP(Binary, getByte, {
      AssertNargs(1)
      Return(J, Get<uint8_t>(*GetArg(Binary, 0), GetArgUInt32(1)));
    }, 2)
    ADD_METHOD_CPP(Binary, getInt, {
      AssertNargs(1)
      Return(J, Get<int>(*GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getUInt, {
      AssertNargs(1)
      Return(J, Get<unsigned>(*GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getFloat4, {
      AssertNargs(1)
      Return(J, Get<float>(*GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Binary, getFloat8, {
      AssertNargs(1)
      Return(J, Get<double>(*GetArg(Binary, 0), GetArgUInt32(1)));
    }, 1)
    // sort
    ADD_METHOD_CPP(Binary, sortAreasByFloat4Field, {
      AssertNargs(3)
      Sort<float>(
        *GetArg(Binary, 0),
        GetArgUInt32(1), // areaSize
        GetArgUInt32(2), // fldOffset
        GetArgBoolean(3) // orderInc
      );
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(Binary, sortAreasByFloat8Field, {
      AssertNargs(3)
      Sort<double>(
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
    ADD_METHOD_CPP(Binary, toFile, {
      AssertNargs(1)
      std::ofstream file(GetArgString(1), std::ios::out | std::ios::binary);
      auto b = GetArg(Binary, 0);
      file.write((char*)&(*b)[0], b->size());
      ReturnVoid(J);
    }, 1)
  }
  JsSupport::endDefineClass(J);
}

} // JsBinary

} // JsBinding
