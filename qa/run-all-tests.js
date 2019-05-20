
function printStatus(res, tmStartSec, tmEndSec) {
  if (res == "OK")
    printna(["clr.fg.green"], res)
  else if (res == "FAIL")
    printna(["clr.fg.red"], res)
  else
    printna(["clr.fg.gray"], res)
  // time if too long
  if (tmEndSec > tmStartSec+1) {
    printa(["clr.fg.reset"], " (in "+(tmEndSec-tmStartSec)+" sec)")
  } else {
    printa(["clr.fg.reset"], "")
  }
}

// run-all-tests.js: runs all tests

var all_tests = ["xyz",
                 "vec3-ops", "vec3-rmsd",
                 "mat3-ops", "mat3-rotate",
                 "computeConvexHullFacets", "computeConvexHullFacets+furthestdist",
                 "web-ui-http", "web-ui-https", "web-ui-url",
                 "gzip", "mmtf",
                 "calc-erkale", "calc-nwchem"
                ]

var nSucc = 0
var nFail = 0
for (var t = 0; t < all_tests.length; t++) {
  printn("running test: "+all_tests[t]+" ... ")
  flush()
  var Test = require('qa/'+all_tests[t])
  var tmStartSec = Time.now()
  var res = Test.run()
  var tmEndSec = Time.now()
  printStatus(res, tmStartSec, tmEndSec)
  if (res == "OK")
    nSucc++
  else if (res == "FAIL")
    nFail++
  else
    throw "Invalid test output '"+res+"' for the test '"+all_tests[t]+"'"
}

if (nSucc == all_tests.length)
  printa(["clr.fg.green"], "All "+all_tests.length+" tests succeeded")
else
  printa(["clr.fg.red"], nFail+" test(s) failed out of "+all_tests.length+" total tests")
