// tests the Molecule.computeConvexHullFacets() function

exports.run = function() {
  // prepare: create two benzene molecules with one being displaced from another
  var m = Moleculex.fromSMILES("c1ccccc1")  // benzene
  if (true) {
    var m1 = Moleculex.fromSMILES("c1ccccc1")
    m1.transform(Mat3.rotate([0.3,0.3,0.3]), [2.,2.,2.])
    m.addMolecule(m1)
  }

  // compute
  var facets = m.computeConvexHullFacets()

  // check
  if (JSON.stringify(facets)+"\n" == File.read("qa/computeConvexHullFacets.gold"))
    return "OK"
  else
    return "FAIL"
}

