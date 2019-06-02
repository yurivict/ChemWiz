

exports.run = function() {
  var resHttp = downloadUrl("http://httpbin.org/").toString()
  var resHttps = downloadUrl("https://www.reddit.com/").toString()

  if (resHttp.length > 0 && resHttps.length > 0)
    return "OK"
  else
    return "FAIL"
}
