
#include "molecule.h"
#include "xerror.h"
#include "util.h"

#include <boost/format.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <memory>

/// Molecule

std::istream& operator>>(std::istream &is, Molecule &m) {
  unsigned natoms = 0;
  std::string dummy;

  char phase = 'N'; // number
  std::string line;
  while (phase != 'E' && std::getline(is, line)) {
    switch (phase) {
    case 'N': {
      std::istringstream is(line);
      if (!(is >> natoms))
        ERROR("no natoms in file");
      if (is >> dummy)
        ERROR(str(boost::format("garbage in the end of the line: %1%") % dummy));
      phase = 'D'; // description
      break;
    } case 'D': {
      m.setDescr(line);
      phase = 'A'; // atoms
      break;
    } case 'A': {
      std::istringstream is(line);
      Element elt;
      Float x;
      Float y;
      Float z;
      if (is >> elt >> x >> y >> z)
        m.add(Atom(elt, Vec3(x, y, z)));
      else
        ERROR(str(boost::format("no atom description found in line: %1%") % line));
      // atom line can also have a gradient in it
      unsigned ndummy = 0;
      while (is >> dummy)
        ndummy++;
      if (ndummy != 0 && ndummy != 3)
        ERROR(str(boost::format("the atom descriptor can have either 4 or 7 elements, found %1% elements in line %2%") % ndummy % line));
      if (--natoms == 0)
        phase = 'E'; // end
      break;
    } default:
      unreachable();
    }
  }

  // detect bonds
  m.detectBonds();

  return is;
}

Molecule* Molecule::readXyzFileOne(const std::string &fname) { // reads a file with a single xyz section
  // open file
  std::ifstream file(fname, std::ios::in);
  if (!file.is_open())
    ERROR(str(boost::format("can't open the xyz file for reading: %1%") % fname));
  // create
  std::unique_ptr<Molecule> output(new Molecule(""));
  // read
  file >> *output.get();
  // see if there's a trailing data
  std::string dummy;
  file >> dummy;
  if (!dummy.empty())
    ERROR(str(boost::format("trailing characters after the atom data: %1%") % dummy));
  //
  return output.release();
}

std::vector<Molecule*> Molecule::readXyzFileMany(const std::string &fname) { // reads a file with a single xyz section
  // open file
  std::ifstream file(fname, std::ios::in);
  if (!file.is_open())
    ERROR(str(boost::format("can't open the xyz file for reading: %1%") % fname));
  // create
  std::vector<std::unique_ptr<Molecule>> output;
  while (!Util::realEof(file)) {
    Molecule *m;
    // create a molecule
    output.push_back(std::unique_ptr<Molecule>(m = new Molecule("")));
    // read
    file >> *m;
  }
  // return as a plain vector
  std::vector<Molecule*> res;
  for (auto &m : output)
    res.push_back(m.release());
  return res;
}

void Molecule::writeXyzFile(const std::string &fname) const {
  // open file
  std::ofstream file(fname, std::ios::out);
  if (!file.is_open())
    ERROR(str(boost::format("can't open the xyz file for writing: %1%") % fname));
  // write
  file << *this;
}

