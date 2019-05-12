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
      peptideChain.appendAminoAcid(aaMolecule, anglesOne[0], anglesOne[1], anglesOne[2])
    }
    var prevCode = code
  })
  return peptideChain
}

exports.codeToName = codeToName
exports.codeToFile = codeToFile
exports.decodePeptide = decodePeptide
exports.combine = combine
