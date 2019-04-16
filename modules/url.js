// module Url: list of various relevant URLs that can be used from ChemWiz

exports.pdbPdbGzipped = function(pdbId) {
  return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb.gz"
}

exports.pdbPdb = function(pdbId) {
  return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb"
}

exports.pdbMmtfGzipped = function(pdbId) {
  return "https://mmtf.rcsb.org/v1.0/full/"+pdbId+".mmtf.gz"
}

