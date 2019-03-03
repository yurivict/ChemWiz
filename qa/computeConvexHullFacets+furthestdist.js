// tests the Molecule.computeConvexHullFacets() function

function toData(facets, furthestdist) {
  return JSON.stringify({facets: facets, furthestdist: furthestdist})+"\n"
}

exports.run = function() {
  // prepare
  //var m = Moleculex.fromSMILES("CC(=O)C") // acetone (OpenBabel randomizes coordinates for acetone, but for benzene for some reason)
  var m = Moleculex.fromSMILES("c1ccccc1") // benzene
  var furthestdist = []

  // compute
  var facets = m.computeConvexHullFacets(furthestdist)

  // check
  if (toData(facets, furthestdist) == File.read("qa/computeConvexHullFacets+furthestdist.gold"))
    return "OK"
  else
    return "FAIL"
}
