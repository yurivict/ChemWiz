// PointVisualizer-3D: converts a set of 3D points to the image

// params
var paramMargin = 0.1; // 10% on each side
var paramImgSz = 800; // 800x800

// helpers
function createFloatArray() {
  return new FloatArray8(); // FloatArray4 also works
}

function addToLims(lims, v) {
  if (v < lims[0])
    lims[0] = v;
  if (v > lims[1])
    lims[1] = v;
}

function bboxToCtr(bbox) {
  var ctr = [];
  bbox.forEach(function(lims) {
    ctr.push((lims[0]+lims[1])/2);
  });
  return ctr;
}

function createNAccBW() { // plain JavaScript, unaccelerated function
  return {
    pts: [],
    // iface
    add: function(x,y,z) {
      this.pts.push([x,y,z]);
    },
    toImage: function(viewRotVec) {
      // find the bounding box
      var bbox = [[Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE]];
      this.pts.forEach(function(pt) {
        addToLims(bbox[0], pt[0]);
        addToLims(bbox[1], pt[1]);
        addToLims(bbox[2], pt[2]);
      });
      // find the center point
      var ctr = bboxToCtr(bbox);
      // rotate, compute x/y bbox and save
      var M = Mat3.rotate(viewRotVec);
      var bboxr = [[Number.MAX_VALUE,Number.MIN_VALUE], [Number.MAX_VALUE,Number.MIN_VALUE]];
      var ptr = [];
      this.pts.forEach(function(pt) {
        var r = Vec3.plus(Mat3.mulv(M, Vec3.minus(pt, ctr)), ctr);
        addToLims(bboxr[0], r[0]);
        addToLims(bboxr[1], r[1]);
        ptr.push([r[0], r[1]]); // only keep x,y
      });
      // find the center point of rotated points
      var ctrr = bboxToCtr(bboxr);
      // compute the scaling coefficient
      var scale = Math.min(paramImgSz*(1-2*paramMargin)/(bboxr[0][1]-bboxr[0][0]),
                           paramImgSz*(1-2*paramMargin)/(bboxr[1][1]-bboxr[1][0]));
      // create image
      var img = new Image(paramImgSz, paramImgSz);
      img.setRegion(0,0, img.width(), img.height(), 255,255,255);
      ptr.forEach(function(pt) {
        img.setPixel(paramImgSz/2+(pt[0]-ctrr[0])*scale, paramImgSz/2+(pt[1]-ctrr[1])*scale, 0); // 0=black
      });
      //
      return img;
    }
  };
}

function createAccBW() { // accelerated through FloatArray functions
  return {
    pts: createFloatArray(),
    // iface
    add: function(x,y,z) {
      this.pts.append3(x,y,z);
    },
    toImage: function(viewRotVec) {
      // find the bounding box
      var bbox = this.pts.bbox(3);
      // find the center point
      var ctr = bboxToCtr(bbox);
      // rotate, compute x/y bbox and save
      var M = Mat3.rotate(viewRotVec);
      var ptr = this.pts.createMulMat3PlusVec3(M, Vec3.minus(ctr, Mat3.mulv(M, ctr)));
      var bboxr = ptr.bbox(3);
      // find the center point of rotated points
      var ctrr = bboxToCtr(bboxr);
      // compute the scaling coefficient
      var scale = Math.min(paramImgSz*(1-2*paramMargin)/(bboxr[0][1]-bboxr[0][0]),
                           paramImgSz*(1-2*paramMargin)/(bboxr[1][1]-bboxr[1][0]));
      // create image
      var img = new Image(paramImgSz, paramImgSz);
      img.setRegion(0,0, img.width(), img.height(), 255,255,255);
      ptr.mulScalarPlusVec3([scale, scale, 1], [paramImgSz/2-ctrr[0]*scale, paramImgSz/2-ctrr[1]*scale, 0]);
      img.setPixelsFromArray8(ptr, 3, 0, 1, 0); // 3=period, 0=x, 1=y, 0=black
      //
      return img;
    }
  };
}

function createAccCLR() { // accelerated through FloatArray functions
  return {
    ptsBin: new Binary(),
    // iface
    add: function(x,y,z,clr) {
      var This = this;
      [x,y,z].forEach(function(c) {
        This.ptsBin.appendFloat8(c);
      });
      (clr != undefined ? clr : [0,0,0]).forEach(function(c) {
        This.ptsBin.appendByte(c); // 0..255 byte
      });
    },
    toImage: function(viewRotVec) {
      //print("> toImage")
      // find the bounding box
      var bbox = this.ptsBin.bboxFloat8(3, 0, 3); // 3=ndims, 0=offset of coords in the area, 3="padding" where color is
      // find the center point
      var ctr = bboxToCtr(bbox);
      // rotate, compute x/y bbox and save
      var M = Mat3.rotate(viewRotVec);
      //print("toImage: ptsBin.size="+this.ptsBin.size())
      var ptrBin = this.ptsBin.createMulMat3PlusVec3Float8(0, 3, M, Vec3.minus(ctr, Mat3.mulv(M, ctr))); // 0=offset of coords in the area, 3="padding" where color is
      //print("toImage: ptrBin="+ptrBin+" sz="+ptrBin.size())
      var bboxr = ptrBin.bboxFloat8(3, 0, 3); // 3=ndims 0=offCoords 3=trailing
      //print("toImage: bboxr="+JSON.stringify(bboxr))
      // find the center point of rotated points
      var ctrr = bboxToCtr(bboxr);
      // compute the scaling coefficient
      var scale = Math.min(paramImgSz*(1-2*paramMargin)/(bboxr[0][1]-bboxr[0][0]),
                           paramImgSz*(1-2*paramMargin)/(bboxr[1][1]-bboxr[1][0]));
      // create image
      var img = new Image(paramImgSz, paramImgSz);
      img.setRegion(0,0, img.width(), img.height(), 255,255,255);
      ptrBin.mulScalarPlusVec3Float8(0, 3, [scale, scale, 1], [paramImgSz/2-ctrr[0]*scale, paramImgSz/2-ctrr[1]*scale, 0]); // 0=leading, 3=trailing
      img.setPixelsFromBinaryFloat8(ptrBin, 0, 8, 24, 27); // 0=offset of coordX, 8=offset of coordY, in the area, 24=color offset, 27=period (3*8+3)
      //
      return img;
    }
  };
}

exports.createBW = createAccBW;       // default
exports.createNAccBW = createNAccBW;
exports.createAccBW = createAccBW;
exports.createAccCLR = createAccCLR;
