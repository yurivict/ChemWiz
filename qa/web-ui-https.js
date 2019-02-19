

exports.run = function() {
  var c = download("www.ncbi.nlm.nih.gov", "https", "/").toString()
  if (c.length > 0)
    return "OK"
  else
    return "FAIL"
}
