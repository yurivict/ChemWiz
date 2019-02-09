
#include "molecule.h"
#include "Exception.h"

#include <boost/format.hpp>

#include <iostream>
#include <string>
#include <map>

#include <dsrpdb/PDB.h>
#include <dsrpdb/Model.h>
#include <dsrpdb/Protein.h>

/// Element

static const char* eltNames[] = {
  "H", "He",
  "Li", "Be", "B", "C", "N", "O", "F", "Ne",
  "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar"
};

class MapEltName : public std::map<std::string,Element> {
public:
  MapEltName() {
    unsigned e = unsigned(H);
    for (auto n : eltNames)
      (*this)[n] = Element(e++);
  }
};

static MapEltName mapEltName;

std::ostream& operator<<(std::ostream &os, Element e) {
  os << eltNames[e-1];
  return os;
}

std::istream& operator>>(std::istream &is, Element &e) {
  std::string s;
  is >> s;
  e = elementFromString(s);
  return is;
}

Element elementFromString(const std::string &s) {
  auto i = mapEltName.find(s);
  if (i == mapEltName.end())
    throw Exception(str(boost::format("Not an element name: %1%") % s));
  return i->second;
}

/// Molecule

std::vector<Molecule*> Molecule::readPdbFile(const std::string &newFname) {
  std::vector<Molecule*> res;
  // read into dsrpdb::PDB
  std::ifstream file;
  file.open(newFname);
  dsrpdb::PDB pdb(file, true/*print_errors*/);

  if (pdb.number_of_models() == 0)
    throw Exception(str(boost::format("The PDB file '%1%' doesn't have any molecules in it") % newFname));

  auto dsrpdbAtomTypeToOurs = [](auto type) {
    switch (type) {
    case dsrpdb::Atom::C: return C;
    case dsrpdb::Atom::N: return N;
    case dsrpdb::Atom::H: return H;
    case dsrpdb::Atom::O: return O;
    case dsrpdb::Atom::S: return S;
    default: throw Exception("Unknown atom type in the PDB file");
    }
  };

  for (unsigned m = 0; m < pdb.number_of_models(); m++) {
    auto &model = pdb.model(m);
    auto molecule = new Molecule("");
    for (unsigned c = 0; c < model.number_of_chains(); c++) {
      auto &chain = model.chain(c);
      //std::cout << "chain#" << c << ": number_of_residues=" << chain.number_of_residues() << std::endl;
      //residueCount += chain.number_of_residues();
      for (auto a = chain.atoms_begin(), ae = chain.atoms_end(); a != ae; ++a) {
        auto &atom = *a;
        molecule->add(Atom(dsrpdbAtomTypeToOurs(atom.second.type()), Vec3(atom.second.cartesian_coords()[0], atom.second.cartesian_coords()[1], atom.second.cartesian_coords()[2])));
      }
    }
    molecule->detectBonds();
    molecule->setId(str(boost::format("%1%#%2%") % newFname % (m+1)));
    res.push_back(molecule);
  }
  return res;
}

