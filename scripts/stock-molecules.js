// StockMolecules module: it contains the definitons of some molecules, their confirmations and combinations

//
// some utility functions
//


// convert angles to radians
function a2r(angle) {
  return 2*pi()*(angle/360)
}

// convert pms to Angstroms
function p2a(len) {
  return len/100
}

// Each molecule should be called {molecule-name}_{source}, because there is no general consensus on what the canonic conformation
// of a molecule should be. There are many variants.

//
// exported molecules
//
exports.h2o_wiki = function () {
  var Dh = p2a(95.84)
  var A  = a2r(104.45)

  var m = new Molecule
  m.addAtom(new Atom("O", [0,0,0]))
  m.addAtom(new Atom("H", [Dh*cos(A/2), +Dh*sin(A/2), 0]))
  m.addAtom(new Atom("H", [Dh*cos(A/2), -Dh*sin(A/2), 0]))
  return m
}

exports.h2o_dimer_2000 = function () {
  // taken from Phys. Chem. Chem. Phys., 2000, 2, 2227-2234 (DOI: 10.1039/a910312k), what they say is the optimized conformation of such dimer

  var R       = p2a(291.2)
  var Abeta   = a2r(124.4)
  var Athetaa = a2r(104.87)
  var Ra      = p2a(95.83)
  var Atheta  = a2r(104.87)
  var Aalpha  = a2r(5.5)
  var Athetad = a2r(104.83)
  var Rf      = p2a(95.69)
  var Rd      = p2a(96.39)

  var m = new Molecule
  // Oa
  m.addAtom(new Atom("O", [-R/2,0,0]))
  // Ha
  m.addAtom(new Atom("H", [-R/2+Ra*cos(Athetaa/2)*cos(Abeta), -Ra*sin(Athetaa/2), -Ra*cos(Athetaa/2)*sin(Abeta)]))
  // Hb
  m.addAtom(new Atom("H", [-R/2+Ra*cos(Athetaa/2)*cos(Abeta), +Ra*sin(Athetaa/2), -Ra*cos(Athetaa/2)*sin(Abeta)]))
  // Od
  m.addAtom(new Atom("O", [+R/2, 0, 0]))
  // Hd
  m.addAtom(new Atom("H", [+R/2-Rd*cos(Aalpha), 0, Rd*sin(Aalpha)]))
  // Hf
  m.addAtom(new Atom("H", [+R/2-Rf*cos(Aalpha+Athetad), 0, Rd*sin(Aalpha+Athetad)]))

  return m
}

