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
#include <cmath>
#include <algorithm> // for std::find

/// references

// Ramachandran plot references: https://chemistry.stackexchange.com/questions/474/what-are-the-border-definitions-in-the-ramachandran-plot


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

std::set<Atom*> Molecule::listNeighborsHierarchically(Atom *self, bool includeSelf, const Atom *except1, const Atom *except2) { // up to 2 excluded atoms
  std::set<Atom*> lst;

  auto listBonds = [self,except1,except2,&lst](Atom *a, auto &lb) -> void {
    for (auto n : a->bonds)
      if (n != self && n != except1 && n != except2)
        if (lst.find(n) == lst.end()) {
          lst.insert(n);
          lb(n, lb);
        }
  };

  listBonds(self, listBonds);

  if (includeSelf)
    lst.insert(self);

  return lst;
}

void Molecule::appendAsAminoAcidChain(Molecule &aa, const std::vector<Angle> &angles) { // angles are in degrees, -180..+180
  // checks
  if (angles.size() != 3 && angles.size() != 6)
    ERROR("Molecule::appendAsAminoAcidChain: angles argument is expected to have either 3 or 6 angle values, supplied " << angles.size() << " values")
  if (!checkAngleR(angles[0]) || !checkAngleR(angles[1]) || !checkAngleR(angles[2]) ||
      (angles.size() == 6 && (!checkAngleA(angles[3]) || !checkAngleA(angles[4]) || !checkAngleA(angles[5]))))
    ERROR("Molecule::appendAsAminoAcidChain: the supplied angle is out-of-bounds")

  // ASSUME that aa is an amino acid XXX alters aa
  // ASSUME that aa is small because we will translate/rotate it
  auto &me = *this;
  // find C/N terms // XXX some peptides are also defined beginning with C-term for some reason, we need another procedure for that
  auto meAaCoreCterm = me.findAaCoreLast();
  auto aaAaCoreNterm = aa.findAaCoreFirst();
  // local functions in a form of lambda functions
  auto getAtomAxis = [](const Atom *atom1, const Atom *atom2) {
    return (atom2->pos - atom1->pos).normalize();
  };
  auto rotate = [](const Vec3 &center, const Vec3 &axis, double angleD, Molecule &m, const Atom *except) {
    // values
    auto M = Mat3::rotate(axis, Vec3::degToRad(angleD));
    for (auto a : m.atoms)
      if (a != except)
        a->pos = M*(a->pos - center) + center;
  };
  // center them both at the junction point
  me.centerAt(meAaCoreCterm.O1->pos); // XXX should not center it
  aa.centerAt(aaAaCoreNterm.N->pos);
  // rotate aa to (1) align its Oalpha-N axis with me's Oalpha-N axis, (2) make O2 bonds anti-parallel
  { // XXX this might be a wrong way, but we align them such that they are in one line along the AA backbone
    auto meAlong = getAtomAxis(meAaCoreCterm.N, meAaCoreCterm.O1);
    auto aaAlong = getAtomAxis(aaAaCoreNterm.N, aaAaCoreNterm.O1);
    auto aaAaCoreCterm = aa.findAaCoreLast();
    auto meDblO = (meAaCoreCterm.O2->pos - meAaCoreCterm.Coo->pos).orthogonal(meAlong).normalize();
    auto aaDblO = (aaAaCoreCterm.O2->pos - aaAaCoreCterm.Coo->pos).orthogonal(aaAlong).normalize();
    assert(meDblO.isOrthogonal(meAlong));
    assert(aaDblO.isOrthogonal(aaAlong));
    aa.applyMatrix(Vec3Extra::rotateCornerToCorner(meAlong,meDblO, aaAlong,-aaDblO));
    assert((meAaCoreCterm.O1->pos - meAaCoreCterm.N->pos).isParallel((aaAaCoreNterm.O1->pos - aaAaCoreNterm.N->pos)));
  }

  { // apply omega, phi, psi angles
    // compute prior angles
    auto priorOmega = AaAngles::omega(meAaCoreCterm, aaAaCoreNterm);
    auto priorPhi   = AaAngles::phi(meAaCoreCterm, aaAaCoreNterm);
    auto priorPsi   = AaAngles::psi(meAaCoreCterm, aaAaCoreNterm);
    // rotate: begin from the most remote from C-term
    rotate(aaAaCoreNterm.Cmain->pos, priorPsi.axis,   angles[2] - priorPsi.angle,   aa, aaAaCoreNterm.N);
    rotate(aaAaCoreNterm.N->pos,     priorPhi.axis,   angles[1] - priorPhi.angle,   aa, nullptr);
    rotate(meAaCoreCterm.Coo->pos,   priorOmega.axis, angles[0] - priorOmega.angle, aa, nullptr);
  }

  // apply adjacencyN, adjacencyCmain, adjacencyCoo when supplied
  if (angles.size() == 6) {
    auto priorAdjacencyN     = AaAngles::adjacencyN(meAaCoreCterm, aaAaCoreNterm);
    auto priorAdjacencyCmain = AaAngles::adjacencyCmain(meAaCoreCterm, aaAaCoreNterm);
    auto priorAdjacencyCoo   = AaAngles::adjacencyCoo(meAaCoreCterm, aaAaCoreNterm);
    // rotate: begin from the most remote from C-term
    rotate(aaAaCoreNterm.Coo->pos,   priorAdjacencyCoo.axis,   angles[5] - priorAdjacencyCoo.angle,   aa, aaAaCoreNterm.N);
    rotate(aaAaCoreNterm.Cmain->pos, priorAdjacencyCmain.axis, angles[4] - priorAdjacencyCmain.angle, aa, nullptr);
    rotate(aaAaCoreNterm.N->pos,     priorAdjacencyN.axis,     angles[3] - priorAdjacencyN.angle,     aa, nullptr);
  }

  // remove atoms that are excluded by the connection: 1xO and 2xH atoms
  me.removeAtEnd(meAaCoreCterm.O1);
  me.removeAtEnd(meAaCoreCterm.Ho);
  aa.removeAtBegin(aaAaCoreNterm.HCn1->elt == H ? aaAaCoreNterm.HCn1 : aaAaCoreNterm.Hn2); // ad-hoc choice - HCn1 and Hn2 are chosen aritrarily in 'findAaCore'
  // append aa to me
  add(aa);
}

