
exports.run = function() {
  var eps = 0.000001
  function EQ(m1, m2) {return Mat3.almostEquals(m1, m2, eps)}

  var m = [[1.1,  2.2,  3.3],
           [11.1,12.2, 13.3],
           [11.1,12.2, 13.3]]

  if (EQ(Mat3.plus(Mat3.muln(m, 3), Mat3.muln(m, 2)), Mat3.muln(m, 5)) &&
      EQ(Mat3.minus(Mat3.plus(Mat3.muln(m, 10), Mat3.muln(m, 5)), Mat3.plus(Mat3.muln(m, 7), Mat3.muln(m, 2))), Mat3.muln(m, 6)))
    return "OK"
  else
    return "FAIL"
}
