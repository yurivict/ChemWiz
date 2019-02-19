// StockMolecules module: it contains the definitons of some molecules, their confirmations and combinations

// some utility functions

// convert angles to radians
function a2r(angle) {
  return 2*pi()*(angle/360)
}

// convert pms to Angstroms
function p2a(len) {
  return len/100
}

exports.h2o = function () {
  var Dh = p2a(95.84)
  var A  = a2r(104.45)

  var m = new Molecule
  m.addAtom(new Atom("O", [0,0,0]))
  m.addAtom(new Atom("H", [Dh*cos(A/2), +Dh*sin(A/2), 0]))
  m.addAtom(new Atom("H", [Dh*cos(A/2), -Dh*sin(A/2), 0]))
  return m
}
