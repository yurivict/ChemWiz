
#include <mujs.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>

#include "js-support.h"
#include "xerror.h"

// see the GLFW sample app: https://github.com/skaslev/gl3w/blob/master/src/glfw_test.c

const char *TAG_GlfwWindow    = "GlfwWindow"; // used from outside

class GlfwInit {
public:
  GlfwInit() {
    // init glfw
    if (!glfwInit())
      ERROR("GLFW Initialization failed")
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  }
}; // GlfwInit

static GlfwInit __glfwInit__;

//
// GlRunner - runs some glXx commands when called
//
class GlRunner {
public:
  virtual ~GlRunner() { }
  virtual void run(GLFWwindow* window) = 0;
}; // GlRunner

class MyTestGlCode : public GlRunner {
public:
  ~MyTestGlCode() { }
  void run(GLFWwindow* window) {
    float ratio;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    //
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLES);
    glColor3f(1.f, 0.f, 0.f);
    glVertex3f(-0.6f, -0.4f, 0.f);
    glColor3f(0.f, 1.f, 0.f);
    glVertex3f(0.6f, -0.4f, 0.f);
    glColor3f(0.f, 0.f, 1.f);
    glVertex3f(0.f, 0.6f, 0.f);
    glEnd();
    //
    glfwSwapBuffers(window);
  }
}; // MyTestGlCode

//
// JsFunctionGlRunner calls arguments that are on stack: (function glFunc(gl), glIface)
//
class JsFunctionGlRunner : public GlRunner {
  js_State *J;
public:
  JsFunctionGlRunner(js_State *newJ) : J(newJ) { }
  ~JsFunctionGlRunner() { }
  void run(GLFWwindow* window) {
    // XXX repeat
    float ratio;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    // call the JS function
    js_copy(J, -2);      // push function
    js_pushundefined(J); // this
    js_copy(J, -3);      // push the argument
    js_call(J, 1); // call glFunc(glIface)
    js_pop(J, 1);  // remove the result which is supposed to be 'undefined'
    // XXX repeat
    glfwSwapBuffers(window);
  }
}; // JsFunctionGlRunner

class GlfwWindow {
  GLFWwindow *window;
  std::unique_ptr<GlRunner>   glRunner;
public:
  GlfwWindow() : glRunner(new MyTestGlCode) {
    window = glfwCreateWindow(640, 480, "Simple Window", NULL, NULL);
    if (!window) {
      glfwTerminate();
      ERROR("GLFW: Window or OpenGL context creation failed")
    }

    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      std::cout << "GLFW: key pressed: key=" << key << std::endl;
      if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    });
    glfwSetWindowRefreshCallback(window, [](GLFWwindow* window) {
      ((GlfwWindow*)glfwGetWindowUserPointer(window))->display(window);
    });
  }
  ~GlfwWindow() {
    if (!window)
      glfwDestroyWindow(window);
  }
  void setGlRunner(GlRunner *newGlRunner) {glRunner.reset(newGlRunner);}
  GLFWwindow* getWindow() {return window;}
  void display(GLFWwindow* window) {
    glRunner->run(window);
  }
  static void keyPressed(unsigned char key, int x, int y) {  
    if (key == 'y') {
      //moveY += 0.5;
      //cout << "y" << endl ;
      std::cout << "GlfwWindow::keyPressed: key=" << key << " x=" << x << " y=" << y << std::endl;
    }
  }
}; // GlfwWindow

namespace JsBinding {

namespace JsGlfwWindow {

static void xnewo(js_State *J, GlfwWindow *gw) {
  js_getglobal(J, TAG_GlfwWindow);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_GlfwWindow, gw, [](js_State *J, void *p) {
    delete (GlfwWindow*)p;
  });
}

void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_GlfwWindow, [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new GlfwWindow);
  });
  ADD_METHOD_CPP(GlfwWindow, loop, { // arguments: (function glFunc(gl), glIface) to pass to glFunc
    AssertNargs(2)
    printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    auto This = GetArg(GlfwWindow, 0);
    auto window = This->getWindow();
    This->setGlRunner(new JsFunctionGlRunner(J));
    // >>> hard loop
    glfwMakeContextCurrent(window);
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      This->display(window);
    }
    glfwDestroyWindow(window); // <<< hard loop
  }, 2);
  JsSupport::endDefineClass(J);
}

} // JsGlfwWindow

} // JsBinding
