// params
var ArraySize = 10000

function addRecIF4I(bin, arg1, arg2, arg3) {
  bin.appendInt(arg1)
  bin.appendFloat4(arg2)
  bin.appendInt(arg3)
}

function getRecIF4I(bin, off) {
  return [bin.getInt(off), bin.getFloat4(off+4), bin.getInt(off+8)]
}

function genRandomArray(sz) {
  var bin = new Binary
  for (var i = 0; i < sz; i++)
    addRecIF4I(bin, Math.random()*1000000%1000, Math.random()*1000, Math.random()*1000000%1000)
  return bin
}

function getAllRecs(bin) {
  var recs = []
  for (var off = 0; off < bin.size(); off+=12)
    recs.push(getRecIF4I(bin, off))
  return recs
}

exports.run = function() {
  // test sort
  var bin = genRandomArray(ArraySize)
  bin.sortAreasByFloat4Field(12, 4, true)
  var binSortIncr = getAllRecs(bin)
  bin.sortAreasByFloat4Field(12, 4, false)
  var binSortDecr = getAllRecs(bin)
  if (JSON.stringify(binSortIncr) == JSON.stringify(binSortDecr.reverse()))
    return "OK"
  else
    return "FAIL"
}
