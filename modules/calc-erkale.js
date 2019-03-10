// module CalcErkale: quantum chemistry computation module using Erkale (https://github.com/susilehtola/erkale)

var CalcUtils = require('calc-utils')

var name = "erkale"
var deftBasis = "3-21G"
// files
var inputXyzFile = "m.xyz"
// executables
var executableEnergy = "erkale_omp"
var executableGeomOpt = "erkale_geom_omp"

//
// local functions
//

function paramsToErkaleParams(engParams) {
  var erkParams = {}
  Object.keys(engParams).forEach(function(key) {
    if (key == "precision")
      erkParams["ConvThr"] = engParams["precision"]
    else if (key == "basis")
      erkParams["Basis"] = engParams["basis"]
    else if (!CalcUtils.isValidParam(key))
      xthrow("unexpected key '"+key+"' found in params passed to the '"+name+"' calc engine")
  })
  return erkParams
}

function defaultParams(erkParams) {
  if (erkParams.Basis == undefined)
    erkParams.Basis = deftBasis
}

function formRunfile(erkParams) {
  var runfile = ""
  runfile += "System "+inputXyzFile+"\n"
  runfile += "Method lda_x-lda_c_vwn\n"
  Object.keys(erkParams).forEach(function(key) {
    runfile += key+" "+erkParams[key]+"\n"
  })
  return runfile
}

function runCalcEngine(rname, m, params, executable, fnReturn) {
  var runDir = CalcUtils.createRunDir(name, rname, params)

  // params
  var erkParams = paramsToErkaleParams(params)
  defaultParams(erkParams)

  // run the process when 'reprocess' isn't chosen, pick the pre-existing output otherwise
  if (!params.reprocess) {
    // write molecule in the xyz format
    File.write(m.toXyz(), runDir+"/"+inputXyzFile)
    // write the runfile
    File.write(formRunfile(erkParams), runDir+"/runfile")
    // run the process
    var out = system("cd "+runDir+" && "+executable+" runfile 2>&1 | tee outp")
  } else {
    var out = File.read(runDir+"/outp")
  }

  // process output
  var lines = out.split('\n')
  var err = findErrors(lines)
  if (err != undefined)
    xthrow(err)

  // remove the last empty element that is present due to the way JS splits strings
  if (lines.pop() != "")
    xthrow("last line in erkale's output doesn't end with a newline")

  // run the output processing function
  return fnReturn(runDir, lines)
}

function extractEnergyFromOutput(outputLines) {
  if (outputLines.length < 5)
    xthrow("erkale output is too short (it has "+outputLines.length+" lines)")

  if (outputLines[outputLines.length-1].indexOf("RDFT converged") != 0)
    xthrow("energy calculation failed: no signature found in the output file (last-line="+outputLines[outputLines.length-1]+")")

  var split = Arrayx.removeEmpty(outputLines[outputLines.length-2].split(" "))
  if (split.length < 3)
    xthrow("didn't find the result line in the output (line="+outputLines[outputLines.length-2]+")")

  return split[1]
}

//
// utils
//

function arrRmEmpty(arr) {
  var res = []
  for (var i = 0; i < arr.length; i++)
    if (arr[i] != "")
      res.push(arr[i])
  return res
}

function xthrow(msg) {
  throw "ERROR("+name+") "+msg
}

//
// output processing
//

function findErrors(lines) {
  //for (var i = 0; i < lines.length; i++)
  //  if (lines[i].search("ERROR") >= 0)
  //    return lines[i]
  //if (lines.length < 25)
  //  return "too few lines in the output ("+lines.length+"), did the process fail?"
  return undefined // TODO
}

//
// export: creates the instance of the NWChem calculation module
//

exports.create = function() {
  return {
    toString: function() {return name+" calc module"},
    kind: function() {return name},
    calcEnergy: function(m, params) {
      return runCalcEngine("energy", m, params, executableEnergy, function(runDir, outputLines) {
        return extractEnergyFromOutput(outputLines)
      })
    },
    calcOptimized: function(m, params) {
      return runCalcEngine("optimize", m, params, executableGeomOpt, function(runDir, outputLines) {
        return Moleculex.fromXyzOne(runDir+"/optimized.xyz")
      })
    },
    calcOptimizedWithSteps: function(m, params) {
      return runCalcEngine("optimize", m, params, executableGeomOpt, function(runDir, outputLines) {
        return Moleculex.fromXyzMany(runDir+"/optimized.xyz")
      })
    }
  }
}
