#include "common.h"
#include "molecule.h"
#include "Exception.h"
#include "Vec3.h"
#include "Vec3-ext.h"
#include "Mat3.h"

#include <stdio.h>

#include <map>


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

/// Atom

std::set<Atom*> Atom::dbgAllocated;

std::ostream& operator<<(std::ostream &os, const Atom &a) {
  auto prnCoord = [](Float c) {
    char buf[10];
    sprintf(buf, "%.05lf", c);
    return std::string(buf);
  };
  os << a.elt << ' ' << prnCoord(a.pos(X)) << ' ' << prnCoord(a.pos(Y)) << ' ' << prnCoord(a.pos(Z));
  return os;
}

/// Molecule

std::set<Molecule*> Molecule::dbgAllocated;

Molecule::Molecule(const std::string &newDescr) : descr(newDescr) {
  //std::cout << "Molecule::Molecule(copy) " << this << std::endl;
  dbgAllocated.insert(this);
}

Molecule::Molecule(const Molecule &other) : descr(other.descr) {
  //std::cout << "Molecule::Molecule() " << this << std::endl;
  for (auto a : other.atoms)
    atoms.push_back((new Atom(*a))->setMolecule(this));
  detectBonds();
  dbgAllocated.insert(this);
}

Molecule::~Molecule() {
  //std::cout << "Molecule::~Molecule " << this << std::endl;
  for (auto a : atoms)
    delete a;
  dbgAllocated.erase(this);
}

std::array<Atom*,3> Molecule::findAaNterm() { // expects that the molecule has an open N-term in the beginning
  auto atomN = findFirst(N); // heuristic step
  assert(atomN->isBonds(C,1, H,2));
  auto atomCprime = atomN->filterBonds1(C);
  // find next C on the backbone
  Atom *atomCnext = nullptr;
  for (auto a : atomCprime->bonds) // XXX no error checking
    if (a->elt == C && (a->isBonds(C,1, O,2) || a->isBonds(C,1, O,1, N,1))) {
      atomCnext = a;
      break;
    }
  // find the C-end of this AA, it is either an oxygen inside the AA (C-term), or 
  if (atomCnext->isBonds(C,1, O,2)) {
    for (auto a : atomCnext->bonds)
      if (a != atomCprime && !a->isBonds(C,1))
        return {{atomN, atomN->findFirstBond(H), a/*oxygen of the open C-term*/}};
  } else if (atomCnext->isBonds(C,1, O,1, N,1)) {
    return {{atomN, atomN->findFirstBond(H), atomCnext->filterBonds1(N) /*nitrogen in the next AA in the chain*/}};
  }
  unreachable(); // no other situation is possible
}

std::array<Atom*,5> Molecule::findAaCterm() { // expects that the molecule has an open C-term in the end
  auto atomC = findLast(C); // heuristic step
  assert(atomC->isBonds(C,1, O,2));
  auto atomCprime = atomC->filterBonds1(C);
  auto atomN = atomCprime->filterBonds1(N);
  auto atomCoo = atomC->filterBonds(O);
  // returns: {C-term, Oxygen with double-link, Oxygen in OH, H in OH, N}
  if (atomCoo[0]->isBonds(C,1, H,1))
    return {{atomC, atomCoo[1], atomCoo[0], atomCoo[0]->findFirstBond(H), atomN}};
  else
    return {{atomC, atomCoo[0], atomCoo[1], atomCoo[1]->findFirstBond(H), atomN}};
}

std::vector<Atom*> Molecule::findAaLast() {
  assert(dbgIsAllocated(this));
  auto cterm = findAaCterm();
  auto atomN = cterm[4];
  auto found = std::find(atoms.rbegin(), atoms.rend(), atomN);
  assert(found != atoms.rend());
  std::vector<Atom*> res;
  while (true) {
    res.push_back(*found);
    if (found == atoms.rbegin())
      break;
    found--;
  }
  return res;
}

void Molecule::detectBonds() {
  // clear previous bonds
  for (auto a : atoms)
    a->bonds.clear();
  // build bonds
  for (unsigned i1 = 0, ie = atoms.size(); i1 < ie; i1++) {
    auto a1 = atoms[i1];
    for (unsigned i2 = i1+1; i2 < ie; i2++) {
      auto a2 = atoms[i2];
      if (a1->isBond(*a2)) {
        a1->link(a2);
        //if (a1->elt == "N" || a2->elt == "N")
        //std::cout << "BOND--> dist=" << (a1->pos-a2->pos).len() << ' ' << *a1 << " -> " << *a2 << std::endl;
      } else {
        //std::cout << ".. NO BOND--> dist=" << (a1->pos-a2->pos).len() << ' ' << *a1 << " -> " << *a2 << std::endl;
      }
    }
  }
}

