var Animate = require("animate")
var PV = require("point-visualizer-3d")


function evolveDim1SineWave() {
  // params
  var numCyclesMin = 2
  var numCyclesMax = 10
  var numCyclesDlt = 2
  var numSamplesPerCycle = 400
  var viewRot = Vec3.muln(Vec3.normalize([+1,-1,0]), Math.PI/3)

  var pv = PV.createBW()

  var z = -1.
  var dz = 2./((numCyclesMax - numCyclesMin) / numCyclesDlt)
  for (var numCycles = numCyclesMin; numCycles <= numCyclesMax; numCycles += numCyclesDlt, z += dz) {
    var x = -1.
    var dx = 2./(numSamplesPerCycle*numCycles)
    Animate.dim1SineWave(-1,+1, numSamplesPerCycle, numCycles, function(y) {
      //print("x="+x+" y="+y+" z="+z)
      pv.add(x, y, z)
      x += dx
    })
  }

  return pv.toImage(viewRot)
}

function evolveDim2WaveSquare() {
  // params
  var numCycles = 15
  var numSamplesPerCycle = 400
  var viewRot = Vec3.muln(Vec3.normalize([+1,-1,0]), Math.PI/5)

  var pv = PV.createBW()

  var z = -1
  var dz = 1./(numSamplesPerCycle*numCycles)
  Animate.dim2WaveSquare(-1,-1,+1,+1, numSamplesPerCycle, numCycles, function(x,y) {
    pv.add(x, y, z)
    z += dz
  })

  return pv.toImage(viewRot)
}

exports.run = function() {
  // dim1SineWave
  var img = evolveDim1SineWave()
  //img.saveImage("animateDim1SineWave.bmp");
  //print("evolveDim1SineWave().cryptoHash()="+img.cryptoHash())
  if (img.cryptoHash() != "0d58e794b5da5bd61750b949bd06aade3d79e0a887ed797921224bf90d89d27f")
    return "FAIL"

  // dim2WaveSquare
  var img = evolveDim2WaveSquare()
  //img.saveImage("animateDim2WaveSquare.bmp");
  //print("evolveDim2WaveSquare().cryptoHash()="+img.cryptoHash())
  if (img.cryptoHash() != "6c807fd5851ef438cc53a749d6ab4d2b064833eaf5a91eddf1aeb20c5c7a32b9")
    return "FAIL"

  return "OK"
}

