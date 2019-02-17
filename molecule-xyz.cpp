
#include "molecule.h"
#include "xerror.h"

#include <boost/format.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <memory>

/// Molecule

Molecule* Molecule::readXyzFile(const std::string &fname) {
  std::ifstream file(fname, std::ios::in);
  if (!file.is_open())
    ERROR(str(boost::format("can't open the xyz file: %1%") % fname));

  unsigned natoms = 0;
  std::string dummy;
  std::unique_ptr<Molecule> m;

  char phase = 'N'; // number
  std::string line;
  while (std::getline(file, line)) {
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
      m.reset(new Molecule(line));
      phase = 'A'; // atoms
      break;
    } case 'A': {
      std::istringstream is(line);
      Element elt;
      Float x;
      Float y;
      Float z;
      if (is >> elt >> x >> y >> z)
        m->add(Atom(elt, Vec3(x, y, z)));
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
    } case 'E': {
      std::istringstream is(line);
      if (is >> dummy)
        ERROR(str(boost::format("trailing characters after the atom data: %1%") % line));
      break;
    } default:
      unreachable();
    }
  }

  // detect bonds
  m->detectBonds();

  return m.release();
}

void Molecule::writeXyzFile(const std::string &fname) const {
  std::ofstream file(fname, std::ios::out);
  file << *this;
}

