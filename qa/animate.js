var Animate = require("animate")
var PV = require("point-visualizer-3d")

var pv = PV.create()

function evolveDim2WaveSquare() {
  // params
  var numCycles = 15
  var numSamplesPerCycle = 400
  var viewRot = Vec3.muln(Vec3.normalize([+1,-1,0]), Math.PI/5)

  var z = -1
  var dz = 1./(numSamplesPerCycle*numCycles)
  Animate.dim2WaveSquare(-1,-1,+1,+1, numSamplesPerCycle, numCycles, function(x,y) {
    pv.add(x, y, z)
    z += dz
  })

  return pv.toImage(viewRot) // return the visual representation
}

exports.run = function() {
  // dim2WaveSquare
  var img = evolveDim2WaveSquare()
  //img.saveImage("animate.bmp");
  //print("evolveDim2WaveSquare().cryptoHash()="+img.cryptoHash())

  if (img.cryptoHash() == "6c807fd5851ef438cc53a749d6ab4d2b064833eaf5a91eddf1aeb20c5c7a32b9")
    return "OK"
  else
    return "FAIL"
}

