

exports.run = function() {
  var c = download("httpbin.org", "http", "/").toString()
  if (c.length > 0)
    return "OK"
  else
    return "FAIL"
}
