// module Animate: miscellaneous functions that facilitate moving things in a particular fashion


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

exports.dim1LinearMotion = dim1LinearMotion;
exports.dim2WaveSquare = dim2WaveSquare;
