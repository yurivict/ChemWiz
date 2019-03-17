// tests the Erkale calculation engine

exports.run = function() {
  var Engine = require('calc-erkale').create()
  var SM = require('stock-molecules')

  var cparams = {dir:"/tmp", precision: "0.001"}

  var EQ = function(v1, v2) {return Math.abs(v1-v2) < 0.001} // due to OpenBabel randomness

  // h2o
  if (!EQ(Engine.calcEnergy(SM.h2o_wiki(), cparams), -75.40752100))
    return "FAIL"
  // benzene
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("c1ccccc1"), cparams), -228.83114495))
    return "FAIL"
  // ammonia
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("N"), cparams), -55.74210771))
    return "FAIL"
  // carbonic acid
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("O=C(O)O"), cparams), -261.58740458))
    return "FAIL"

  return "OK"
}
