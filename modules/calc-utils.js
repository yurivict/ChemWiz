// module CalcUtils: common utility functions of all CalcXxx modules

var runsDir = ".calc-runs"

exports.createRunDir = function(ename, rname, params) {
  var formDirName = function(runsDir, timestamp, rname) {
    return runsDir+'/'+ename+"/"+timestamp+"-"+rname
  }
  if (!params.reprocess) {
    var timestamp = Time.currentDateTimeStr()
    var fullDir = formDirName(runsDir, timestamp, rname)
    if (File.ckdir(fullDir))
      throw "run directory for a new process '"+params.reprocess+"' already exists"
    File.mkdir(runsDir) // ignore failure by design
    File.mkdir(runsDir+"/"+ename) // ignore failure by design
    File.mkdir(fullDir)
  } else {
    var fullDir = formDirName(runsDir, params.reprocess, rname)
    if (!File.ckdir(fullDir))
      throw "run directory to reprocess '"+params.reprocess+"' doesn't exist"
  }
  return fullDir
}

