
#include <mujs.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "js-support.h"
#include "xerror.h"

static const char *TAG_GlfwWindow    = "GlfwWindow";

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

class GlfwWindow {
  GLFWwindow* window;
public:
  GlfwWindow() {
    window = glfwCreateWindow(640, 480, "Simple Window", NULL, NULL);
    if (!window) {
      glfwTerminate();
      ERROR("GLFW: Window or OpenGL context creation failed")
    }

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      std::cout << "GLFW: key pressed: key=" << key << std::endl;
      if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    });

    display();
  }
  ~GlfwWindow() {
    if (!window)
      glfwDestroyWindow(window);
  }
private:
  void display() {
    glfwMakeContextCurrent(window);
    while (!glfwWindowShouldClose(window)) {
      float ratio;
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      ratio = width / (float) height;
      glViewport(0, 0, width, height);
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
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
    glfwDestroyWindow(window);
    window = nullptr;
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
  // no methods are defined for GlfwWindow yet
  JsSupport::endDefineClass(J);
}

} // JsGlfwWindow

} // JsBinding
