
#include "js-support.h"

#include <mujs.h>

static const char *TAG_Image         = "Image";

class Image {
public:
  int i;
}; // Image

namespace JsImage {

static void imageFinalize(js_State *J, void *p) {
  delete (Image*)p;
}

static void xnewo(js_State *J, Image *i) {
  js_getglobal(J, TAG_Image);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Image, i, imageFinalize);
}

static void xnew(js_State *J) {
  AssertNargs(0)
  ReturnObj(Image, new Image);
}

namespace prototype {

}

void init(js_State *J) {
  JsSupport::InitObjectRegistry(J, TAG_Image);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Image)
  js_getglobal(J, TAG_Image);
  js_getproperty(J, -1, "prototype");
  JsSupport::StackPopPrevious(J);
  { // methods
    //ADD_METHOD_CPP(Obj, id, 0)
  }
  js_pop(J, 2);
}

} // JsImage
