// module Jmol: renders molecules through Jmol, using the executable 'jmoldata'

var imageFormat = 'PNG'
var deftImageSz = [800, 600]

function scriptXyzToPngInstruction(xyzFile, pngFile, mid, sz) {
  return "load '"+xyzFile.fname()+"'\n"+
         mid+
         "write IMAGE "+sz[0]+" "+sz[1]+" "+imageFormat+" '"+pngFile.fname()+"'\n"
}

function runJmoldata(scriptFile) {
  system("jmoldata -s "+scriptFile.fname())
}

function paramsToSz(params) {
  var szx, szy
  if (params != undefined && params.szX != undefined && params.szY != undefined) {
    szx = params.szX
    szy = params.szY
  } else {
    szx = deftImageSz[0]
    szy = deftImageSz[1]
  }
  return [szx, szy]
}

function paramsToMid(params) {
  var mid = ""
  if (params != undefined && params.camRot != undefined) {
    var len = Vec3.length(params.camRot)
    var n = Vec3.normalizeZl(params.camRot, len)
    mid += "moveto 0 "+n.join(' ')+" "+(len/2/Math.PI*360)+"\n"
  }
  return mid
}

function renderMoleculeToFile(m, params) {
  // image size
  var sz = paramsToSz(params)
  // save molecule as xyz
  var xyz = new TempFile("xyz")
  writeXyzFile(m, xyz.fname())
  // prepare files
  var output = new TempFile("png")
  var script = ""
  script += "color cpk\n"
  script += "set defaultColors Jmol\n"
  //script += "set defaultColors Rasmol\n"
  script = new TempFile("jmol-script", script+scriptXyzToPngInstruction(xyz, output, paramsToMid(params), sz))
  // run jmoldata command
  runJmoldata(script)
  //
  return output
}

function renderMoleculesToFiles(molecules, params) {
  var paramsIsPerMolecule = Array.isArray(params)
  function getParams(idx) {
    return paramsIsPerMolecule ? params[idx] : params
  }
  // save molecules as xyz files, create output files, create Jmol script
  var xyzFiles = []
  var pngFiles = []
  var script = "color cpk\n"
  script += "set defaultColors Jmol\n"
  script += "set defaultColors Rasmol\n"
  var idx = 0
  molecules.forEach(function(m) {
    var mparams = getParams(idx)
    var xyzFile = new TempFile("xyz")
    writeXyzFile(m, xyzFile.fname())
    xyzFiles.push(xyzFile)
    var pngFile = new TempFile("png")
    pngFiles.push(pngFile)
    script += scriptXyzToPngInstruction(xyzFile, pngFile, paramsToMid(mparams), paramsToSz(mparams))
    idx++
  })
  // run jmoldata command
  runJmoldata(new TempFile("jmol-script", script))
  //
  return pngFiles
}

function renderMoleculeToMemory(m, params) {
  return renderMoleculeToFile(m, params).toBinary()
}

exports.renderMoleculeToFile = renderMoleculeToFile
exports.renderMoleculesToFiles = renderMoleculesToFiles
exports.renderMoleculeToMemory = renderMoleculeToMemory

