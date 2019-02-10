myimport('scripts/amino-acids-and-codons.js')
myimport('scripts/amino-acids.js')

var formula = "VVV"
var aa = AminoAcids.combine(formula)
writeXyzFile(aa, formula+".xyz")
