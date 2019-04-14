
exports.run = function() {
  var Url = require('url')

  var pdbRecord = "4HHB"
  var m = Mmtf.readBuffer(gunzip(downloadUrl(Url.pdbMmtfGzipped(pdbRecord))))

  if (m.numAtoms() == 4779)
    return "OK"
  else
    return "FAIL"
}
