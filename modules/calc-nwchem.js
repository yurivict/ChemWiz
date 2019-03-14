// module CalcNWChem: quantum chemistry computation module using NWChem (http://www.nwchem-sw.org/)

var CalcUtils = require('calc-utils')

var name = "NWChem"
var executable = "nwchem"
//var deftBasis = "6-311G*"
var deftBasis = "3-21G"
// files
var inputXyzFile = "m.xyz"

var numCPUs = System.numCPUs()

//
// utils
//

function runProcess(runDir) {
  if (numCPUs == 1)
    return system("cd "+runDir+" && "+executable+" inp 2>&1 | tee outp")
  else
    return system("cd "+runDir+" && mpirun -np "+numCPUs+" "+executable+" inp 2>&1 | tee outp")
}

function xthrow(msg) {
  throw "ERROR("+name+") "+msg
}

//
// input
//

function formInp(m, op, params) {
  var inp = ""
  inp += 'title "todo-title"\n'
  inp += 'echo\n'
  inp += 'geometry units angstroms\n'
  inp += ' load '+inputXyzFile+'\n'
  inp += 'end\n'
  inp += 'basis\n'
  for (var elts = m.allElements(), i = 0; i < elts.length; i++)
    inp += ' '+elts[i]+' library '+deftBasis+'\n'
  inp += 'end\n'
  inp += 'scf\n'
  if (params.precision != undefined)
    inp += '  thresh '+params.precision+'\n'
  inp += 'end\n'
  if (op == "optimize")
    inp += 'task scf optimize\n'
  else if (op == "energy")
    inp += 'task scf energy\n'
  else
    xthrow("unknown op '"+op+"'")
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
    var cols = Arrayx.removeEmpty(lines[lno].split(' '))
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

function runCalcEngine(rname, m, params, fnReturn) {
  var runDir = CalcUtils.createRunDir(name, "energy", params)
  if (!params.reprocess) {
    // write molecule in the xyz format
    File.write(m.toXyz(), runDir+"/"+inputXyzFile)
    // write the inp file
    File.write(formInp(m, rname, params), runDir+"/inp")
    // run the process
    var out = runProcess(runDir)
  } else {
    var out = File.read(runDir+"/outp")
  }
  // process output
  var lines = out.split('\n')
  var err = findErrors(lines)
  if (err != undefined)
    xthrow(err)
  // call retyurn filter and return the value
  return fnReturn(runDir, lines)
}

function extractEnergyFromOutput(outputLines) {
  for (var i = 0; i < outputLines.length; i++)
    if (outputLines[i].indexOf("Final RHF  results") >= 0)
      for (var j = i+1; j < outputLines.length; j++)
        if (outputLines[j].indexOf("Total SCF energy =") >= 0) {
          var split = outputLines[j].split(' ')
          return split[split.length-1]
        }
  xthrow("can't find the energy value in the output file")
}

//
// export: creates the instance of the NWChem calculation module
//
exports.create = function() {
  return {
    toString: function() {return name+" calc module"},
    kind: function() {return name},
    calcEnergy: function(m, params) {
      return runCalcEngine("energy", m, CalcUtils.argParams(params), function(runDir, outputLines) {
        return extractEnergyFromOutput(outputLines)
      })
    },
    calcOptimized: function(m, params) {
      return runCalcEngine("optimize", m, CalcUtils.argParams(params), function(runDir, outputLines) {
        return parseLastCoordSection(outputLines, m)
      })
    },
    calcOptimizedWithSteps: function(m, params) {
      return runCalcEngine("optimize", m, CalcUtils.argParams(params), function(runDir, outputLines) {
        return parseAllCoordSections(outputLines, m)
      })
    }
  }
}
