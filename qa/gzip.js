
function test(data, nDupl) {
  for (var i = 0; i < nDupl; i++)
    data = data+data
  var b = new Binary(data)
  // print("uncompressedSize="+b.size())
  var gzData = gzip(b)
  //print("compresseedSize="+gzData.size())
  var uData = gunzip(gzData)

  if (data == uData)
    return true // success
  else
    return false // failure
}

exports.run = function() {
  var test5 = test("Hello!", 5)
  var test15 = test("Hello!", 15)
  var test25 = test("Hello!", 25) // uncompressed size ~= 201MB, compressed size ~= 293kB

  if (test5 && test15 && test25)
    return "OK"
  else
    return "FAIL"
}
