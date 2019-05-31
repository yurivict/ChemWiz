
#include "js-support.h"
#include "xerror.h"
#include "mytypes.h"

#include <string>
#include <sstream>
#include <memory>

#include <mujs.h>
#include <bitmap_image.hpp>
#include <picosha2.h>

static const char *TAG_Image         = "Image";
static const char *TAG_ImageDrawer   = "ImageDrawer";
extern const char *TAG_FloatArray; // allow to access the arguments of this type

namespace JsBinding {
namespace JsBinary {
  extern void xnewo(js_State *J, Binary *b);
}
}

typedef std::vector<Float> FloatArray;

//
// wrapper classes // TODO get rid of them by extending the framework to accept arbitrary classes
//

class Image : public bitmap_image {
public:
  Image(const std::string& filename) : bitmap_image(filename) { }
  Image(unsigned int width, unsigned int height) : bitmap_image(width, height) { }
}; // Image

class ImageDrawer : public image_drawer {
public:
  ImageDrawer(Image &image) : image_drawer(image) { }
}; // ImageDrawer

namespace Rgb {

union U {
  rgb_t    rgb;
  unsigned u;
};

static unsigned rgbToUnsigned(const rgb_t &rgb) {
  return ((U*)&rgb)->u;
}

static rgb_t unsignedToRgb(const unsigned &u) {
  return ((U*)&u)->rgb;
}

} // Rgb

