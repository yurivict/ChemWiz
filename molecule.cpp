#include "common.h"
#include "molecule.h"
#include "xerror.h"
#include "Vec3.h"
#include "Vec3-ext.h"
#include "Mat3.h"
#include "periodic-table-data.h"

#include <stdio.h>

#include <map>
#include <set>
#include <list>
#include <array>
#include <ostream>


/// Element

std::ostream& operator<<(std::ostream &os, Element e) {
  os << PeriodicTableData::get()(e).symbol;
  return os;
}

std::istream& operator>>(std::istream &is, Element &e) {
  std::string s;
  is >> s;
  e = (Element)PeriodicTableData::get().elementFromSymbol(s);
  return is;
}

/// Atom

std::ostream& operator<<(std::ostream &os, const SecondaryStructureKind &secStr) {
  switch (secStr) {
  case PiHelix:    os << "HELIX";        break;
  case Bend:       os << "BEND";         break;
  case AlphaHelix: os << "ALPHA_HELIX";  break;
  case Extended:   os << "EXTENDED";     break;
  case Helix3_10:  os << "HELIX_3_10";   break;
  case Bridge:     os << "BRIDGE";       break;
  case Turn:       os << "TURN";         break;
  case Coil:       os << "COIL";         break;
  case Undefined:  os << "UNDEFINED";    break;
  default:         assert(0); // bogus value
  }
  return os;
}

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

void Atom::snapToGrid(const Vec3 &grid) {
  pos.snapToGrid(grid);
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

Molecule::Molecule(const std::string &newDescr)
: descr(newDescr),
  nChains(0),
  nGroups(0)
{
  //std::cout << "Molecule::Molecule(copy) " << this << std::endl;
}

Molecule::Molecule(const Molecule &other)
: descr(other.descr),
  nChains(other.nChains),
  nGroups(other.nGroups)
{
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

std::vector<std::vector<Atom*>> Molecule::findComponents() const {
  std::vector<std::vector<Atom*>> res;
  std::set<const Atom*> seen;
  auto already = [&seen](const Atom *atom) {return seen.find(atom) != seen.end();};
  for (auto a : atoms)
    if (!already(a)) {
      // new component
      res.resize(res.size()+1);
      auto &curr = *res.rbegin();
      std::list<const Atom*> todo;
      todo.push_back(a);
      seen.insert(a);
      curr.push_back(a);
      while (!todo.empty()) {
        auto at = *todo.begin();
        todo.pop_front();
        // iterate through the bonds
        for (auto ab : at->bonds)
          if (!already(ab)) {
            todo.push_back(ab);
            seen.insert(ab);
            curr.push_back(ab);
          }
      }
    }

  return res;
}

Molecule::AaCore Molecule::findAaCore1() { // finds a single AaCore, fails when AA core is missing or it has multiple AA cores
  AaCore aaCore;
  bool   aaCoreFound = false;
  // begin from O2, use it as an anchor because it is always present
  for (auto aO2 : atoms)
    if (aO2->elt == O && aO2->isBonds(C, 1)) {
      bool aaCoreFoundNew = findAaCore(aO2, aaCore);
      if (aaCoreFoundNew && aaCoreFound)
        ERROR("multiple AA cores in a molecule when findAaCore1() expects only one core")

      aaCoreFound = aaCoreFoundNew;
    }
  return aaCoreFound ? aaCore : AaCore({});
}

std::vector<Molecule::AaCore> Molecule::findAaCores() {  // finds all AA cores
  std::vector<AaCore> aaCores;
  AaCore aaCore;
  // begin from O2, use it as an anchor because it is always present
  for (auto aO2 : atoms)
    if (aO2->elt == O && aO2->isBonds(C, 1))
      if (findAaCore(aO2, aaCore))
        aaCores.push_back(aaCore);
  return aaCores;
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

Vec3 Molecule::centerOfMass() const {
  const PeriodicTableData &ptd = PeriodicTableData::get();
  Vec3 m(0,0,0);
  double totalMass = 0;
  for (auto a : atoms) {
    auto M = ptd((unsigned)a->elt).atomic_mass;
    m += a->pos*M;
    totalMass += M;
  }

  return m/totalMass;
}

void Molecule::snapToGrid(const Vec3 &grid) {
  for (auto a : atoms)
    a->snapToGrid(grid);
}

bool Molecule::findAaCore(Atom *O2anchor, AaCore &aaCore) {
  // TODO Proline is different - the group includes a 5-cycle
  // begin from O2, use it as an anchor because it is always present
  auto aCoo = O2anchor->bonds[0];
  Atom *aO1 = nullptr;
  Atom *aHo = nullptr;
  if (aCoo->isBonds(O,2, C,1)) { // carbon end it free: O2 + OH (free tail) + C
    aO1 = aCoo->filterBonds1excl(O, O2anchor);
    if (aO1->isBonds(C,1, H,1))
      aHo = aO1->filterBonds1(H);
    else
      return false; // wrong aO1 connections
  } else if (!aCoo->isBonds(O,1, C,1, N,1)) // carbon end is connected: O2 + N (connected) + C
    return false; // not an AA oxygen
  Atom *aCmain = aCoo->filterBonds1(C);
  if (!(aCmain->isBonds(C,2, N,1, H,1) || aCmain->isBonds(C,1, N,1, H,2))) // the first payload atom is either C, or H (only in Glycine)
    return false; // wrong aCmain bonds
  Atom *aHc = aCmain->filterBonds(H)[0]; // could be 1 or two hydrogens
  Atom *aN = aCmain->filterBonds1(N);
  Atom* aHn1;
  Atom* aHn2 = nullptr;
  if (aN->isBonds(C,1, H,2)) { // tail N has 2 hydrogens
    auto aaHn2 = aN->filterBonds2(H);
    aHn1 = aaHn2[0];
    aHn2 = aaHn2[1];
  } else if (aN->isBonds(C,2, H,1)) { // connected N
    aHn1 = aN->filterBonds1(H);
  } else
    return false; // wrong aN bonds

  Atom *aPayload = nullptr;
  for (auto a : aCmain->bonds)
    if (a != aN && a != aCoo && a != aHc) {
      aPayload = a;
      break;
    }
  assert(aPayload);

  // found
  aaCore = {aN, aHn1, aHn2, aCmain, aHc, aCoo, O2anchor, aO1, aHo, aPayload};
  return true;
}

std::ostream& operator<<(std::ostream &os, const Molecule &m) {
  os << m.atoms.size() << std::endl;
  os << m.descr << std::endl;
  m.prnCoords(os);
  return os;
}