std::vector<std::array<Molecule::Angle,6>> Molecule::readAminoAcidAnglesFromAaChain(const std::vector<AaCore> &aaCores) {
  std::vector<std::array<Angle,6>> angles;
  if (!aaCores.empty())
    angles.reserve(aaCores.size());
  const AaCore *prev = nullptr;
  for (auto &curr : aaCores) {
    if (prev)
      angles.push_back(std::array<Angle,6>{{AaAngles::omega(*prev, curr).angle, AaAngles::phi(*prev, curr).angle, AaAngles::psi(*prev, curr).angle,
                                            AaAngles::adjacencyN(*prev, curr).angle,
                                            AaAngles::adjacencyCmain(*prev, curr).angle,
                                            AaAngles::adjacencyCoo(*prev, curr).angle
                                          }});
    prev = &curr;
  }
  return angles;
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
      aHCn1 = aCproline; // inner C in proline
      aHn2 = aN->filterBonds1(H); // connectable H
      assert(aHn2->elt == H);
    }
  } else if (aN->isBonds(C,3)) { // possibly connected proline
    for (auto a : aN->bonds)
      if (a != aCmain && (aCproline = a->filterBonds1(C)) == aPayload->filterBonds1excl(C, aCmain)) { // is connected proline
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

// internals

bool Molecule::checkAngle(Angle angle, Angle low, Angle high) {
 return low <= angle && angle <= high;
}

bool Molecule::checkAngleR(Angle angle) {
  return checkAngle(angle, -180, +180);
}

bool Molecule::checkAngleA(Angle angle) {
  return checkAngle(angle, 0, +180);
}

std::ostream& operator<<(std::ostream &os, const Molecule &m) {
  os << m.atoms.size() << std::endl;
  os << m.descr << std::endl;
  m.prnCoords(os);
  return os;
}

/// Molecule::AaCore

std::set<Atom*> Molecule::AaCore::listPayload() {
  return Molecule::listNeighborsHierarchically(payload, true/*includeSelf*/, Cmain, N); // except Cmain,N (N is only for Proline)
}

Atom* Molecule::AaCore::nextN() const {
  return O1 ? O1 : Coo->filterBonds1(Element::N);
}

/// Molecule::AaAngles // see https://en.wikipedia.org/wiki/Ramachandran_plot#/media/File:Protein_backbone_PhiPsiOmega_drawing.svg

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::omega(const AaCore &cterm, const AaCore &nterm) {
  return ramachandranFromChain(cterm.Cmain, cterm.Coo, nterm.N, nterm.Cmain);
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::phi(const AaCore &cterm, const AaCore &nterm) {
  return ramachandranFromChain(cterm.Coo, nterm.N, nterm.Cmain, nterm.Coo);
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::psi(const AaCore &cterm, const AaCore &nterm) {
  return ramachandranFromChain(nterm.N, nterm.Cmain, nterm.Coo, nterm.nextN());
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::adjacencyN(const AaCore &cterm, const AaCore &nterm) {
  return adjacencyFromChain(cterm.Coo, nterm.N, nterm.Cmain);
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::adjacencyCmain(const AaCore &cterm, const AaCore &nterm) {
  return adjacencyFromChain(nterm.N, nterm.Cmain, nterm.Coo);
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::adjacencyCoo(const AaCore &cterm, const AaCore &nterm) {
  return adjacencyFromChain(nterm.Cmain, nterm.Coo, nterm.nextN());
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::ramachandranFromChain(const Atom *nearNext, const Atom *near, const Atom *far, const Atom *farNext) {
  auto axis = atomPairToVecN(near, far);
  return {Vec3Extra::angleAxis1x1(axis, atomPairToVec(far, farNext), atomPairToVec(near, nearNext)), axis};
}

Molecule::AaAngles::AngleAndAxis Molecule::AaAngles::adjacencyFromChain(const Atom *prev, const Atom *curr, const Atom *next) {
  auto toPrev = atomPairToVecN(curr, prev);
  auto toNext = atomPairToVecN(curr, next);
  return {Vec3::radToDeg(std::acos(toPrev*toNext)), toPrev.cross(toNext).normalize()};
}

Vec3 Molecule::AaAngles::atomPairToVec(const Atom *a1, const Atom *a2) {
  return (a2->pos - a1->pos);
}

Vec3 Molecule::AaAngles::atomPairToVecN(const Atom *a1, const Atom *a2) { // returns a normalized vector
  return atomPairToVec(a1, a2).normalize();
}

std::ostream& operator<<(std::ostream &os, const Molecule::AaAngles::AngleAndAxis &aa) {
  os << "{";
  os << "angle=" << aa.angle;
  os << " axis=" << aa.axis;
  os << "}";
  return os;
}

