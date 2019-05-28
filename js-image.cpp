
#include "js-support.h"
#include "xerror.h"

#include <string>
#include <sstream>

#include <mujs.h>
#include <bitmap_image.hpp>
#include <cryptopp/cryptlib.h>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1 // workaround for crash with SHA256, see at Algo
//#include <cryptopp/sha.h>
#include <cryptopp/md5.h> // workaround for crash with SHA256, see at Algo
#include <cryptopp/hex.h>

static const char *TAG_Image         = "Image";

class Image : public bitmap_image {
public:
  Image(const std::string& filename) : bitmap_image(filename) { }
  Image(unsigned int width, unsigned int height) : bitmap_image(width, height) { }
}; // Image

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

namespace prototype {

static void width(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Image, 0)->width());
}

static void height(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Image, 0)->height());
}

static void getPixel(js_State *J) {
  AssertNargs(2)
  Return(J, Rgb::rgbToUnsigned(GetArg(Image, 0)->get_pixel(GetArgUInt32(1), GetArgUInt32(2))));
}

static void setPixel(js_State *J) {
  AssertNargs(3)
  GetArg(Image, 0)->set_pixel(GetArgUInt32(1), GetArgUInt32(2), Rgb::unsignedToRgb(GetArgUInt32(3)));
  ReturnVoid(J);
}

static void plasma(js_State *J) {
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
}

static void checkeredPattern(js_State *J) {
  AssertNargs(4)
  checkered_pattern(GetArgUInt32(1), GetArgUInt32(2), GetArgUInt32(3), (bitmap_image::color_plane)GetArgUInt32(4), *GetArg(Image, 0));
  ReturnVoid(J);
}

static void equal(js_State *J) {
  AssertNargs(1)
  *GetArg(Image, 0) = *GetArg(Image, 1);
  ReturnVoid(J);
}

static void clear(js_State *J) {
  AssertNargs(0)
  GetArg(Image, 0)->clear();
  ReturnVoid(J);
}

static void cryptoHash(js_State *J) {
  AssertNargs(0)
  // cryptopp
  using namespace CryptoPP;
  //typedef SHA256 Algo; // SHA256 crashes, need to investigate why
  typedef Weak::MD5 Algo; // workaround for crash with SHA256
  // get image data buffer
  auto *img  = GetArg(Image, 0);
  auto imgData = (byte*)img->data();
  size_t imgDataSize = img->bytes_per_pixel()*img->width()*img->height();
  // compute hash
  byte digest[Algo::DIGESTSIZE];
  Algo().CalculateDigest(digest, (byte*)imgData, imgDataSize);
  // convert hash to string
  std::string hash;
  HexEncoder encoder;
  encoder.Attach(new StringSink(hash));
  encoder.Put(digest, sizeof(digest));
  encoder.MessageEnd();
  //
  Return(J, hash);
}

static void saveImage(js_State *J) {
  AssertNargs(1)
  GetArg(Image, 0)->save_image(GetArgString(1));
  ReturnVoid(J);
}

static void setRegion(js_State *J) {
  AssertNargs(7)
  GetArg(Image, 0)->set_region(
    GetArgUInt32(1), GetArgUInt32(2), // x y
    GetArgUInt32(3), GetArgUInt32(4), // width height
    GetArgUInt32(5), GetArgUInt32(6), GetArgUInt32(7)); // red green blue
  ReturnVoid(J);
}

static void reflectiveImage(js_State *J) {
  AssertNargs(2)
  GetArg(Image, 0)->reflective_image(*GetArg(Image, 1), GetArgBoolean(2));
  ReturnVoid(J);
}

static void convertToGrayscale(js_State *J) {
  AssertNargs(0)
  GetArg(Image, 0)->convert_to_grayscale();
  ReturnVoid(J);
}

static void horizontalFlip(js_State *J) {
  AssertNargs(0)
  GetArg(Image, 0)->horizontal_flip();
  ReturnVoid(J);
}

static void verticalFlip(js_State *J) {
  AssertNargs(0)
  GetArg(Image, 0)->vertical_flip();
  ReturnVoid(J);
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
    ADD_METHOD_CPP(Image, width, 0)
    ADD_METHOD_CPP(Image, height, 0)
    ADD_METHOD_CPP(Image, getPixel, 2)
    ADD_METHOD_CPP(Image, setPixel, 3)
    ADD_METHOD_CPP(Image, plasma, 10)
    ADD_METHOD_CPP(Image, checkeredPattern, 4)
    ADD_METHOD_CPP(Image, equal, 1)
    ADD_METHOD_CPP(Image, clear, 0)
    ADD_METHOD_CPP(Image, cryptoHash, 0)
    ADD_METHOD_CPP(Image, saveImage, 1)
    // drawing functions
    ADD_METHOD_CPP(Image, setRegion, 7)
    ADD_METHOD_CPP(Image, reflectiveImage, 2)
    ADD_METHOD_CPP(Image, convertToGrayscale, 0)
    ADD_METHOD_CPP(Image, horizontalFlip, 0)
    ADD_METHOD_CPP(Image, verticalFlip, 0)
  }
  js_pop(J, 2);
}

} // JsImage
