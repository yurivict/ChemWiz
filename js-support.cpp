#include "js-support.h"
#include "xerror.h"

#include <iostream>

#include <boost/format.hpp>

#include <mujs.h>

#include <string.h>

// simple macros are inline-like
#define AssertStack(n)           assert(js_gettop(J) == n);

// static functions
static void ckErr(js_State *J, int err) {
  if (err) {
    std::cerr << js_trystring(J, -1, "Error") << std::endl;
    js_pop(J, 1);
    abort();
  }
}

// code borrowed from the MuJS sources
namespace MuJS {

static void jsB_propf(js_State *J, const char *name, js_CFunction cfun, int n) {
  const char *pname = strrchr(name, '.');
  pname = pname ? pname + 1 : name;
  js_newcfunction(J, cfun, name, n);
  js_defproperty(J, -2, pname, JS_DONTENUM);
}

} // MuJS


// JsSupport methods

void JsSupport::beginDefineClass(js_State *J, const char *clsTag, js_CFunction newFun) {
  initObjectRegistry(J, clsTag);
  js_pushglobal(J);
  addJsConstructor(J, clsTag, newFun);
  js_getglobal(J, clsTag);
  js_getproperty(J, -1, "prototype");
  popPreviousStackElement(J);
  // returns at 2 slots down in stack
}

void JsSupport::endDefineClass(js_State *J) {
  js_pop(J, 2);
  AssertStack(0);
}

void JsSupport::beginNamespace(js_State *J, const char *nsNameStr) {
  js_newobject(J);
}

void JsSupport::endNamespace(js_State *J, const char *nsNameStr) {
  js_defglobal(J, nsNameStr, JS_DONTENUM);
}

void JsSupport::addJsConstructor(js_State *J, const char *clsTag, js_CFunction newFun) {
  js_getregistry(J, clsTag);                                 /*@lev=2*/
  js_newcconstructor(J, newFun, newFun, clsTag, 0/*nargs*/); /*@lev=2*/
  js_defproperty(J, -2, clsTag, JS_DONTENUM);                /*@lev=1*/
}

void JsSupport::addMethodCpp(js_State *J, const char *cls, const char *methodStr, const char *prototypeName, js_CFunction methodFun, unsigned nargs) {
  AssertStack(2);
  js_newcfunction(J, methodFun, prototypeName, nargs); /*PUSH a function object wrapping a C function pointer*/
  js_defproperty(J, -2, methodStr, JS_DONTENUM); /*POP a value from the top of the stack and set the value of the named property of the object (in prototype).*/ \
  AssertStack(2);
}

void JsSupport::addMethodJs(js_State *J, const char *cls, const char *methodStr, const char *codeStr) {
  js_dostring(J, str(boost::format("%1%.prototype['%2%'] = %3%") % cls % methodStr % codeStr).c_str());
}

void JsSupport::addJsFunction(js_State *J, const char *funcNameStr, js_CFunction funcFun, unsigned nargs) { // top-level standalone function
  js_newcfunction(J, funcFun, funcNameStr, nargs);
  js_setglobal(J, funcNameStr);
}

void JsSupport::addNsFunctionCpp(js_State *J, const char *nsNameStr, const char *jsfnNameStr, js_CFunction funcFun, unsigned nargs) {
  auto s = str(boost::format("%1%.%2%") % nsNameStr % jsfnNameStr);
  MuJS::jsB_propf(J, s.c_str(), funcFun, nargs);
}

void JsSupport::addNsFunctionJs(js_State *J, const char *nsNameStr, const char *fnNameStr, const char *codeStr) {
  auto sName = str(boost::format("[static function %1%.%2%]") % nsNameStr % fnNameStr);
  auto sCode = str(boost::format("(%1%)") % codeStr);
  ckErr(J, js_ploadstring(J, sName.c_str(), sCode.c_str()));
  js_call(J, -1);
  js_defproperty(J, -2, fnNameStr, JS_DONTENUM);
}

void JsSupport::addNsConstInt(js_State *J, const char *nsNameStr, const char *constNameStr, int constValue) {
  js_dostring(J, str(boost::format("%1%.%2% = %3%") % nsNameStr % constNameStr % constValue).c_str());
}

//
// require() - inspired by mujs: it loads the module, and exposes its exports.* symbols as the returned object's keys
//             borrowed from MuJS's main.c with modifications
//
void JsSupport::registerFuncRequire(js_State *J) {
  js_dostring(J,
    "function require(name) {\n"
    "  var cache = require.cache;\n"
    "  if (name in cache)\n"
    "    return cache[name];\n"
    "  var exports = {};\n"
    "  cache[name] = exports;\n"
    "  Function('exports', File.read(name.indexOf('/')==-1 ? 'modules/'+name+'.js' : name+'.js'))(exports);\n"
    "  return exports;\n"
    "}\n"
    "require.cache = Object.create(null);\n"
  );
}

//
// importCodeString() - imports a module from the string and allows the user to call its exported methods, access its exported data
//
void JsSupport::registerFuncImportCodeString(js_State *J) {
  js_dostring(J,
    "function importCodeString(codeString) {\n"
    "  var exports = {};\n"
    "  Function('exports', codeString)(exports);\n"
    "  return exports;\n"
    "}\n"
  );
}

//
// Error.prototype.toString - prints error trace on error
//                            borrowed from MuJS's main.c
//
void JsSupport::registerErrorToString(js_State *J) {
  js_dostring(J,
    "Error.prototype.toString = function() {\n"
    "  if (this.stackTrace)\n"
    "    return this.name + ': ' + this.message + this.stackTrace;\n"
    "  return this.name + ': ' + this.message;\n"
    "};\n"
  );
}

std::vector<int> JsSupport::objToInt32Array(js_State *J, int idx, const char *fname) {
  if (!js_isarray(J, idx))
    js_typeerror(J, "JsSupport::objToInt32Array: not an array in arg#%d of the function '%s'", idx, fname);

  auto len = js_getlength(J, idx);

  std::vector<int> v;
  v.reserve(len);

  for (unsigned i = 0; i < len; i++) {
    js_getindex(J, idx, i);
    v.push_back(js_tonumber(J, -1));
    js_pop(J, 1);
  }

  return v;
}

std::vector<int> JsSupport::objToInt32ArrayZ(js_State *J, int idx, const char *fname) {
  if (!js_isundefined(J, idx))
    return objToInt32Array(J, idx, fname);
  else
    return std::vector<int>();
}

char JsSupport::toChar(js_State *J, int idx) {
  const char *str = js_tostring(J, idx);
  if (str[0] == 0) {
    error(J, "expect char but got an empty string");
    return 0;
  } else if (str[1] != 0) {
    error(J, "expect char but got a string");
    return 0;
  } else
    return str[0];
}

void JsSupport::error(js_State *J, const std::string &msg) {
  js_newerror(J, msg.c_str());
  js_throw(J);
}

/// internals

void JsSupport::initObjectRegistry(js_State *J, const char *objTag) {
  js_newobject(J);
  js_setregistry(J, objTag);
}

void JsSupport::popPreviousStackElement(js_State *J) {
  js_rot2(J);
  js_pop(J, 1);
}

