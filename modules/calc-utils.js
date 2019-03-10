// module CalcUtils: common utility functions of all CalcXxx modules

var runsDirSubdir = ".calc-runs"

exports.isValidParam = function(key) {
  if (key == "dir")
    return true
  if (key == "reprocess")
    return true
  return false
}

exports.createRunDir = function(ename, rname, params) {
  var runsDir = function() {
    if (params.dir)
      return params.dir+"/"+runsDirSubdir
    else
      return runsDirSubdir // under the current dir
  }
  var dirName = function(timestamp) {
    return runsDir()+'/'+ename+"/"+timestamp+"-"+rname
  }
  if (!params.reprocess) {
    var timestamp = Time.currentDateTimeToMs()
    var fullDir = dirName(timestamp)
    if (File.ckdir(fullDir))
      throw "run directory for a new process '"+fullDir+"' already exists"
    File.mkdir(runsDir()) // ignore failure by design
    File.mkdir(runsDir()+"/"+ename) // ignore failure by design
    File.mkdir(fullDir)
  } else {
    var fullDir = dirName(params.reprocess)
    if (!File.ckdir(fullDir))
      throw "run directory to reprocess '"+params.reprocess+"' doesn't exist"
  }
  return fullDir
}

