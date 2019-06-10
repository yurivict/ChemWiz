
//
// webgl module to support being able to display WebGL-based graphics
// Rationale: There are a lot of available stable and mature JavaScript libraries using WebGL.
//            It would be nice to use them to:
//            1. display molecules
//            2. display various data graphs, illustrations, etc

#include <mujs.h>

#include "js-support.h"

//#include <iostream>
#include <string>

#include <GL/gl.h>

static const char *TAG_WebGlInterface    = "WebGlInterface";

namespace JsBinding {

namespace JsWebGlInterface {

void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_WebGlInterface, [](js_State *J) {
    AssertNargs(0)
    // WebGlInterface is a simple JS object with pre-defined set of methods
    js_newobject(J);
    {
#define GL_CONST(gl_const) \
      js_pushnumber(J, GL_##gl_const); \
      js_setproperty(J, -2, #gl_const);

#define GL_FUNC(funcNameWGL, nargs, code) \
      js_newcfunction(J, [](js_State *J) code, #funcNameWGL, nargs); \
      js_setproperty(J, -2, #funcNameWGL);

      // functions
      GL_FUNC(viewport, 4, {
        glViewport(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3), GetArgInt32(4));
      });
      GL_FUNC(clear, 1, {
        //std::cout << "clear: " << GetArgFloat(1) << std::endl;
        glClear(GetArgInt32(1));
      });
      GL_FUNC(clearColor, 4, {
        //std::cout << "clearColor: " << GetArgFloat(1) << " " << GetArgFloat(2) << " " << GetArgFloat(3) << " " << GetArgFloat(4) << std::endl;
        glClearColor(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4));
      });

      // consts
      GL_CONST(COLOR_BUFFER_BIT)
      GL_CONST(DEPTH_BUFFER_BIT)
      GL_CONST(TRIANGLES)

#undef GL_FUNC
#undef GL_CONST
    }
  });
  // no methods are defined for WebGlInterface
  JsSupport::endDefineClass(J);
}

} // JsWebGlInterface

} // JsBinding
