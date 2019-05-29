// PointVisualizer-3D: converts a set of 3D points to the image

// params
var paramMargin = 0.1 // 10% on each side
var paramImgSz = 800 // 800x800

// helpers
function addToLims(lims, v) {
  if (v < lims[0])
    lims[0] = v
  if (v > lims[1])
    lims[1] = v
}

exports.create = function() {
  return {
    pts: [],
    // iface
    add: function(x,y,z) {
      this.pts.push([x,y,z])
    },
    toImage: function(viewRotVec) {
      // find the bounding box
      var bbox = [[Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE]]
      this.pts.forEach(function(pt) {
        addToLims(bbox[0], pt[0])
        addToLims(bbox[1], pt[1])
        addToLims(bbox[2], pt[2])
      })
      // find the center point
      var ctr = [(bbox[0][0]+bbox[0][1])/2, (bbox[1][0]+bbox[1][1])/2, (bbox[2][0]+bbox[2][1])/2]
      // rotate, compute x/y bbox and save
      var M = Mat3.rotate(viewRotVec)
      var bboxr = [[Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE]]
      var ptr = []
      this.pts.forEach(function(pt) {
        var r = Vec3.plus(Mat3.mulv(M, Vec3.minus(pt, ctr)), ctr)
        addToLims(bboxr[0], r[0])
        addToLims(bboxr[1], r[1])
        ptr.push([r[0], r[1]]) // only keep x,y
      })
      // find the center point of rorared points
      var ctrr = [(bboxr[0][0]+bboxr[0][1])/2, (bboxr[1][0]+bboxr[1][1])/2]
      // compute scaling coefficient
      var scalex = paramImgSz*(1-2*paramMargin)/(bboxr[0][1]-bboxr[0][0])
      var scaley = paramImgSz*(1-2*paramMargin)/(bboxr[1][1]-bboxr[1][0])
      var scale = Math.min(scalex, scaley)
      // create image
      var img = new Image(paramImgSz, paramImgSz)
      img.setRegion(0,0, img.width(), img.height(), 255,255,255)
      ptr.forEach(function(pt) {
        img.setPixel(paramImgSz/2+(pt[0]-ctrr[0])*scale, paramImgSz/2+(pt[1]-ctrr[1])*scale, 0) // 0=black
      })
      //
      return img
    }
  }
}
