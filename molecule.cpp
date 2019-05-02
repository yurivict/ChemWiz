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

Molecule::AaCore Molecule::findAaCoreFirst() {
  for (auto aO2 : atoms)
    if (aO2->elt == O && aO2->isBonds(C, 1)) {
      AaCore aaCore;
      if (findAaCore(aO2, aaCore))
        return aaCore;
    }
  ERROR("no AA core found in the molecule when findAaCoreFirst() expects at least one AA core")
}

Molecule::AaCore Molecule::findAaCoreLast() {
  for (auto i = atoms.rbegin(); i != atoms.rend(); i++) {
    auto aO2 = *i;
    if (aO2->elt == O && aO2->isBonds(C, 1)) {
      AaCore aaCore;
      if (findAaCore(aO2, aaCore))
        return aaCore;
    }
  }
  ERROR("no AA core found in the molecule when findAaCoreLast() expects at least one AA core")
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
  auto meAaCoreCterm = me.findAaCoreLast();
  auto aaAaCoreNterm = aa.findAaCoreFirst();
  // center them both at the junction point
  me.centerAt(meAaCoreCterm.O1->pos);
  aa.centerAt(aaAaCoreNterm.N->pos);
  if (true) { // rotate
    // determine directions along the AAs
    auto meAlong = (meAaCoreCterm.O1->pos - meAaCoreCterm.N->pos).normalize();
    auto aaAlong = (aaAaCoreNterm.O1->pos - aaAaCoreNterm.N->pos).normalize();
    auto aaAaCoreCterm = aa.findAaCoreLast();
    auto meDblO = (meAaCoreCterm.O2->pos - meAaCoreCterm.Coo->pos).orthogonal(meAlong).normalize();
    auto aaDblO = (aaAaCoreCterm.O2->pos - aaAaCoreCterm.Coo->pos).orthogonal(aaAlong).normalize();
    assert(meDblO.isOrthogonal(meAlong));
    assert(aaDblO.isOrthogonal(aaAlong));
    aa.applyMatrix(Vec3Extra::rotateCornerToCorner(meAlong,meDblO, aaAlong,-aaDblO));
    assert((meAaCoreCterm.O1->pos - meAaCoreCterm.N->pos).isParallel((aaAaCoreNterm.O1->pos - aaAaCoreNterm.N->pos)));
  }
  // remove atoms
  me.removeAtEnd(meAaCoreCterm.O1);
  me.removeAtEnd(meAaCoreCterm.Ho);
  aa.removeAtBegin(aaAaCoreNterm.HCn1->elt == H ? aaAaCoreNterm.HCn1 : aaAaCoreNterm.Hn2); // ad-hoc choice - HCn1 and Hn2 are chosen aritrarily in 'findAaCore'
  // append
  add(aa);
  //auto meAaCoreCtermNew = me.findAaCoreLast();
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

  // Cmain
  Atom *aCmain = aCoo->filterBonds1(C);
  if (!(aCmain->isBonds(C,2, N,1, H,1) || aCmain->isBonds(C,1, N,1, H,2))) // the first payload atom is either C, or H (only in Glycine)
    return false; // wrong aCmain bonds
  Atom *aHc = aCmain->filterBonds(H)[0]; // could be 1 or two hydrogens
  Atom *aN = aCmain->filterBonds1(N);

  // find the payload atom
  Atom *aPayload = nullptr;
  for (auto a : aCmain->bonds)
    if (a != aN && a != aCoo && a != aHc) {
      aPayload = a;
      break;
    }
  assert(aPayload);

  // what is connected to N?
  Atom* aHCn1;
  Atom* aHn2 = nullptr;
  Atom *aCproline;
  if (aN->isBonds(C,1, H,2)) { // tail N has 2 hydrogens: tail
    auto aaHn2 = aN->filterBonds2(H);
    aHCn1 = aaHn2[0];
    aHn2 = aaHn2[1];
    assert(aHn2->elt == H);
  } else if (aN->isBonds(C,2, H,1)) { // connected N or proline (disconnected)
    aHCn1 = aN->filterBonds1(H); // inner H
    if (aPayload->isBonds(C,2, H,2) && (aCproline = aN->filterBonds1excl(C, aCmain))->isBonds(N,1, C,1, H,2) && aCproline->filterBonds1(C) == aPayload->filterBonds1excl(C, aCmain)) { // is proline
      std::cout << "-- prolin (1)" << std::endl;
      aHCn1 = aCproline; // inner C in proline
      aHn2 = aN->filterBonds1(H); // connectable H
      assert(aHn2->elt == H);
    }
  } else if (aN->isBonds(C,3)) { // possibly connected proline
    for (auto a : aN->bonds)
      if (a != aCmain && (aCproline = a->filterBonds1(C)) == aPayload->filterBonds1excl(C, aCmain)) { // is connected proline
        std::cout << "-- prolin (2)" << std::endl;
        aHCn1 = aCproline; // inner C in proline
        break;
      }
  } else
    return false; // wrong aN bonds

  assert(aN->elt == N);
  assert(aHCn1->elt == C || aHCn1->elt == H);
  assert(!aHn2 || aHn2->elt == H);
  assert(aCmain->elt == C);
  assert(aHc->elt == H);
  assert(aCoo->elt == C);
  assert(O2anchor->elt == O);
  assert(!aO1 || aO1->elt == O);
  assert(!aHo || aHo->elt == H);

  // found
  aaCore = {aN, aHCn1, aHn2, aCmain, aHc, aCoo, O2anchor, aO1, aHo, aPayload};
  return true;
}

std::ostream& operator<<(std::ostream &os, const Molecule &m) {
  os << m.atoms.size() << std::endl;
  os << m.descr << std::endl;
  m.prnCoords(os);
  return os;
}

