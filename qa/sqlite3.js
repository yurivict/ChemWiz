
// params
var dbFileName = "/tmp/qa-run-tm"+Time.now()+".sqlite"
var numRecs = 30000
var delta = 17

// helpers
function sumSeq(v1,v2) {
  return (v2-v1+1)*(v2+v1)/2
}

exports.run = function() {
  //
  // create DB
  //
  var db = require('sqlite3').openDatabase(dbFileName)
  //var db = require('sqlite3').openDatabase(":memory:")
  db.run("CREATE TABLE tbl1(fldi int, flds char[256]);")

  //
  // insert values
  //
  for (var i = 1; i <= numRecs; i++)
    db.run("INSERT INTO tbl1 VALUES ("+i+", 'value="+i+"');")

  //
  // read back #1
  //
  var sumVal1 = 0
  var sumCnt1 = 0
  db.run("SELECT sum(fldi) FROM tbl1;", function(opaque,nCols,arg3,arg4) {
    sumVal1 = arg3
    sumCnt1++
    return 0
  })

  //
  // update values
  //
  db.run("UPDATE tbl1 SET fldi = fldi+"+delta+";")

  //
  // read back #2
  //
  var sumVal2 = 0
  var sumCnt2 = 0
  db.run("SELECT sum(fldi) FROM tbl1;", function(opaque,nCols,arg3,arg4) {
    sumVal2 = arg3
    sumCnt2++
    return 0
  })

  //
  // close
  //
  db.close()

  // cleanup
  File.unlink(dbFileName)

  // check and return result
  if (sumCnt1 == 1 && sumVal1 == sumSeq(1,numRecs) && sumCnt2 == 1 && sumVal2 == sumSeq(1+delta, numRecs+delta))
    return "OK"
  else
    return "FAIL"
}
