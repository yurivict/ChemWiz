
#include "js-support.h"
#include "misc.h"
#include "mytypes.h"

#include <memory>


const char *TAG_LAVectorF      = "LAVectorF"; // non-static: used externally
const char *TAG_LAMatrixF      = "LAMatrixF"; // non-static: used externally
const char *TAG_LAVectorD      = "LAVectorD"; // non-static: used externally
const char *TAG_LAMatrixD      = "LAMatrixD"; // non-static: used externally


namespace JsBinding {

namespace JsLinearAlgebra {

void xnewo(js_State *J, LAVectorF *b) {
  js_getglobal(J, TAG_LAVectorF);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_LAVectorF, b, [](js_State *J, void *p) {
    delete (LAVectorF*)p;
  });
}

void xnewo(js_State *J, LAMatrixF *b) {
  js_getglobal(J, TAG_LAMatrixF);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_LAMatrixF, b, [](js_State *J, void *p) {
    delete (LAMatrixF*)p;
  });
}

void xnewo(js_State *J, LAVectorD *b) {
  js_getglobal(J, TAG_LAVectorD);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_LAVectorD, b, [](js_State *J, void *p) {
    delete (LAVectorD*)p;
  });
}

void xnewo(js_State *J, LAMatrixD *b) {
  js_getglobal(J, TAG_LAMatrixD);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_LAMatrixD, b, [](js_State *J, void *p) {
    delete (LAMatrixD*)p;
  });
}

