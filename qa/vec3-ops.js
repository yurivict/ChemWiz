
exports.run = function() {
  var eps = 0.000001

  if (Vec3.almostEquals(Vec3.plus([1.1,2.2,3.3],[11.2,12.3,13.4]), [12.3,14.5,16.7], eps))
    return "OK"
  else
    return "FAIL"
}
