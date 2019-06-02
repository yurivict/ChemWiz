#pragma once

#include "mytypes.h"
#include "Vec3.h"
#include "Mat3.h"

#include <string>
#include <utility> // for pair
#include <vector>
#include <array>
#include <functional>

#include <assert.h>

#include <mujs.h>

//
// JsSupport contains various support routines for binding various object types to JS
//
// It assumes that the presence of "const char* TAG_{cls}", "Js{cls}::xnew", etc in the module where it is included in
//

class JsSupport {
public:
  static void beginDefineClass(js_State *J, const char *clsTag, js_CFunction newFun);
  static void endDefineClass(js_State *J);
  static void beginNamespace(js_State *J, const char *nsNameStr);
  static void endNamespace(js_State *J, const char *nsNameStr);
  static void addJsConstructor(js_State *J, const char *clsTag, js_CFunction newFun);
  static void addMethodCpp(js_State *J, const char *cls, const char *methodStr, js_CFunction methodFun, unsigned nargs);
  static void addMethodJs(js_State *J, const char *cls, const char *methodStr, const char *codeStr);
  static void addJsFunction(js_State *J, const char *funcNameStr, js_CFunction funcFun, unsigned nargs);
  static void addNsFunctionCpp(js_State *J, const char *nsNameStr, const char *jsfnNameStr, js_CFunction funcFun, unsigned nargs);
  static void addNsFunctionJs(js_State *J, const char *nsNameStr, const char *fnNameStr, const char *codeStr);
private:
  static void initObjectRegistry(js_State *J, const char *objTag);
  static void popPreviousStackElement(js_State *J);
}; // JsSupport

// convenience macros that come with JsSupport

#define BEGIN_NAMESPACE(ns) \
  JsSupport::beginNamespace(J, #ns);

#define END_NAMESPACE(ns) \
  JsSupport::endNamespace(J, #ns);

// add method defined by the C++ code
#define ADD_METHOD_CPP(cls, methodName, methodBody, nargs) \
  JsSupport::addMethodCpp(J, #cls, #methodName, [](js_State *J) methodBody,  nargs);
#define ADD_METHOD_CPPc(cls, methodName, methodBody, nargs) \
  JsSupport::addMethodCpp(J, cls, #methodName, [](js_State *J) methodBody,  nargs);

// add method defined in JavaScript
#define ADD_METHOD_JS(cls, method, code...) \
  JsSupport::addMethodJs(J, #cls, #method, #code);

#define ADD_JS_FUNCTION(name, nargs) \
  JsSupport::addJsFunction(J, #name, JsBinding::name, nargs);

#define ADD_NS_FUNCTION_CPP(ns, jsfn, cfn, nargs) \
  JsSupport::addNsFunctionCpp(J, #ns, #jsfn, cfn, nargs);

#define ADD_NS_FUNCTION_JS(ns, func, code...) \
  JsSupport::addNsFunctionJs(J, #ns, #func, #code);

#define AssertNargs(n)           assert(js_gettop(J) == n+1);
#define AssertNargsRange(n1,n2)  assert(n1+1 <= js_gettop(J) && js_gettop(J) <= n2+1);
#define AssertNargs2(nmin,nmax)  assert(nmin+1 <= js_gettop(J) && js_gettop(J) <= nmax+1);
#define AssertStack(n)           assert(js_gettop(J) == n);

//
// helper classes and functions
//

namespace JsBinding {

class StrPtr { // class that maps pointer<->string for the purpose of passing pointers to JS
public:
  static void* s2p(const std::string &s) {
    void *ptr = 0;
    ::sscanf(s.c_str(), "%p", &ptr);
    return ptr;
  }
  static std::string p2s(void *ptr) {
    char buf[sizeof(void*)*2+1];
    ::sprintf(buf, "%p", ptr);
    return buf;
  }
}; // StrPtr

// complex argument support
Vec3 objToVec3(js_State *J, int idx, const char *fname);
Mat3 objToMat3x3(js_State *J, int idx, const char *fname);

}; // JsBinding

//
// Return helpers
//

inline void Push(js_State *J, const bool &val) {
  js_pushboolean(J, val ? 1 : 0);
}

inline void Push(js_State *J, const Float &val) {
  js_pushnumber(J, val);
}

inline void Push(js_State *J, const unsigned &val) {
  js_pushnumber(J, val);
}

inline void Push(js_State *J, const int &val) {
  js_pushnumber(J, val);
}

inline void Push(js_State *J, const std::string &val) {
  js_pushstring(J, val.c_str());
}

inline void Push(js_State *J, const Vec3 &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto c : val) {
    js_pushnumber(J, c);
    js_setindex(J, -2, idx++);
  }
}

inline void Push(js_State *J, const Mat3 &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto &r : val) {
    Push(J, r);
    js_setindex(J, -2, idx++);
  }
}

inline void Push(js_State *J, void* const &val) {
  Push(J, JsBinding::StrPtr::p2s(val));
}

template<typename T, std::size_t SZ>
inline void Push(js_State *J, const std::array<T, SZ> &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto const &v : val) {
    Push(J, v);
    js_setindex(J, -2, idx++);
  }
}

template<typename T>
inline void Push(js_State *J, const std::vector<T> &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto const &v : val) {
    Push(J, v);
    js_setindex(J, -2, idx++);
  }
}

template<typename T>
inline void Push(js_State *J, const std::pair<T,T> &val) {
  js_newarray(J);
  Push(J, val.first);
  js_setindex(J, -2, 0/*idx*/);
  Push(J, val.second);
  js_setindex(J, -2, 1/*idx*/);
}

template<typename T>
inline void Return(js_State *J, const T &val) {
  Push(J, val);
}

inline void ReturnVoid(js_State *J) {
  js_pushundefined(J);
}

//inline void ReturnNull(js_State *J) {
//  js_pushnull(J);
//}

//
// convenience macros to return object
//
#define ReturnObj(v) xnewo(J, v)
#define ReturnObjExt(type, v) Js##type::xnewo(J, v)
#define ReturnObjZ(v) xnewoZ(J, v)
#define ReturnObjZExt(type, v) Js##type::xnewoZ(J, v)


//
// convenience macro to access arguments
//

#define GetNArgs()               (js_gettop(J)-1)
#define GetArg(type, n)          ((type*)js_touserdata(J, n, TAG_##type))
#define GetArgExt(type, tag, n)  ((type*)js_touserdata(J, n, tag))
#define GetArgZ(type, n)         (!js_isundefined(J, n) ? (type*)js_touserdata(J, n, TAG_##type) : nullptr)
#define GetArgBoolean(n)         js_toboolean(J, n)
#define GetArgFloat(n)           js_tonumber(J, n)
#define GetArgString(n)          std::string(js_tostring(J, n))
#define GetArgStringCptr(n)      js_tostring(J, n)
#define GetArgInt32(n)           js_toint32(J, n)
#define GetArgUInt32(n)          js_touint32(J, n)

// complex arguments need yet to be templetized

#define GetArgVec3(n)            JsBinding::objToVec3(J, n, __func__)
#define GetArgMat3x3(n)          JsBinding::objToMat3x3(J, n, __func__)

