// module Jmol: renders molecules through Jmol, using the executable 'jmoldata'

function renderMoleculeToFile(m) {
  // save molecule as xyz
  var xyz = new TempFile("xyz")
  writeXyzFile(m, xyz.fname())
  // prepare files
  var output = new TempFile("png")
  var script = new TempFile("jmol-script",
    "load '"+xyz.fname()+"';\n"+
    "select all;\n"+
    "write '"+output.fname()+"';\n"
  )
  // run jmoldata command
  system("jmoldata -s "+script.fname())
  //
  return output
}

function renderMoleculeToMemory(m) {
  return renderMoleculeToFile(m).toBinary()
}

exports.renderMoleculeToFile = renderMoleculeToFile
exports.renderMoleculeToMemory = renderMoleculeToMemory