void Molecule::appendAsAminoAcidChain(Molecule &aa) { // ASSUME that aa is an amino acid XXX alters aa
  auto &me = *this;
  // find C/N terms
  auto meCterm = me.findAaCterm();
  auto aaNterm = aa.findAaNterm();
  // center them both at the junction point
  me.centerAt(meCterm[2/*O that will be deleted*/]->pos);
  aa.centerAt(aaNterm[0/*N that is kept*/]->pos);
  if (true) { // rotate
    // determine directions along the AAs
    auto meAlong = (meCterm[2/*oxygen from OH-group*/]->pos - meCterm[4/*N*/]->pos).normalize();
    auto aaAlong = (aaNterm[2/*oxygen from OH-group*/]->pos - aaNterm[0/*N*/]->pos).normalize();
    auto aaCterm = aa.findAaCterm();
    auto meDblO = (meCterm[1]->pos - meCterm[0]->pos).orthogonal(meAlong).normalize();
    auto aaDblO = (aaCterm[1]->pos - aaCterm[0]->pos).orthogonal(aaAlong).normalize();
    assert(meDblO.isOrthogonal(meAlong));
    assert(aaDblO.isOrthogonal(aaAlong));
    aa.applyMatrix(Vec3Extra::rotateCornerToCorner(meAlong,meDblO, aaAlong,-aaDblO));
    assert((meCterm[2/*oxygen from OH-group*/]->pos - meCterm[4/*N*/]->pos).isParallel((aaNterm[2/*oxygen from OH-group*/]->pos - aaNterm[0/*N*/]->pos)));
    //std::cout << "Molecule::appendAsAminoAcidChain >>>" << std::endl;
    //std::cout << "Molecule::appendAsAminoAcidChain: meAlong=" << (meCterm[2/*oxygen from OH-group*/]->pos - meCterm[4/*N*/]->pos).normalize() << std::endl;
    //std::cout << "Molecule::appendAsAminoAcidChain: aaAlong=" << (aaNterm[2/*oxygen from OH-group*/]->pos - aaNterm[0/*N*/]->pos).normalize() << std::endl;
    //std::cout << "Molecule::appendAsAminoAcidChain: OLD(aa) atomN={" << *aaNterm[0/*N*/] << "} atomO={" << *aaNterm[2/*oxygen from OH-group*/] << "}" << std::endl;
  }
  // remove atoms
  me.removeAtEnd(meCterm[2/*O*/]);
  me.removeAtEnd(meCterm[3/*H*/]);
  aa.removeAtBegin(aaNterm[1]);
  // append
  add(aa);
  auto meCtermNew = me.findAaCterm();
  //std::cout << "Molecule::appendAsAminoAcidChain: meAlongNew=" << (meCtermNew[2/*oxygen from OH-group*/]->pos - meCtermNew[4/*N*/]->pos).normalize() << std::endl;
  //std::cout << "Molecule::appendAsAminoAcidChain: NEW atomN={" << *meCtermNew[4/*N*/] << "} atomO={" << *meCtermNew[2/*oxygen from OH-group*/] << "}" << std::endl;
  //std::cout << "Molecule::appendAsAminoAcidChain <<<" << std::endl;
}

Molecule* Molecule::readXyzFile(const std::string &fname) {
  std::ifstream file(fname, std::ios::in);
  if (!file.is_open())
    throw Exception(str(boost::format("can't open the xyz file: %1%") % fname));

  unsigned natoms = 0;
  std::string dummy;
  Molecule *m = nullptr;

  try {
    char phase = 'N'; // number
    std::string line;
    while (std::getline(file, line)) {
      switch (phase) {
      case 'N': {
        std::istringstream is(line);
        if (!(is >> natoms))
          throw Exception("no natoms in file");
        if (is >> dummy)
          throw Exception(str(boost::format("garbage in the end of the line: %1%") % dummy));
        phase = 'D'; // description
        break;
      } case 'D': {
        m = new Molecule(line);
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
          throw Exception(str(boost::format("no atom description found in line: %1%") % line));
        // atom line can also have a gradient in it
        unsigned ndummy = 0;
        while (is >> dummy)
          ndummy++;
        if (ndummy != 0 && ndummy != 3)
          throw Exception(str(boost::format("the atom descriptor can have either 4 or 7 elements, found %1% elements in line %2%") % ndummy % line));
        if (--natoms == 0)
          phase = 'E'; // end
        break;
      } case 'E': {
        std::istringstream is(line);
        if (is >> dummy)
          throw Exception(str(boost::format("trailing characters after the atom data: %1%") % line));
        break;
      } default:
        unreachable();
      }
    }
  } catch (const Exception &e) {
    if (m)
      delete m;
    throw e;
  }

  // detect bonds
  m->detectBonds();

  return m;
}

void Molecule::writeXyzFile(const std::string &fname) const {
  std::ofstream file(fname, std::ios::out);
  file << *this;
}

std::string Molecule::toString() const {
  std::ostringstream ss;
  ss << *this;
  return ss.str();
}

std::ostream& operator<<(std::ostream &os, const Molecule &m) {
  os << m.atoms.size() << std::endl;
  os << m.descr << std::endl;
  for (auto &a : m.atoms)
    os << *a << std::endl;
  return os;
}