void init(js_State *J) {
#define VECTOR_METHODS(cls) \
  ADD_METHOD_CPP(cls, str, { \
    AssertNargs(0) \
    Return(J, STR(*GetArg(cls, 0))); \
  }, 0) \
  ADD_METHOD_CPP(cls, size, { \
    AssertNargs(0) \
    Return(J, GetArg(cls, 0)->size()); \
  }, 0) \
  ADD_METHOD_CPP(cls, get, { \
    AssertNargs(1) \
    Return(J, (*GetArg(cls, 0))(GetArgUInt32(1))); \
  }, 0) \
  ADD_METHOD_CPP(cls, set, { \
    AssertNargs(2) \
    (*GetArg(cls, 0))(GetArgUInt32(1)) = GetArgFloat(2); \
    ReturnVoid(J); \
  }, 0) \
  ADD_METHOD_CPP(cls, resize, { \
    AssertNargs(1) \
    GetArg(cls, 0)->resize(GetArgUInt32(1)); \
    ReturnVoid(J); \
  }, 0)
#define MATRIX_METHODS(cls) \
  ADD_METHOD_CPP(cls, str, { \
    AssertNargs(0) \
    Return(J, STR(*GetArg(cls, 0))); \
  }, 0) \
  ADD_METHOD_CPP(cls, rows, { \
    AssertNargs(0) \
    Return(J, GetArg(cls, 0)->rows()); \
  }, 0) \
  ADD_METHOD_CPP(cls, get, { \
    AssertNargs(2) \
    Return(J, (*GetArg(cls, 0))(GetArgUInt32(1),GetArgUInt32(2))); \
  }, 0) \
  ADD_METHOD_CPP(cls, set, { \
    AssertNargs(3) \
    (*GetArg(cls, 0))(GetArgUInt32(1),GetArgUInt32(2)) = GetArgFloat(3); \
    ReturnVoid(J); \
  }, 0) \
  ADD_METHOD_CPP(cls, cols, { \
    AssertNargs(0) \
    Return(J, GetArg(cls, 0)->cols()); \
  }, 0) \
  ADD_METHOD_CPP(cls, resize, { \
    AssertNargs(2) \
    GetArg(cls, 0)->resize(GetArgUInt32(1), GetArgUInt32(2)); \
    ReturnVoid(J); \
  }, 0) \
  ADD_METHOD_CPP(cls, add, { \
    AssertNargs(1) \
    std::unique_ptr<cls> res(new cls); \
    *res = (*GetArg(cls, 0)) + (*GetArg(cls, 1)); \
    GetArg(cls, 0)->resize(GetArgUInt32(1), GetArgUInt32(2)); \
    ReturnObj(res.release()); \
  }, 0) \
  ADD_METHOD_CPP(cls, mulm, { \
    AssertNargs(1) \
    std::unique_ptr<cls> res(new cls); \
    *res = (*GetArg(cls, 0)) * (*GetArg(cls, 1)); \
    GetArg(cls, 0)->resize(GetArgUInt32(1), GetArgUInt32(2)); \
    ReturnObj(res.release()); \
  }, 0)
  // LAVectorF
  JsSupport::beginDefineClass(J, TAG_LAVectorF, [](js_State *J) {
    AssertNargsRange(0,3)
    switch (GetNArgs()) {
    case 0:
      ReturnObj(new LAVectorF);
      break;
    case 1:
      ReturnObj(new LAVectorF(GetArgUInt32(1)));
      break;
    case 2:
      switch (GetArgChar(2)) {
      case 'Z': { // filled with zeros
        std::unique_ptr<LAVectorF> res(new LAVectorF);
        *res = LAVectorF::Zero(GetArgUInt32(1));
        ReturnObj(res.release());
        break;
      } case 'R': { // filled with random values
        std::unique_ptr<LAVectorF> res(new LAVectorF);
        *res = LAVectorF::Random(GetArgUInt32(1));
        ReturnObj(res.release());
        break;
      } default:
        JS_ERROR("wrong argument #2 supplied to LAVectorF constructor")
      }
      break;
    default:
      JS_ERROR("wrong argument number of arguments supplied to LAVectorF constructor: " << GetNArgs())
    }
  });
  VECTOR_METHODS(LAVectorF)
  JsSupport::endDefineClass(J);
  // LAMatrixF
  JsSupport::beginDefineClass(J, TAG_LAMatrixF, [](js_State *J) {
    AssertNargsRange(0,3)
    switch (GetNArgs()) {
    case 0:
      ReturnObj(new LAMatrixF);
      break;
    case 2:
      ReturnObj(new LAMatrixF(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/));
      break;
    case 3:
      switch (GetArgChar(3)) {
      case 'Z': { // filled with zeros
        std::unique_ptr<LAMatrixF> res(new LAMatrixF);
        *res = LAMatrixF::Zero(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/);
        ReturnObj(res.release());
        break;
      } case 'R': { // filled with random values
        std::unique_ptr<LAMatrixF> res(new LAMatrixF);
        *res = LAMatrixF::Random(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/);
        ReturnObj(res.release());
        break;
      } default:
        JS_ERROR("wrong argument #3 supplied to LAMatrixF constructor")
      }
      break;
    default:
      JS_ERROR("wrong argument number of arguments supplied to LAMatrixF constructor: " << GetNArgs())
    }
  });
  MATRIX_METHODS(LAMatrixF)
  JsSupport::endDefineClass(J);
  // LAVectorD
  JsSupport::beginDefineClass(J, TAG_LAVectorD, [](js_State *J) {
    AssertNargsRange(0,3)
    switch (GetNArgs()) {
    case 0:
      ReturnObj(new LAVectorD);
      break;
    case 1:
      ReturnObj(new LAVectorD(GetArgUInt32(1)));
      break;
    case 2:
      switch (GetArgChar(2)) {
      case 'Z': { // filled with zeros
        std::unique_ptr<LAVectorD> res(new LAVectorD);
        *res = LAVectorD::Zero(GetArgUInt32(1));
        ReturnObj(res.release());
        break;
      } case 'R': { // filled with random values
        std::unique_ptr<LAVectorD> res(new LAVectorD);
        *res = LAVectorD::Random(GetArgUInt32(1));
        ReturnObj(res.release());
        break;
      } default:
        JS_ERROR("wrong argument #2 supplied to LAVectorD constructor")
      }
      break;
    default:
      JS_ERROR("wrong argument number of arguments supplied to LAVectorD constructor: " << GetNArgs())
    }
  });
  VECTOR_METHODS(LAVectorD)
  JsSupport::endDefineClass(J);
  // LAMatrixD
  JsSupport::beginDefineClass(J, TAG_LAMatrixD, [](js_State *J) {
    AssertNargsRange(0,3)
    switch (GetNArgs()) {
    case 0:
      ReturnObj(new LAMatrixD);
      break;
    case 2:
      ReturnObj(new LAMatrixD(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/));
      break;
    case 3:
      switch (GetArgChar(3)) {
      case 'Z': { // filled with zeros
        std::unique_ptr<LAMatrixD> res(new LAMatrixD);
        *res = LAMatrixD::Zero(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/);
        ReturnObj(res.release());
        break;
      } case 'R': { // filled with random values
        std::unique_ptr<LAMatrixD> res(new LAMatrixD);
        *res = LAMatrixD::Random(GetArgUInt32(1)/*rows*/, GetArgUInt32(2)/*cols*/);
        ReturnObj(res.release());
        break;
      } default:
        JS_ERROR("wrong argument #3 supplied to LAMatrixD constructor")
      }
      break;
    default:
      JS_ERROR("wrong argument number of arguments supplied to LAMatrixD constructor: " << GetNArgs())
    }
  });
  MATRIX_METHODS(LAMatrixD)
  JsSupport::endDefineClass(J);
}

} // JsNeuralNetwork

} // JsBinding
