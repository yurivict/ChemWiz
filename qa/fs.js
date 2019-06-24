
var fs = require('fs');

//
// helpers
//

function genTmpFile(testName) {
  return "/tmp/test-"+testName+"tm"+Time.now();
}

function genRandomString(sz) {
  var str = "x"+Math.random()+"xy"+Math.random()+"xyz"+Math.random();
  while (str.length < sz)
    str = str+str;
  return str.substring(0, sz);
}

function largeSubstr(str, num) { // assume num << str.length
  return str.substring(0, num)+"..."+str.substring(str.length-num, str.length);
}

//
// exports
//

exports.run = function() {
  var tests = {
    // unlink
    unlink_nonexistent: function() {
      fs.unlink(genTmpFile("unlink_nonexistent"), function(err) {
        if (err != "No such file or directory")
          throw "unlink(nonexistent): expected the error 'No such file or directory', got '"+err+"'";
      });
    },
    unlink_existing: function() {
      var fname = genTmpFile("unlink_existing");
      Process.system("touch "+fname);
      fs.unlink(fname, function(err) {
        if (err != undefined)
          throw "unlink(existing): expected no error, got error '"+err+"'";
      });
    },
    // rename
    rename_nonexistent: function() {
      var fname = genTmpFile("rename_nonexistent");
      fs.rename(fname, fname+"x", function(err) {
        if (err != "No such file or directory")
          throw "rename(nonexistent): expected the error 'No such file or directory', got error '"+err+"'";
      });
    },
    rename_existing_small: function() {
      var fname = genTmpFile("rename_existing_small");
      Process.system("echo abc > "+fname);
      fs.rename(fname, fname+"x", function(err) {
        if (err != undefined)
          throw "rename(existing): expected no error, got error '"+err+"'";
      });
    },
    // readFile
    readFile_nonexistent: function() {
      fs.readFile(genTmpFile("readFile_nonexistent"), function(err, data) {
        if (err != "No such file or directory")
          throw "readFile(nonexistent): expected the error 'No such file or directory', got error '"+err+"'";
      });
    },
    readFile_existing_small: function() {
      var fname = genTmpFile("readFile_existing_small");
      File.write("abc", fname);
      fs.readFile(fname, function(err, data) {
        if (err)
          throw "readFile(existing_small): failed with err='"+err+"' while not expected an error";
        if (data != "abc")
          throw "readFile(existing_small): expected the data 'abc', got data '"+data+"'";
      });
    },
    readFile_existing_large: function() {
      var fname = genTmpFile("readFile_existing_large");
      var dataOrig = genRandomString(20*1024*1024); // 20MB
      File.write(dataOrig, fname);
      fs.readFile(fname, function(err, data) {
        if (err)
          throw "readFile(existing_large): failed with err='"+err+"' while not expected an error";
        if (data != dataOrig)
          throw "readFile(existing_large): expected the data '"+largeSubstr(dataOrig,16)+"', got data '"+largeSubstr(data,16)+"'";
      });
    }
  };

  try {
    Object.keys(tests).forEach(function(test) {
      tests[test]();
    });
  } catch (msg) {
    return ["FAIL", msg]
  }

  return "OK";
}
