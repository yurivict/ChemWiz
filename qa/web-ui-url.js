

exports.run = function() {
  var resHttp = downloadUrl("http://time.gov/")
  var resHttps = downloadUrl("https://reddit.com/")

  if (resHttp.length > 0 && resHttps.length > 0)
    return "OK"
  else
    return "FAIL"
}
