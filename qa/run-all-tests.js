
// run-all-tests.js: runs all tests

var all_tests = ["xyz",
                 "vec3-rmsd",
                 "web-ui-http", "web-ui-https", "web-ui-url",
                 "gzip", "mmtf"]

var nSucc = 0
var nFail = 0
for (var t = 0; t < all_tests.length; t++) {
  printn("running test: "+all_tests[t]+" ... ")
  var Test = require('qa/'+all_tests[t])
  var res = Test.run()
  print(res)
  if (res == "OK")
    nSucc++
  else if (res == "FAIL")
    nFail++
  else
    throw "Invalid test output '"+res+"' for the test '"+all_tests[t]+"'"
}

if (nSucc == all_tests.length)
  print("All "+all_tests.length+" tests succeeded")
else
  print(nFail+" test(s) failed out of "+all_tests.length+" total tests")
