// module Jmol: renders molecules through Jmol, using the executable 'jmoldata'

var imageFormat = 'PNG'
var deftImageSz = [800, 600]

function renderMoleculeToFile(m, params) {
  // image size
  var szx, szy
  if (params != undefined && params.szX != undefined && params.szY != undefined) {
    szx = params.szX
    szy = params.szY
  } else {
    szx = deftImageSz[0]
    szy = deftImageSz[1]
  }
  // save molecule as xyz
  var xyz = new TempFile("xyz")
  writeXyzFile(m, xyz.fname())
  // prepare files
  var output = new TempFile("png")
  var script = new TempFile("jmol-script",
    "load '"+xyz.fname()+"';\n"+
    "write IMAGE "+szx+" "+szy+" "+imageFormat+" '"+output.fname()+"';\n"
  )
  // run jmoldata command
  system("jmoldata -s "+script.fname())
  //
  return output
}

function renderMoleculeToMemory(m, params) {
  return renderMoleculeToFile(m, params).toBinary()
}

exports.renderMoleculeToFile = renderMoleculeToFile
exports.renderMoleculeToMemory = renderMoleculeToMemory

