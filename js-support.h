#pragma once

#include <mujs.h>

//
// JsSupport contains various support routines for binding various object types to JS
//
// It assumes that the presence of "const char* TAG_{cls}", "Js{cls}::xnew", etc in the module where it is included in
//

class JsSupport {
public:
  static void InitObjectRegistry(js_State *J, const char *objTag);
  static void beginNamespace(js_State *J, const char *nsNameStr);
  static void endNamespace(js_State *J, const char *nsNameStr);
  static void addJsConstructor(js_State *J, const char *clsTag, js_CFunction newFun);
  static void addMethodCpp(js_State *J, const char *cls, const char *methodStr, js_CFunction methodFun, unsigned nargs);
  static void addMethodJs(js_State *J, const char *cls, const char *methodStr, const char *codeStr);
  static void addJsFunction(js_State *J, const char *funcNameStr, js_CFunction funcFun, unsigned nargs);
  static void addNsFunctionCpp(js_State *J, const char *nsNameStr, const char *jsfnNameStr, js_CFunction funcFun, unsigned nargs);
  static void addNsFunctionJs(js_State *J, const char *nsNameStr, const char *fnNameStr, const char *codeStr);
}; // JsSupport

// convenience macros that come with JsSupport

#define BEGIN_NAMESPACE(ns) \
  JsSupport::beginNamespace(J, #ns);

#define END_NAMESPACE(ns) \
  JsSupport::endNamespace(J, #ns);

#define ADD_JS_CONSTRUCTOR(cls) /*@lev=1, a generic constructor, nargs=0 below doesn't seem to enforce number of args, or differentiate constructors by number of args*/ \
  JsSupport::addJsConstructor(J, TAG_##cls, Js##cls::xnew);

// add method defined by the C++ code
#define ADD_METHOD_CPP(cls, method, nargs) \
  JsSupport::addMethodCpp(J, #cls, #method, prototype::method, nargs);

// add method defined in JavaScript
#define ADD_METHOD_JS(cls, method, code...) \
  JsSupport::addMethodJs(J, #cls, #method, #code);

#define ADD_JS_FUNCTION(name, nargs) \
  JsSupport::addJsFunction(J, #name, JsBinding::name, nargs);

#define ADD_NS_FUNCTION_CPP(ns, jsfn, cfn, nargs) \
  JsSupport::addNsFunctionCpp(J, #ns, #jsfn, cfn, nargs);

#define ADD_NS_FUNCTION_JS(ns, func, code...) \
  JsSupport::addNsFunctionJs(J, #ns, #func, #code);

