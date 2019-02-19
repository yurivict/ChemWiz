
exports.run = function() {
  var Url = require('scripts/url')

  var pdbRecord = "4HHB"
  var m = readMmtfBuffer(gunzip(downloadUrl(Url.pdbMmtfGzipped(pdbRecord))))

  if (m.numAtoms() == 4779)
    return "OK"
  else
    return "FAIL"
}
