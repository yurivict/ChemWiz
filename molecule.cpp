#include "common.h"
#include "molecule.h"
#include "xerror.h"
#include "Vec3.h"
#include "Vec3-ext.h"
#include "Mat3.h"

#include <stdio.h>

#include <map>
#include <array>
#include <ostream>


/// Element

static const char* eltNames[] = {
  "H", "He",
  "Li", "Be", "B", "C", "N", "O", "F", "Ne",
  "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar",
  "K", "Ca", "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As", "Se", "Br", "Kr"
};

class MapEltName : public std::map<std::string,Element> {
public:
  MapEltName() { // ASSUMES sequentialness of the Element enum
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
    ERROR(str(boost::format("Not an element name: %1%") % s));
  return i->second;
}

/// Atom

bool Atom::isEqual(const Atom &other) const {
  return elt == other.elt &&
         pos == other.pos;
}

bool Atom::hasBond(const Atom *other) const {
  for (auto a : bonds)
    if (a == other)
      return true;
  return false;
}

Atom* Atom::getOtherBondOf3(Atom *othr1, Atom *othr2) const {
  assert(nbonds() == 3);
  if (bonds[0]!=othr1 && bonds[0]!=othr2)
    return bonds[0];
  if (bonds[1]!=othr1 && bonds[1]!=othr2)
    return bonds[1];
  return bonds[2];
}

Atom* Atom::findOnlyC() const {
  Atom *res = nullptr;
  for (auto n : bonds)
    if (n->elt == C) {
      if (!res)
        res = n;
      else
        return nullptr; // not the only one
    }
  return res;
}

Atom* Atom::findSingleNeighbor(Element elt) const {
  Atom *res = nullptr;
  for (auto a : bonds)
    if (a->nbonds() == 1 && a->elt == elt) {
      if (!res)
        res = a;
      else
        return nullptr; // not found
    }
  return res;
}

Atom* Atom::findSingleNeighbor2(Element elt1, Element elt2) const {
  Atom *res = nullptr;
  for (auto a : bonds)
    if (a->nbonds() == 2 && a->elt == elt1) {
      auto nx = a->bonds[0] == this ? a->bonds[1] : a->bonds[0];
      if (nx->nbonds() == 1 && nx->elt == elt2) {
        if (!res)
          res = a;
        else
          return nullptr; // not found
      }
    }
  return res;
}

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

Molecule::Molecule(const std::string &newDescr) : descr(newDescr) {
  //std::cout << "Molecule::Molecule(copy) " << this << std::endl;
}

Molecule::Molecule(const Molecule &other) : descr(other.descr) {
  //std::cout << "Molecule::Molecule() " << this << std::endl;
  for (auto a : other.atoms)
    atoms.push_back((new Atom(*a))->setMolecule(this));
  detectBonds();
}

Molecule::~Molecule() {
  //std::cout << "Molecule::~Molecule " << this << std::endl;
  for (auto a : atoms)
    delete a;
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

bool Molecule::isEqual(const Molecule &other) const {
  // atoms
  if (atoms.size() != other.atoms.size())
    return false;
  for (unsigned i = 0; i < atoms.size(); i++)
    if (!atoms[i]->isEqual(*other.atoms[i]))
      return false;

  return true;
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

std::string Molecule::toString() const {
  std::ostringstream ss;
  ss << *this;
  return ss.str();
}

void Molecule::prnCoords(std::ostream &os) const {
  for (auto &a : atoms)
    os << *a << std::endl;
}

std::ostream& operator<<(std::ostream &os, const Molecule &m) {
  os << m.atoms.size() << std::endl;
  os << m.descr << std::endl;
  m.prnCoords(os);
  return os;
}

