// module AtomUtils: miscellaneous utility functions that operate on atoms


function rotateAtoms(M, atoms) {
  for (var i = 0; i < atoms.length; i++) {
    var a = atoms[i]
    a.setPos(mulMatVec(M, a.getPos()))
  }
}

exports.rotateAtoms = rotateAtoms
