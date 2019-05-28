var Animate = require("animate")

function evolveDim2WaveSquare() {
  // params
  var sz = 800
  var numCycles = 15
  var numSamplesPerCycle = 400
  var scaleCoef = 200
  var viewRot = Vec3.muln(Vec3.normalize([+1,-1,0]), Math.PI/5)

  var img = new Image(sz, sz)
  img.setRegion(0,0, img.width(), img.height(), 255,255,255)

  var M = Mat3.rotate(viewRot)
  var ctr = [sz/2, sz/2, sz/2]

  function rotate(pt) {
    return Vec3.plus(Mat3.mulv(M, Vec3.minus(pt, ctr)), ctr)
  }

  var z = (sz - 2*scaleCoef)/2
  var dz = 2*scaleCoef/(numSamplesPerCycle*numCycles)
  Animate.dim2WaveSquare(-1,-1,+1,+1, numSamplesPerCycle, numCycles, function(x,y) {
    //print("x="+x+" y="+y)
    var pt = rotate([x*scaleCoef+sz/2, y*scaleCoef+sz/2, z])
    img.setPixel(pt[0], pt[1], 0) // 0=black
    z += dz
  })

  return img // return the visual representation
}

exports.run = function() {
  // dim2WaveSquare
  var img = evolveDim2WaveSquare()
  //img.saveImage("animate.bmp");
  //print("evolveDim2WaveSquare().cryptoHash()="+img.cryptoHash())

  if (img.cryptoHash() == "C09139415E25BAA1FD8E6ED36927BB2A")
    return "OK"
  else
    return "FAIL"
}

