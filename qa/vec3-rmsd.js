
exports.run = function() {
  var eps = 0.000001

  if (Vec3.rmsd([[0,0,0],[1,1,1]], [[0,0,0],[-1,1,-1]]) < eps &&
      Vec3.rmsd([[0,0,0],[1,1,1],[2,2,2]], [[0,0,0],[-1,-1,-1],[-2,-2,-2]]) < eps)
    return "OK"
  else
    return "FAIL"
}
