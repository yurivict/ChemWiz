
// Java Script equivalent of the "Frosted Glass Effect" example (http://www.partow.net/programming/bitmap/index.html#simpleexample07)

function exampleFrostedGlassEffect() {

  // RNG: use lagecy RNG like in the original example
  var seed = 0xA5AA57A0
  LegacyRng.srand(seed)

  var width       = 600
  var height      = 600
  var kernel_size = 10

  var base = new Image(width, height)
  base.clear()

  if (true) {
    var c1 = 0.8
    var c2 = 0.4
    var c3 = 0.2
    var c4 = 0.6

    base.plasma(0, 0, base.width(), base.height(), c1, c2, c3, c4, 3.0, "jet_colormap");

    base.checkeredPattern(30, 30, 230, 2) // bitmap_image::  red_plane
    base.checkeredPattern(30, 30,   0, 1) // bitmap_image::green_plane
    base.checkeredPattern(30, 30, 100, 0) // bitmap_image:: blue_plane
  }

  var glass_image = new Image(base.width(), base.height())
  glass_image.equal(base)

  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      var min_x = Math.max(0, x - kernel_size)
      var min_y = Math.max(0, y - kernel_size)
      var max_x = Math.min(x + kernel_size, width  - 1)
      var max_y = Math.min(y + kernel_size, height - 1)
      var dx    = max_x - min_x
      var dy    = max_y - min_y
      var N     = LegacyRng.rand() % (dx * dy)
      var cx    = (N % dx) + min_x
      var cy    = (N / dx) + min_y

      glass_image.setPixel(x, y, base.getPixel(cx, cy));
    }
  }

  return glass_image
}


exports.run = function() {
  var glass_image = exampleFrostedGlassEffect()
  //glass_image.saveImage("glass_effect.bmp");
  //print("exampleFrostedGlassEffect().hash="+glass_image.cryptoHash())

  if (glass_image.cryptoHash() == "B4CE3FAF139F97D2ECBAD951FDBA5C84")
    return "OK"
  else
    return "FAIL"
}
