// module AminoAcids

var AcidsCodons = require('amino-acids-and-codons')

var xyzPath = "molecules/Amino_Acids/";

function codeToName(code) {
  return AcidsCodons.AMINO_ACIDS[code].name
}

function codeToFile(code) {
  return xyzPath+"L-"+codeToName(code)+".xyz"
}

function decodePeptide(peptide, f) {
  for (var i = 0; i < peptide.length; i++) {
    f(i, peptide.charAt(i))
  }
}

function combine(peptide) {
  var peptideChain;
  decodePeptide(peptide, function(i,code) {
    var aaMolecule = Moleculex.fromXyzOne(codeToFile(code))
    if (i == 0) {
      peptideChain = aaMolecule
    } else {
      peptideChain.appendAminoAcid(aaMolecule)
    }
  })
  return peptideChain
}

exports.codeToName = codeToName
exports.codeToFile = codeToFile
exports.decodePeptide = decodePeptide
exports.combine = combine
