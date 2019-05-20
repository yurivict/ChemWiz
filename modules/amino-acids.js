// module AminoAcids

var AcidsCodons = require('amino-acids-and-codons')

var xyzPath = "molecules/Amino_Acids/";

function codeToName(code) {
  return AcidsCodons.AMINO_ACIDS[code].name
}

function codeToFile(code) {
  if (code != 'G') {
    return xyzPath+"L-"+codeToName(code).replace(' ', '_')+".xyz"
  } else { // Glycine doesn't have D- and L- forms - they are the same
    return xyzPath+codeToName(code).replace(' ', '_')+".xyz"
  }
}

function decodePeptide(peptide, f) {
  for (var i = 0; i < peptide.length; i++) {
    f(i, peptide.charAt(i))
  }
}

function combine(peptide, angles) {
  // internal functions
  function defaultAngles(aaCode1, aaCode2) {
    throw "defaultAngles isn't implemented yet"
  }
  //
  if (angles != undefined && angles.length != peptide.length-1)
    throw "peptide combine: unmatching angles array provided, angles.length=" + angles.length + ", expected " + (peptide.length-1)
  var peptideChain
  decodePeptide(peptide, function(i,code) {
    var aaMolecule = Moleculex.fromXyzOne(codeToFile(code))
    if (i == 0) {
      peptideChain = aaMolecule
    } else {
      var anglesOne = angles != undefined ? angles[i-1] : defaultAngles(prevCode, code)
      peptideChain.appendAminoAcid(aaMolecule, anglesOne)
    }
    var prevCode = code
  })
  return peptideChain
}

//
// convenience function that converts the amino acid backbone angles to the object, mostly for the ease of debugging
//
function anglesArrayToObject(angles) {
  var o = {}
  for (var i = 0; i < angles.length; i++) {
    switch (i) {
      case 0:   o.omega    = angles[0]; break;
      case 1:   o.phi      = angles[1]; break;
      case 2:   o.psi      = angles[2]; break;
      case 3:   o.adjN     = angles[3]; break;
      case 4:   o.adjCmain = angles[4]; break;
      case 5:   o.adjCoo   = angles[5]; break;
      case 6:   o.O2Rise   = angles[6]; break;
      case 7:   o.O2Tilt   = angles[7]; break;
      case 8:   o.PlRise   = angles[8]; break;
      case 9:   o.PlTilt   = angles[9]; break;
      default:
        throw "invalid index "+i+" in the angles array"
    }
  }
  return o
}

//
// convenience function that converts the array of amino acid backbone angles to the objects, mostly for the ease of debugging
//
function anglesArraysToObjects(angleArrays) {
  var a = []
  for (var i = 0; i < angleArrays.length; i++)
    a.push(anglesArrayToObject(angleArrays[i]))
  return a
}

//
// exports
//

exports.codeToName = codeToName
exports.codeToFile = codeToFile
exports.decodePeptide = decodePeptide
exports.combine = combine
exports.anglesArrayToObject = anglesArrayToObject
exports.anglesArraysToObjects = anglesArraysToObjects
