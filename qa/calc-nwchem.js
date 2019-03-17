// tests the Erkale calculation engine

exports.run = function() {
  var Engine = require('calc-nwchem').create()
  var SM = require('stock-molecules')

  var cparams = {dir:"/tmp", precision: "0.001"}

  var EQ = function(v1, v2) {return Math.abs(v1-v2) < 0.001} // due to OpenBabel randomness

  // h2o
  if (!EQ(Engine.calcEnergy(SM.h2o_wiki(), cparams), -75.585429011145))
    return "FAIL"
  // benzene
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("c1ccccc1"), cparams), -229.417884355653))
    return "FAIL"
  // ammonia
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("N"), cparams), -55.869917040751))
    return "FAIL"
  // carbonic acid
  if (!EQ(Engine.calcEnergy(Moleculex.fromSMILES("O=C(O)O"), cparams), -262.163353276760))
    return "FAIL"

  return "OK"
}
