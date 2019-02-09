// requires amino-acids-and-codons.js


var AminoAcids = {
  xyzPath: "molecules/Amino_Acids/",
  codeToName: function(code) {
    return AMINO_ACIDS[code].name
  },
  codeToFile: function(code) {
    return AminoAcids.xyzPath+"L-"+AminoAcids.codeToName(code)+".xyz"
  },
  decodePeptide: function(peptide, f) {
    for (var i = 0; i < peptide.length; i++) {
      f(i, peptide.charAt(i))
    }
  },
  combine: function(peptide) {
    var peptideChain;
    AminoAcids.decodePeptide(peptide, function(i,code) {
      var aaMolecule = readXyzFile(AminoAcids.codeToFile(code))
      if (i == 0) {
        peptideChain = aaMolecule
      } else {
        moleculeAppendAminoAcid(peptideChain, aaMolecule)
      }
    })
    return peptideChain
  }
}

