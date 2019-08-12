// module Url: list of various relevant URLs that can be used from ChemWiz

exports.pdb = {
  //
  // below pdbId is a complete record, structureId is either a complete record, or a sub-record like 4HHB.A,1A4W.H
  //
  getPdbGzipped: function(pdbId) {
    return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb.gz"
  },
  getPdbPlain: function(pdbId) {
    return "https://files.rcsb.org/download/"+pdbId.toLowerCase()+".pdb"
  },
  getMmtfGzipped: function(pdbId) {
    return "https://mmtf.rcsb.org/v1.0/full/"+pdbId+".mmtf.gz"
  },
  // documented here: https://www.rcsb.org/pages/webservices/rest-fetch
  // PDB File Description Service
  // List all current/obsolete PDB IDs
  getCurrentPdbIdsJson: function() {
    return "https://www.rcsb.org/pdb/json/getCurrent"
  },
  getCurrentPdbIdsXml: function() {
    return "https://www.rcsb.org/pdb/rest/getCurrent"
  },
  getObsoletePdbIdsXml: function() {
    return "https://www.rcsb.org/pdb/rest/getObsolete"
  },
  // Information about release status
  isStatusByPdbIdsJson: function(pdbIds) {
    return "https://www.rcsb.org/pdb/json/idStatus?structureId="+pdbIds.join(',')
  },
  isStatusByPdbIdsXml: function(pdbIds) {
    return "https://www.rcsb.org/pdb/rest/idStatus?structureId="+pdbIds.join(',')
  },
  // List unreleased PDB IDs
  // List pre-release sequences in FASTA format
  // List biological assemblies
  // Ligands in PDB files
  // Gene Ontology terms for PDB chains
  getTermsByStructureIdsJson: function(structureIds) {
    return "https://www.rcsb.org/pdb/json/goTerms?structureId="+structureIds.join(',')
  },
  // Pfam annotations for PDB entries
  // Sequence and Structure cluster related web services
  getSequenceClusterSegmentIdJson: function(cluster, structureId) {
    return "https://www.rcsb.org/pdb/json/sequenceCluster?cluster="+cluster+"&structureId="+structureId
  },
  getSequenceClusterSegmentIdXml: function(cluster, structureId) {
    return "https://www.rcsb.org/pdb/rest/sequenceCluster?cluster="+cluster+"&structureId="+structureId
  },
  // TODO more functions
  // Third-party annotations and PDB to UniProtKB mapping
  getPdbchainfeaturesFeaturesBySegmentIdXml: function(segmentId) {
    return "https://www.rcsb.org/pdb/rest/das/pdbchainfeatures/features?segment="+segmentId
  },
  getPdbchainfeaturesSequenceBySegmentIdXml: function(segmentId) {
    return "https://www.rcsb.org/pdb/rest/das/pdbchainfeatures/sequence?segment="+segmentId
  }
}
