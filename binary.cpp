#include <string.h>

#include <string>
#include <fstream>

#include <mujs.h>

#include "mytypes.h"
#include "js-support.h"

const char *TAG_Binary      = "Binary";

//
// helpers
//
template<typename T>
inline void BinaryAppend(Binary &b, T arg) {
  auto sz = b.size();
  b.reserve(sz + sizeof(arg));
  *(decltype(arg)*)&b[sz] = arg;
  b.resize(sz + sizeof(arg));
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
    AssertNargs(1)
    auto b = new Binary;
    auto str = GetArgStringCptr(1);
    auto strLen = ::strlen(str);
    b->resize(strLen);
    ::memcpy(&(*b)[0], str, strLen); // FIXME inefficient copying char* -> Binary, should do this in one step
    ReturnObj(b);
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
      BinaryAppend(*GetArg(Binary, 0), GetArgInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendUInt, { // appends the 'unsigned' value as bytes
      AssertNargs(1)
      BinaryAppend(*GetArg(Binary, 0), GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendFloat4, { // appends the 'float' value as bytes
      AssertNargs(1)
      BinaryAppend(*GetArg(Binary, 0), (float)GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Binary, appendFloat8, { // appends the 'double' value as bytes
      AssertNargs(1)
      BinaryAppend(*GetArg(Binary, 0), (double)GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
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
