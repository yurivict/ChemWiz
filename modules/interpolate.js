// module Interpolate

function linear3DOneSegment(b, e, n) {
  var d = Vec3.muln(Vec3.minus(e, b), 1/n)
  var segm = []
  var p = b
  for (var i = 0; i < n; i++, p = Vec3.plus(p,d))
    segm.push(p)
  segm.push(e) // segments include the end point
  return segm
}

exports.linear3D = function(pts, nsplits) {
  if (nsplits.length+1 != pts.length)
    throw "Interpolate.linear3D: mismatch between pts and nsplits"
  var segms = []
  for (var i = 0; i < nsplits.length; i++)
    segms.push(linear3DOneSegment(pts[i], pts[i+1], nsplits[i]))
  return segms
}
