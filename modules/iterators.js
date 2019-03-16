// Iterators module: it contains functions to iterate through values of various kinds

exports.angles = function(num, axis, orth, cb) { // 'num' specifies how many values to divide the circles/sphere into
  var fnRotateAroundAxis = function(descr, R) {
    [0,1,2,3].forEach(function(a) {
      cb(descr+"-"+a, Mat3.mul(Mat3.rotate(Vec3.muln(axis, a*Math.PI/2)), R))
    })
  }
  if (num == 4) {
    // rotate around the main axis (front)
    fnRotateAroundAxis("F", Mat3.identity())
    // rotate around the main axis (back)
    fnRotateAroundAxis("B", Mat3.muln(Mat3.rotate(orth), Math.PI));
    // orth rotations
    [-1,+1].forEach(function(a) {
      fnRotateAroundAxis("M0"+(a>0 ? '+'+a : a), Mat3.rotate(Vec3.muln(orth, a*Math.PI/2)))
    })
    // orth1 rotations
    var orth1 = Vec3.cross(axis, orth);
    [-1,+1].forEach(function(a) {
      fnRotateAroundAxis("M1"+(a>0 ? '+'+a : a), Mat3.rotate(Vec3.muln(orth1, a*Math.PI/2)))
    })
  } else {
    throw "unsupported num="+num
  }
}
