
exports.run = function() {
  var SM = require('stock-molecules')
  var goldXyz = "3\ncreated-in-script\nO 0.00000 0.00000 0.00000\nH 0.58708 0.75754 0.00000\nH 0.58708 -0.75754 0.00000\n"

  if (SM.h2o_wiki().toXyz() == goldXyz)
    return "OK"
  else
    return "FAIL"
}
