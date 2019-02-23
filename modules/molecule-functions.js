// MoleculeFunctions module: various advanced molecular functions defined in JS

//
// helpers
//

function fmtAngle(angle) {
  return "∠"+formatFp(angle, 2)+"°"
}

function fmtDistanceA(dist) {
  return "l="+formatFp(dist, 2)+"Å"
}

function extractCoords(m) {
  var res = []
  var atoms = m.getAtoms()
  for (var i = 0; i < atoms.length; i++) {
    res.push(atoms[i].getPos())
  }
  return res
}

//
// extractCoords: extract coordinates of all atoms
//
exports.extractCoords = extractCoords

//
// rmsd: computes RMSD between molecules
//
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

//
// traverseForward: traverses atoms beginning from the bond atom1 -> atom2, calls function fn(atomId, atom, lev) per object
//
exports.traverseForward = function(m, atom1, atom2, fn) {
  var seen = {}
  var todo = []
  function traverseBonds(a, lev) {
    var bonds = a.getBonds()
    for (var b = 0; b < bonds.length; b++) {
      var bondAtom = bonds[b]
      var bondAtomId = bondAtom.id()
      if (!(bondAtomId in seen)) {
        todo.push([bondAtomId, bondAtom, lev])
      }
    }
  }
  seen[atom1.id()] = atom1
  seen[atom2.id()] = atom2
  var dist = 0 // distance from atom2
  var a = atom2
  // seed
  traverseBonds(atom2, 1)
  // iterate
  while (todo.length > 0) {
    var tri = todo.pop()
    fn(tri[0], tri[1], tri[2])
    seen[tri[0]] = tri[1]
    traverseBonds(tri[1], tri[2]+1)
  }
}

//
// findLevelsFrom: find atom levels beginning from the bond atom1 -> atom2, returns the array of levels
//
exports.findLevelsFrom = function(m, atom1, atom2) {
  var res = []
  exports.traverseForward(m, atom1, atom2, function(id,a,lev) {
    // resize
    while (lev >= res.length) {
      res.push([])
    }
    // add
    res[lev].push([id,a])
  })
  return res
}

//
// printGeometryDetails: accepts the output of findLevelsFrom, prints inter-level distances and angles
//

exports.printGeometryDetails = function(levels) {
  function descrAtom(a, lev) {
    return a.getElement()+"@"+lev
  }
  for (var l = 1; l < levels.length; l++) {
    var levPrev = levels[l-1]
    var levCurr = levels[l]
    for (var p = 0; p < levPrev.length; p++)
      for (var c1 = 0; c1 < levCurr.length; c1++)
        if (levPrev[p][1].hasBond(levCurr[c1][1]))
          for (var c2 = c1+1; c2 < levCurr.length; c2++)
            if (levPrev[p][1].hasBond(levCurr[c2][1])) {
              var prev = levPrev[p][1]
              var curr1 = levCurr[c1][1]
              var curr2 = levCurr[c2][1]
              print("level #"+l+": "+descrAtom(curr1, l)+"‒"+descrAtom(prev, l-1)+"‒"+descrAtom(curr2, l)+":"
                +" "+fmtAngle(prev.angleBetweenDeg(curr1, curr2))
                +" "+fmtDistanceA(prev.distance(curr1))
                +" "+fmtDistanceA(prev.distance(curr2))
                +" "+fmtDistanceA(curr1.distance(curr2))
              )
            }
  }
}
