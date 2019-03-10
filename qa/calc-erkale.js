// tests the Erkale calculation engine

exports.run = function() {
  var CalcErkale = require('calc-erkale')
  var SM = require('stock-molecules')

  var Erkale = CalcErkale.create()

  var cparams = {dir:"/tmp", precision: "0.001"}

  var EQ = function(v1, v2) {return Math.abs(v1-v2) < 0.001} // due to OpenBabel randomness

  // h2o
  if (!EQ(Erkale.calcEnergy(SM.h2o_wiki(), cparams), -75.40752100))
    return "FAIL"
  // benzene
  if (!EQ(Erkale.calcEnergy(Moleculex.fromSMILES("c1ccccc1"), cparams), -228.83114495))
    return "FAIL"
  // ammonia
  if (!EQ(Erkale.calcEnergy(Moleculex.fromSMILES("N"), cparams), -55.74210771))
    return "FAIL"
  // carbonic acid
  if (!EQ(Erkale.calcEnergy(Moleculex.fromSMILES("O=C(O)O"), cparams), -261.58740458))
    return "FAIL"

  return "OK"
}
