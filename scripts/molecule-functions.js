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

exports.printBondLengths = function(m) {
  var aa = m.getAtoms()
  for (var a = 0; a < aa.length; a++) {
    var atom = aa[a]
    print("- atom#"+(a+1)+": elt="+atom.getElement()+" pos="+atom.getPos())
    var bonds = atom.getBonds()
    for (var b = 0; b < bonds.length; b++) {
      var bondAtom = bonds[b]
      print(" ---> bond#"+(b+1)+": elt="+bondAtom.getElement()+" pos="+bondAtom.getPos()+" bondLength="+vecLength(vecMinus(atom.getPos(),bondAtom.getPos())))
    }
  }
}

