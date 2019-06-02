#include "js-support.h"

#include <iostream>

#include <boost/format.hpp>

#include <mujs.h>

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

void JsSupport::addMethodCpp(js_State *J, const char *cls, const char *methodStr, js_CFunction methodFun, unsigned nargs) {
  auto s = str(boost::format("%1%.prototype.%2%") % cls % methodStr);
  AssertStack(2);
  js_newcfunction(J, methodFun, s.c_str(), nargs); /*PUSH a function object wrapping a C function pointer*/
  js_defproperty(J, -2, methodStr, JS_DONTENUM); /*POP a value from the top of the stack and set the value of the named property of the object (in prototype).*/ \
  AssertStack(2);
}

void JsSupport::addMethodJs(js_State *J, const char *cls, const char *methodStr, const char *codeStr) {
  js_dostring(J, str(boost::format("%1%.prototype['%2%'] = %3%") % cls % methodStr % codeStr).c_str());
}

void JsSupport::addJsFunction(js_State *J, const char *funcNameStr, js_CFunction funcFun, unsigned nargs) {
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

//
// require() - inspired by mujs: it loads the module, and exposes its exports.* symbols as the returned object's keys
//             borrowed from MuJS's main.c with modifications
//
void JsSupport::registerFuncRequire(js_State *J) {
  js_dostring(J,
    "function require(name) {\n"
    "  var cache = require.cache\n"
    "  if (name in cache) return cache[name]\n"
    "  var exports = {}\n"
    "  cache[name] = exports\n"
    "  Function('exports', File.read(name.indexOf('/')==-1 ? 'modules/'+name+'.js' : name+'.js'))(exports)\n"
    "  return exports\n"
    "}\n"
    "require.cache = Object.create(null)\n"
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

/// internals

void JsSupport::initObjectRegistry(js_State *J, const char *objTag) {
  js_newobject(J);
  js_setregistry(J, objTag);
}

void JsSupport::popPreviousStackElement(js_State *J) {
  js_rot2(J);
  js_pop(J, 1);
}

