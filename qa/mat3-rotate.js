
exports.run = function() {
  var eps = 0.000001
  function EQ(m1, m2) {return Mat3.almostEquals(m1, m2, eps)}

  var angle = 2*Math.PI/17
  var dir = Vec3.muln([1,1,1], Math.pow(1/3, 1/2)*angle)
  var m = Mat3.rotate(dir)
  var m2 = Mat3.mul(m,m)
  var m4 = Mat3.mul(m2,m2)
  var m8 = Mat3.mul(m4,m4)
  var m16 = Mat3.mul(m8,m8)

  if (EQ(Mat3.mul(m16, m), Mat3.identity()))
    return "OK"
  else
    return "FAIL"
}
