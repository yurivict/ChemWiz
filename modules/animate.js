// module Animate: miscellaneous functions that facilitate moving things in a particular fashion

//
// In QA cases we took an approach to create an image showing the animation visually, and check its crypto-hash.
// Please always create an image like this, and always see the image visually if QA cases fail.
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

/// exports

exports.dim1LinearMotion = dim1LinearMotion
exports.dim1SineWave = dim1SineWave
exports.dim2WaveSquare = dim2WaveSquare
