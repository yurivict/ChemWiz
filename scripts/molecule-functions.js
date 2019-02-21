// MoleculeFunctions module: various advanced molecular functions defined in JS


function extractCoords(m) {
  var res = []
  var atoms = m.getAtoms()
  for (var i = 0; i < atoms.length; i++) {
    res.push(atoms[i].getPos())
  }
  return res
}

// extract coordinates of all atoms
exports.extractCoords = extractCoords

// RMSD operation between molecules
exports.rmsd = function(m1, m2) {
  return rmsd(extractCoords(m1), extractCoords(m2))
}