namespace JsBinding {

namespace JsImage {

static void xnewo(js_State *J, Image *i) {
  js_getglobal(J, TAG_Image);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Image, i, [](js_State *J, void *p) {
    delete (Image*)p;
  });
}

static void xnew(js_State *J) {
  AssertNargsRange(1,2)
  switch (GetNArgs()) {
  case 1: // load bmp from file
    ReturnObj(Image, new Image(GetArgString(1)));
    break;
  case 2: // create a blank image with sizes
    ReturnObj(Image, new Image(GetArgUInt32(1), GetArgUInt32(2)));
    break;
  }
}

void init(js_State *J) {
  JsSupport::InitObjectRegistry(J, TAG_Image);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Image)
  js_getglobal(J, TAG_Image);
  js_getproperty(J, -1, "prototype");
  JsSupport::StackPopPrevious(J);
  { // methods
    ADD_METHOD_CPP(Image, width, {
      AssertNargs(0)
      Return(J, GetArg(Image, 0)->width());
    }, 0)
    ADD_METHOD_CPP(Image, height, {
      AssertNargs(0)
      Return(J, GetArg(Image, 0)->height());
    }, 0)
    ADD_METHOD_CPP(Image, getPixel, {
      AssertNargs(2)
      Return(J, Rgb::rgbToUnsigned(GetArg(Image, 0)->get_pixel(GetArgUInt32(1), GetArgUInt32(2))));
    }, 2)
    ADD_METHOD_CPP(Image, setPixel, {
      AssertNargs(3)
      GetArg(Image, 0)->set_pixel(GetArgUInt32(1), GetArgUInt32(2), Rgb::unsignedToRgb(GetArgUInt32(3)));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(Image, setPixelsFromArray, {
      AssertNargs(5)
      auto img = GetArg(Image, 0);
      auto arr = GetArg(FloatArray, 1);
      auto period = GetArgUInt32(2);
      auto idxx = GetArgUInt32(3);
      auto idxy = GetArgUInt32(4);
      auto clr = Rgb::unsignedToRgb(GetArgUInt32(5)); // color as UINT
      if (arr->size() % period != 0)
        ERROR("Image::setPixelsFromArray: image size="<< arr->size() << " isn't a multiple of a period=" << period)
      for (auto it = arr->begin(), ite = arr->end(); it != ite; it += period) {
        //std::cout << "setPixel: x=" << *(it+idxx) << " y=" << *(it+idxy) << std::endl;
        img->set_pixel((unsigned)*(it+idxx), (unsigned)*(it+idxy), clr);
      }
      ReturnVoid(J);
    }, 5)
    ADD_METHOD_CPP(Image, plasma, {
      AssertNargs(10)
      auto strToColormap = [](const std::string &name) {
        // TODO other colormaps
        if (name == "gray_colormap")
          return gray_colormap;
        if (name == "hot_colormap")
          return hot_colormap;
        if (name == "hsv_colormap")
          return hsv_colormap;
        if (name == "jet_colormap")
          return jet_colormap;
        ERROR("Unknown image colormap supplied: " << name)
      };
      plasma(*GetArg(Image, 0),
             GetArgFloat(1), GetArgFloat(2),
             GetArgFloat(3), GetArgFloat(4),
             GetArgFloat(5), GetArgFloat(6),
             GetArgFloat(7), GetArgFloat(8),
             GetArgFloat(9),
             strToColormap(GetArgString(10)));
      ReturnVoid(J);
    }, 10)
    ADD_METHOD_CPP(Image, checkeredPattern, {
      AssertNargs(4)
      checkered_pattern(GetArgUInt32(1), GetArgUInt32(2), GetArgUInt32(3), (bitmap_image::color_plane)GetArgUInt32(4), *GetArg(Image, 0));
      ReturnVoid(J);
    }, 4)
    ADD_METHOD_CPP(Image, equal, {
      AssertNargs(1)
      *GetArg(Image, 0) = *GetArg(Image, 1);
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Image, clear, {
      AssertNargs(0)
      GetArg(Image, 0)->clear();
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(Image, cryptoHash, {
      AssertNargs(0)
      // get image data buffer
      auto *img  = GetArg(Image, 0);
      auto imgData = (uint8_t*)img->data();
      size_t imgDataSize = img->bytes_per_pixel()*img->width()*img->height();
      // compute hash
      auto hash_hex_str = picosha2::hash256_hex_string(imgData, imgData+imgDataSize);
      //
      Return(J, hash_hex_str);
    }, 0)
    ADD_METHOD_CPP(Image, saveImage, {
      AssertNargs(1)
      GetArg(Image, 0)->save_image(GetArgString(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Image, toBinary, {
      AssertNargs(0)
      // get image data buffer
      std::ostringstream ss; // FIXME this is inefficient to copy it to the string first TODO need to use std::basic_ostream with a custom std::basic_streambuf
      ss << std::noskipws;
      GetArg(Image, 0)->write_image(ss);
      ss.flush();
      // create binary
      const std::string &strBuf = ss.str();
      auto p = (uint8_t*)strBuf.c_str();
      assert(ss.str()[0] == 'B');
      std::unique_ptr<Binary> binary(new Binary(p, p + strBuf.size()));
      //
      ReturnObj(Binary, binary.release());
    }, 0)
    // drawing functions
    ADD_METHOD_CPP(Image, setRegion, {
      AssertNargs(7)
      GetArg(Image, 0)->set_region(
        GetArgUInt32(1), GetArgUInt32(2), // x y
        GetArgUInt32(3), GetArgUInt32(4), // width height
        GetArgUInt32(5), GetArgUInt32(6), GetArgUInt32(7)); // red green blue
      ReturnVoid(J);
    }, 7)
    ADD_METHOD_CPP(Image, reflectiveImage, {
      AssertNargs(2)
      GetArg(Image, 0)->reflective_image(*GetArg(Image, 1), GetArgBoolean(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(Image, convertToGrayscale, {
      AssertNargs(0)
      GetArg(Image, 0)->convert_to_grayscale();
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(Image, horizontalFlip, {
      AssertNargs(0)
      GetArg(Image, 0)->horizontal_flip();
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(Image, verticalFlip, {
      AssertNargs(0)
      GetArg(Image, 0)->vertical_flip();
      ReturnVoid(J);
    }, 0)
  }
  js_pop(J, 2);
}

} // JsImage

namespace JsImageDrawer {

static void xnewo(js_State *J, ImageDrawer *d) {
  js_getglobal(J, TAG_ImageDrawer);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_ImageDrawer, d, [](js_State *J, void *p) {
    delete (ImageDrawer*)p;
  });
}

static void xnew(js_State *J) {
  AssertNargs(1)
  ReturnObj(ImageDrawer, new ImageDrawer(*GetArg(Image, 1)));
}

void init(js_State *J) {
  JsSupport::InitObjectRegistry(J, TAG_ImageDrawer);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(ImageDrawer)
  js_getglobal(J, TAG_ImageDrawer);
  js_getproperty(J, -1, "prototype");
  JsSupport::StackPopPrevious(J);
  { // methods
    ADD_METHOD_CPP(ImageDrawer, penWidth, {
      AssertNargs(1)
      GetArg(ImageDrawer, 0)->pen_width(GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(ImageDrawer, penColor, {
      AssertNargs(3)
      GetArg(ImageDrawer, 0)->pen_color(GetArgUInt32(1), GetArgUInt32(2), GetArgUInt32(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(ImageDrawer, rectangle, {
      AssertNargs(4)
      GetArg(ImageDrawer, 0)->rectangle(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4));
      ReturnVoid(J);
    }, 4)
    ADD_METHOD_CPP(ImageDrawer, triangle, {
      AssertNargs(8)
      GetArg(ImageDrawer, 0)->triangle(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4), GetArgFloat(5), GetArgFloat(6));
      ReturnVoid(J);
    }, 6)
    ADD_METHOD_CPP(ImageDrawer, quadix, {
      AssertNargs(8)
      GetArg(ImageDrawer, 0)->quadix(
        GetArgFloat(1), GetArgFloat(2),
        GetArgFloat(3), GetArgFloat(4),
        GetArgFloat(5), GetArgFloat(6),
        GetArgFloat(7), GetArgFloat(8));
      ReturnVoid(J);
    }, 8)
    ADD_METHOD_CPP(ImageDrawer, lineSegment, {
      AssertNargs(4)
      GetArg(ImageDrawer, 0)->line_segment(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4));
      ReturnVoid(J);
    }, 4)
    ADD_METHOD_CPP(ImageDrawer, horiztonalLineSegment, {
      AssertNargs(3)
      GetArg(ImageDrawer, 0)->horiztonal_line_segment(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(ImageDrawer, verticalLineSegment, {
      AssertNargs(3)
      GetArg(ImageDrawer, 0)->vertical_line_segment(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3));
      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(ImageDrawer, ellipse, {
      AssertNargs(4)
      GetArg(ImageDrawer, 0)->ellipse(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3), GetArgFloat(4));
      ReturnVoid(J);
    }, 4)
    ADD_METHOD_CPP(ImageDrawer, circle, {
      AssertNargs(3)
      GetArg(ImageDrawer, 0)->circle(GetArgFloat(1), GetArgFloat(2), GetArgFloat(3));
      ReturnVoid(J);
    }, 3)
  }
  js_pop(J, 2);
}

} // JsImageDrawer

} // JsBinding
