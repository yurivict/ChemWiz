// module Animate: miscellaneous functions that facilitate moving things in a particular fashion

//
// In QA cases we took an approach to create an image showing the animation visually, and check its crypto-hash.
// Please always create an image like this, and always see the image visually if QA cases fail.
//

//
// 1-dimension
//

//
// dim1LinearMotion
//
function dim1LinearMotion(fromVal, toVal, numSamples, fn) {
  var dVal = (toVal - fromVal)/numSamples
  for (var sample = 0, val = fromVal; sample < numSamples; sample++, val += dVal) {
    fn(val)
  }
  // not including the last point
}

//
// dim1SineWave
//
function dim1SineWave(fromVal, toVal, numSamples, numCycles, fn) { // ASSUMES fromVal < toVal
  var valMid = (fromVal + toVal)/2
  var valAmpl = (toVal - fromVal)/2

  var dphase = 2*Math.PI/numSamples

  for (var cycle = 0; cycle < numCycles; cycle++)
    for (var phase = 0; phase < 2*Math.PI; phase += dphase)
      fn(Math.sin(phase)*valAmpl + valMid)
}

//
// 2-dimensions
//

//
// dim2LinearMotion
//
function dim2LinearMotion(fromVal, toVal, numSamples, fn) {
  var dVal = [(fromVal[0] + toVal[0])/2, (fromVal[1] + toVal[1])/2]
  for (var sample = 0, val = fromVal; sample < numSamples; sample++, val[0] += dVal[0], val[1] += dVal[1]) {
    fn(val)
  }
  // not including the last point
}

//
// dim2WaveSquare: draws a square
//
function dim2WaveSquare(minx, miny, maxx, maxy, numSamplesPerCycle, numCycles, fn) {
  var numSamplesT = numSamplesPerCycle/4
  var numSamplesR = numSamplesPerCycle/4
  var numSamplesB = numSamplesPerCycle/4
  var numSamplesL = numSamplesPerCycle - numSamplesT - numSamplesR - numSamplesB
  for (var cycle = 0; cycle < numCycles; cycle++) {
    // top
    dim1LinearMotion(minx, maxx, numSamplesT, function(x) {
      fn(x, maxy)
    })
    // right
    dim1LinearMotion(maxy, miny, numSamplesR, function(y) {
      fn(maxx, y)
    })
    // bottom
    dim1LinearMotion(maxx, minx, numSamplesB, function(x) {
      fn(x, miny)
    })
    // left
    dim1LinearMotion(miny, maxy, numSamplesL, function(y) {
      fn(minx, y)
    })
  }
}

//
// 3-dimensions
//

//
// dim3LinearMotion
//
function dim3LinearMotion(fromVal, toVal, numSamples, fn) {
  var dVal = Vec3.divn(Vec3.minus(toVal, fromVal), numSamples)
  for (var sample = 0, val = fromVal; sample < numSamples; sample++, val = Vec3.plus(val, dVal)) {
    fn(val)
  }
  // not including the last point
}

//
// exports
//

exports.dim1LinearMotion = dim1LinearMotion
exports.dim1SineWave = dim1SineWave
exports.dim2LinearMotion = dim2LinearMotion
exports.dim2WaveSquare = dim2WaveSquare
exports.dim3LinearMotion = dim3LinearMotion
