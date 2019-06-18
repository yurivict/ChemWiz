
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

#include <GL/glew.h>
#include <GL/gl.h>

// Good WebGl tutorial: https://www.khronos.org/webgl/wiki/Tutorial

// WebGL IDLs: https://www.khronos.org/registry/webgl/specs/latest/

static const char *TAG_WebGlInterface    = "WebGlInterface";

namespace JsBinding {

namespace JsWebGlInterface {

// WebGL is "OpenGL ES 2", not plain OpenGL (the ES stands for 'for Embedded Systems'). (see https://stackoverflow.com/questions/8462421/differences-between-webgl-and-opengl/8462463)
// spec: https://www.khronos.org/registry/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf
// or in the XML form: https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/gl.xml

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
      // void
#define GL_FUNC_void_uint(funcNameWGL, funcNameGL) \
      GL_FUNC(funcNameWGL, 1, { \
        funcNameGL(GetArgFloat(1)); \
        ReturnVoid(J); \
      })
#define GL_FUNC_void_uint_uint(funcNameWGL, funcNameGL) \
      GL_FUNC(funcNameWGL, 2, { \
        funcNameGL(GetArgFloat(1), GetArgFloat(2)); \
        ReturnVoid(J); \
      })
#define GL_FUNC_void_float_float_float_float(funcNameWGL, funcNameGL) \
      GL_FUNC(funcNameWGL, 4, { \
        funcNameGL(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4)); \
        ReturnVoid(J); \
      })
#define GL_FUNC_void_int_float_float_float_float(funcNameWGL, funcNameGL) \
      GL_FUNC(funcNameWGL, 5, { \
        funcNameGL(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4), GetArgFloat(5)); \
        ReturnVoid(J); \
      })
      // nvoid
#define GL_FUNC_nvoid_noargs(funcNameWGL, funcNameGL) \
      GL_FUNC(funcNameWGL, 0, { \
        Return(J, funcNameGL()); \
      })

#include "webgl-auto-generated-macros.h"
/*
      // functions
      GL_FUNC(viewport, 4, {
        glViewport(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3), GetArgInt32(4));
      });
      GL_FUNC(bindBuffer, 2, {
        glBindBuffer(GetArgInt32(1), GetArgInt32(2));
      });
      //GL_FUNC(vertexAttribPointer, 6, {
      //  glVertexAttribPointer(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3), GetArgBoolean(4), GetArgInt32(5), GetArgInt32(6));
      //});
      GL_FUNC(clear, 1, {
        glClear(GetArgInt32(1));
      });
      //GL_FUNC(clearColor, 4, {
      //  //std::cout << "clearColor: " << GetArgFloat(1) << " " << GetArgFloat(2) << " " << GetArgFloat(3) << " " << GetArgFloat(4) << std::endl;
      //  glClearColor(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4));
      //  ReturnVoid(J);
      //});
      GL_FUNC_void_float_float_float_float(clearColor, glClearColor)
      GL_FUNC_nvoid_noargs(createProgram, glCreateProgram)
      GL_FUNC_void_uint(deleteShader, glDeleteShader)
      GL_FUNC_void_uint_uint(attachShader, glAttachShader)

      // 2.3    GL Command Syntax
      GL_FUNC_void_int_float_float_float_float(uniform4f, glUniform4f)

      // consts
      GL_CONST(COLOR_BUFFER_BIT)
      GL_CONST(DEPTH_BUFFER_BIT)
      GL_CONST(VERTEX_SHADER)
      GL_CONST(FRAGMENT_SHADER)
      GL_CONST(COMPILE_STATUS)
      GL_CONST(LINK_STATUS)
      GL_CONST(ARRAY_BUFFER)
      GL_CONST(STATIC_DRAW)
      GL_CONST(TRIANGLES)
*/
#undef GL_FUNC
#undef GL_CONST
    }
  });
  // no methods are defined for WebGlInterface
  JsSupport::endDefineClass(J);
}

} // JsWebGlInterface

} // JsBinding
