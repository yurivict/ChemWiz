// module Url: list of various relevant URLs that can be used from ChemWiz

exports.pdb = {
  getPdbGzipped: function(pdbId) {
    return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb.gz"
  },
  getPdbPlain: function(pdbId) {
    return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb"
  },
  getMmtfGzipped: function(pdbId) {
    return "https://mmtf.rcsb.org/v1.0/full/"+pdbId+".mmtf.gz"
  },
  getAllPdbIdsJson: function() {
    return "https://www.rcsb.org/pdb/json/getCurrent"
  },
  getAllPdbIdsXml: function() {
    return "https://www.rcsb.org/pdb/json/getCurrent"
  }
}
