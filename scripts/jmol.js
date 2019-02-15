// module Jmol: renders molecules through Jmol, using the executable 'jmoldata'

var imageFormat = 'PNG'
var deftImageSz = [800, 600]

function scriptXyzToPngInstruction(xyzFile, pngFile, sz) {
  return "load '"+xyzFile.fname()+"';\n"+
         "write IMAGE "+sz[0]+" "+sz[1]+" "+imageFormat+" '"+pngFile.fname()+"';\n"
}

function runJmoldata(scriptFile) {
  print("> running jmoldata")
  system("jmoldata -s "+scriptFile.fname())
  print("< running jmoldata")
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

function renderMoleculeToFile(m, params) {
  // image size
  var sz = paramsToSz(params)
  // save molecule as xyz
  var xyz = new TempFile("xyz")
  writeXyzFile(m, xyz.fname())
  // prepare files
  var output = new TempFile("png")
  var script = new TempFile("jmol-script", scriptXyzToPngInstruction(xyz, output, sz))
  // run jmoldata command
  runJmoldata(script)
  //
  return output
}

function renderMoleculesToFiles(molecules, params) {
  // image size
  var sz = paramsToSz(params)
  // save molecules as xyz files, create output files, create Jmol script
  var xyzFiles = []
  var pngFiles = []
  var script = ""
  for (var i = 0; i < molecules.length; i++) {
    var xyzFile = new TempFile("xyz")
    writeXyzFile(molecules[i], xyzFile.fname())
    xyzFiles.push(xyzFile)
    var pngFile = new TempFile("png")
    pngFiles.push(pngFile)
    script += scriptXyzToPngInstruction(xyzFile, pngFile, sz)
  }
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

