// module CalcNWChem: quantum chemistry computation module using NWChem (http://www.nwchem-sw.org/)

var name = "NWChem"
var runsDir = ".calc-runs"
var executable = "nwchem"
var deftBasis = "6-311G*"

//
// utils
//

function createRunDir(rname, params) {
  var formDirName = function(runsDir, timestamp, rname) {
    return runsDir+'/'+name+"/"+timestamp+"-"+rname
  }
  if (!params.reprocess) {
    var timestamp = Time.currentDateTimeStr()
    var fullDir = formDirName(runsDir, timestamp, rname)
    if (File.ckdir(fullDir))
      throw "run directory for a new process '"+params.reprocess+"' already exists"
    File.mkdir(runsDir) // ignore failure by design
    File.mkdir(runsDir+"/"+name) // ignore failure by design
    File.mkdir(fullDir)
  } else {
    var fullDir = formDirName(runsDir, params.reprocess, rname)
    if (!File.ckdir(fullDir))
      throw "run directory to reprocess '"+params.reprocess+"' doesn't exist"
  }
  return fullDir
}

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
// input
//

function formInpForOptimize(m, params) {
  var inp = ""
  inp += 'title "todo-title"\n'
  inp += 'echo\n'
  inp += 'geometry units angstroms\n'
  inp += m.toXyzCoords()
  inp += 'end\n'
  inp += 'basis\n'
  for (var elts = m.allElements(), i = 0; i < elts.length; i++)
    inp += ' '+elts[i]+' library '+deftBasis+'\n'
  inp += 'end\n'
  inp += 'scf\n'
  if (params.precision != undefined)
    inp += '  thresh '+params.precision+'\n'
  inp += 'end\n'
  inp += 'task scf optimize\n'
  return inp
}

//
// output processing
//

function findErrors(lines) {
  for (var i = 0; i < lines.length; i++)
    if (lines[i].search("ERROR") >= 0)
      return lines[i]
  if (lines.length < 25)
    return "too few lines in the output ("+lines.length+"), did the process fail?"
}

function parseMoleculeHeaderLnos(lines) {
  var lnos = []
  for (var i = 0; i < lines.length; i++)
    if (lines[i].search("Output coordinates in angstroms") == 1)
      lnos.push(i)
  return lnos
}

function parseCoordsSection(lines, lno, m) {
  if (lines[++lno] != "")
    xthrow("coordinates format error (no empty line after the header)")
  if (lines[++lno] != "  No.       Tag          Charge          X              Y              Z")
    xthrow("coordinates format error (column names)")
  if (lines[++lno] != " ---- ---------------- ---------- -------------- -------------- --------------")
    xthrow("coordinates format error (dash separators)")
  var mo = new Molecule("")
  for (lno++; lines[lno] != ""; lno++) {
    var cols = arrRmEmpty(lines[lno].split(' '))
    if (cols.length != 6)
      xthrow("coordinates format error (expected 6 columns, found "+cols.length+", line='"+lines[lno]+"')")
    mo.addAtom(new Atom(cols[1], [cols[3], cols[4], cols[5]]))
  }
  if (mo.numAtoms() != m.numAtoms())
    xthrow("mismatch in the atom count: found "+mo.numAtoms()+" atoms, original molecule had "+m.numAtoms()+" atoms")
  // TODO compare elements in both molecules?
  mo.detectBonds();
  return mo
}

function parseLastCoordSection(lines, m) {
  // find headers
  var molHeaders = parseMoleculeHeaderLnos(lines)
  if (molHeaders.length == 0)
    xthrow("no output coordinates found")
  // parse the last molecule
  return parseCoordsSection(lines, molHeaders[molHeaders.length-1], m)
}

function parseAllCoordSections(lines, m) {
  // find headers
  var molHeaders = parseMoleculeHeaderLnos(lines)
  if (molHeaders.length == 0)
    xthrow("no output coordinates found")
  // parse the last molecule
  var mols = []
  for (var i = 0; i < molHeaders.length; i++)
    mols.push(parseCoordsSection(lines, molHeaders[i], m))
  return mols
}

//
// export: creates the instance of the NWChem calculation module
//
exports.create = function() {
  var mod = {}
  mod.toString = function() {return name+" calc module"}
  // iface
  mod.kind = function() {return name}
  mod.calcEnergy = function(m, params) {
    throw "TODO NWChem.calcEnergy"
  }
  mod.calcOptimized = function(m, params) {
    var runDir = createRunDir("optimize", params)
    if (!params.reprocess) {
      File.write(formInpForOptimize(m, params), runDir+"/inp")
      // run the process
      //var out = system("cd "+runDir+" && "+executable+" inp 2>&1 | tee outp")
      var out = system("cd "+runDir+" && mpirun -np 8 "+executable+" inp 2>&1 | tee outp")
    } else {
      var out = File.read(runDir+"/outp")
    }
    // process output
    var lines = out.split('\n')
    var err = findErrors(lines)
    if (err != undefined)
      xthrow(err)
    return parseLastCoordSection(lines, m)
  }
  mod.calcOptimizedWithSteps = function(m, params) {
    var runDir = createRunDir("optimize", params)
    if (!params.reprocess) {
      File.write(formInpForOptimize(m, params), runDir+"/inp")
      // run the process
      //var out = system("cd "+runDir+" && "+executable+" inp 2>&1 | tee outp")
      var out = system("cd "+runDir+" && mpirun -np 8 "+executable+" inp 2>&1 | tee outp")
    } else {
      var out = File.read(runDir+"/outp")
    }
    // process output
    var lines = out.split('\n')
    var err = findErrors(lines)
    if (err != undefined)
      xthrow(err)
    return parseAllCoordSections(lines, m)
  }
  return mod
}