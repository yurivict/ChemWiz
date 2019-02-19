

exports.run = function() {
  var c = download("time.gov", "http", "/").toString()
  if (c.length > 0)
    return "OK"
  else
    return "FAIL"
}
