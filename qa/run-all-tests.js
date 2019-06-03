// run-all-tests.js: runs all QA tests and reports the results

// the list of cases

var all_tests = ["xyz",
                 "vec3-ops", "vec3-rmsd",
                 "mat3-ops", "mat3-rotate",
                 "binary",
                 "computeConvexHullFacets", "computeConvexHullFacets+furthestdist",
                 "gzip", "mmtf",
                 "sqlite3",
                 "image",
                 "animate",
                 "web-ui-http", "web-ui-https", "web-ui-url",
                 "calc-erkale", "calc-nwchem"
                ]

// helper functions

function printStatus(res, tmStartSec, tmEndSec) {
  // res can be either a status string, or a pair of [statusString, description]
  if (Array.isArray(res)) {
    var resCode = res[0]
    var resDescr = " ("+res[1]+")"
  } else {
    var resCode = res
    var resDescr = ""
  }
  if (resCode == "OK")
    printna(["clr.fg.green"], resCode)
  else if (resCode == "FAIL")
    printna(["clr.fg.red"], resCode)
  else
    printna(["clr.fg.gray"], resCode)
  // time if too long
  if (tmEndSec > tmStartSec+1) {
    printa(["clr.fg.reset"], resDescr+" (in "+(tmEndSec-tmStartSec)+" sec)")
  } else {
    printa(["clr.fg.reset"], resDescr)
  }
  return resCode == "OK"
}

// run the QA cases
function RunAllTests() {
  var nSucc = 0
  var nFail = 0
  var tmAllStartSec = Time.now()
  for (var t = 0; t < all_tests.length; t++) {
    printn("running test: "+all_tests[t]+" ... ")
    flush()
    var Test = require('qa/'+all_tests[t])
    var tmCaseStartSec = Time.now()
    var res = Test.run()
    var tmCaseEndSec = Time.now()
    var succ = printStatus(res, tmCaseStartSec, tmCaseEndSec)
    if (succ)
      nSucc++
    else
      nFail++
  }
  var tmAllEndSec = Time.now()

  // print the QA status
  if (nSucc == all_tests.length)
    printna(["clr.fg.green"], "All "+all_tests.length+" tests succeeded")
  else
    printna(["clr.fg.red"], nFail+" test(s) failed out of "+all_tests.length+" total tests")
  printa(["clr.fg.reset"], " (in "+(tmAllEndSec-tmAllStartSec)+" secs)")
}

/// MAIN

RunAllTests()


