// module Group: contains procedures to find chemical groups in the molecules

exports.findCarboxylGroup = function(m) { // returns [C, O, Oh, Ratom]
  var Cs = m.findAtoms(function(a) {return a.getElement() == "C" && a.getNumBonds() == 3;})
  if (Cs.length != 1)
    return undefined // no group or many groups
  var C = Cs[0]
  var O = C.findSingleNeighbor('O');
  if (O == undefined)
    return undefined // no match
  var OH = C.findSingleNeighbor2('O', 'H');
  if (OH == undefined)
    return undefined // no match
  return [C, O, OH, C.getOtherBondOf3(O, OH)]
}

