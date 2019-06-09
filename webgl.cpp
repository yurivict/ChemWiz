
//
// webgl module to support being able to display WebGL-based graphics
// Rationale: There are a lot of available stable and mature JavaScript libraries using WebGL.
//            It would be nice to use them to:
//            1. display molecules
//            2. display various data graphs, illustrations, etc

#include <mujs.h>

#include "js-support.h"

static const char *TAG_WebGlInterface    = "WebGlInterface";

namespace JsBinding {

namespace JsWebGlInterface {

void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_WebGlInterface, [](js_State *J) {
    AssertNargs(0)
    // WebGlInterface is a simple JS object with pre-defined set of methods
    js_newobject(J);
    {
      js_pushnumber(J, 10);
      js_setproperty(J, -2, "fld1");
      js_pushnumber(J, 101);
      js_setproperty(J, -2, "fld2");
    }
  });
  // no methods are defined for WebGlInterface
  JsSupport::endDefineClass(J);
}

} // JsWebGlInterface

} // JsBinding
