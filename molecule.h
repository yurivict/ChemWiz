#pragma once

#include "common.h"
#include "xerror.h"
#include "obj.h"
#include "Vec3.h"
#include "Mat3.h"

#include <boost/format.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <array>
#include <vector>
#include <set>

enum Element {
  H  = 1,
  He = 2,
  Li = 3,
  Be = 4,
  B  = 5,
  C  = 6,
  N  = 7,
  O  = 8,
  F  = 9,
  Ne = 10,
  Na = 11,
  Mg = 12,
  Al = 13,
  Si = 14,
  P  = 15,
  S  = 16,
  Cl = 17,
  Ar = 18,
  K  = 19,
  Ca = 20,
  Sc = 21,
  Ti = 22,
  V  = 23,
  Cr = 24,
  Mn = 25,
  Fe = 26,
  Co = 27,
  Ni = 28,
  Cu = 29,
  Zn = 30,
  Ga = 31,
  Ge = 32,
  As = 33,
  Se = 34,
  Br = 35,
  Kr = 36,
  I  = 53,
  Xe = 54
};

std::ostream& operator<<(std::ostream &os, Element e);
std::istream& operator>>(std::istream &is, Element &e);

class Molecule;

// define SecondaryStructureKind values to be the same as in the secStructList of MMTF because for now they mostly come from there
enum SecondaryStructureKind {Undefined = -1, PiHelix = 0, Bend = 1, AlphaHelix = 2, Extended = 3, Helix3_10 = 4, Bridge = 5, Turn = 6, Coil = 7};
std::ostream& operator<<(std::ostream &os, const SecondaryStructureKind &secStr);

class Atom : public Obj {
public:
  Molecule          *molecule; // Atom can only belong to one molecule
  Element            elt;
  Vec3               pos;
  bool               isHetAtm; // what exactly is this?
  std::string        name;     // atom can be given a name, see group.atomNameList[] in MMTF spec
  std::vector<Atom*> bonds;    // all bonds are listed
  unsigned           chain;    // chain number, if available
  unsigned           group;    // group number, if available
  SecondaryStructureKind secStructKind; // defined per-group, but we don't have group objects
  void              *obj;
  Atom(Element newElt, const Vec3 &newPos) : molecule(nullptr), elt(newElt), pos(newPos), isHetAtm(false), chain(0), group(0), secStructKind(Undefined), obj(nullptr) {
    //std::cout << "Atom::Atom " << this << std::endl;
  }
  Atom(const Atom &other)
  : molecule(nullptr), elt(other.elt), pos(other.pos), isHetAtm(other.isHetAtm), chain(other.chain), group(other.group), secStructKind(other.secStructKind), obj(nullptr) { // all but bonds and obj
    //std::cout << "Atom::Atom(copy) " << this << std::endl;
  }
  ~Atom() {
    //std::cout << "Atom::~Atom " << this << std::endl;
  }
  Atom transform(const Vec3 &shft, const Vec3 &rot) const {
    return Atom(elt, Mat3::rotate(rot)*pos + shft);
  }
  Atom* setMolecule(Molecule *m) {molecule = m; return this;}
  bool isEqual(const Atom &other) const; // compares if the data is exactly the same
  unsigned nbonds() const {return bonds.size();}
  bool hasBond(const Atom *other) const;
  static Float atomBondAvgRadius(Element elt) {
    // based on the paper Raji Heyrovska "Atomic Structures of all the Twenty Essential Amino Acids and a Tripeptide, with Bond Lengths as Sums of Atomic Covalent Radii"
    // tolerances will be added to account for ranges
    if (elt == H)  return 0.37;
    if (elt == C)  return 0.70;
    if (elt == N)  return 0.66;
    if (elt == O)  return 0.63;
    if (elt == F)  return 0.70; // ad-hoc wrong
    if (elt == S)  return 1.04;
    if (elt == Cl) return 1.04; // ad-hoc wrong
    if (elt == Br) return 1.04; // ad-hoc wrong
    if (elt == I)  return 1.04; // ad-hoc wrong
    ERROR(str(boost::format("atomBondAvgRadius: unknown element %1%") % elt));
  }
  static Float atomBondAvgDistance(Element elt1, Element elt2) {
    // based on the same paper of Raji Heyrovska
    if (elt1 == H && elt2 == H)
      return 2*0.37;
    return atomBondAvgRadius(elt1) + atomBondAvgRadius(elt2);
  }
  bool isBond(const Atom &a) const {
    auto distActual = (pos - a.pos).len();
    auto distAverage = atomBondAvgDistance(elt, a.elt);
    const static Float tolerance = 0.2;
    //assert(distActual > distAverage - tolerance); // needs to be larger than this threshold, otherwise this molecule is invalid
    if (distActual <= distAverage - tolerance)
      warning("distance between atoms " << elt << "/" << a.elt << " is too low: dist=" << distActual << " avg=" << distAverage << " tolerance=" << tolerance)
    return distActual < distAverage + tolerance;
  }
  void link(Atom *othr) {
    this->addToBonds(othr);
    othr->addToBonds(this);
  }
  void addToBonds(Atom *a) {
    bonds.push_back(a);
  }
  void removeFromBonds(Atom *a) {
    for (auto i = bonds.begin(), ie = bonds.end(); i != ie; i++)
      if (*i == a) {
        bonds.erase(i);
        return;
      }
    unreachable();
  }
  void unlink(Atom *othr) {
    this->removeFromBonds(othr);
    othr->removeFromBonds(this);
  }
  void applyMatrix(const Mat3 &m) {
    pos = m*pos;
  }
  Atom* getOtherBondOf3(Atom *othr1, Atom *othr2) const;
  Atom* findOnlyC() const;
  Atom* findSingleNeighbor(Element elt) const;
  Atom* findSingleNeighbor2(Element elt1, Element elt2) const;
  auto findFirstBond(Element bondElt) {
    for (auto a : bonds)
      if (a->elt == bondElt)
        return a;
    unreachable();
  }
  bool isBonds(Element E, unsigned cnt) const {
    unsigned cnt1 = 0, cntOther = 0;
    for (auto a : bonds)
      if (a->elt == E)
        cnt1++;
      else
        cntOther++;
    return cnt1 == cnt && cntOther == 0;
  }
  bool isBonds(Element E1, unsigned cnt1, Element E2, unsigned cnt2) const {
    unsigned cnt[2] = {0,0}, cntOther = 0;
    for (auto a : bonds)
      if (a->elt == E1)
        cnt[0]++;
      else if (a->elt == E2)
        cnt[1]++;
      else
        cntOther++;
    return cnt[0] == cnt1 && cnt[1] == cnt2 && cntOther == 0;
  }
  bool isBonds(Element E1, unsigned cnt1, Element E2, unsigned cnt2, Element E3, unsigned cnt3) const {
    unsigned cnt[3] = {0,0,0}, cntOther = 0;
    for (auto a : bonds)
      if (a->elt == E1)
        cnt[0]++;
      else if (a->elt == E2)
        cnt[1]++;
      else if (a->elt == E3)
        cnt[2]++;
      else
        cntOther++;
    return cnt[0] == cnt1 && cnt[1] == cnt2 && cnt[2] == cnt3 && cntOther == 0;
  }
  std::vector<Atom*> filterBonds(Element bondElt) const {
    std::vector<Atom*> res;
    for (auto a : bonds)
      if (a->elt == bondElt)
        res.push_back(a);
    return res;
  }
  Atom* filterBonds1(Element bondElt) const {
    Atom* res = nullptr;
    for (auto a : bonds)
      if (a->elt == bondElt) {
        if (res)
          ERROR(str(boost::format("filterBonds1: duplicate %1%->%2% bond when only one is expected") % elt % bondElt));
        res = a;
      }
    if (!res)
      ERROR(str(boost::format("filterBonds1: no %1%->%2% bond found when one is expected") % elt % bondElt));
    return res;
  }
  void centerAt(const Vec3 &pt) {
    pos -= pt;
  }
  void snapToGrid(const Vec3 &grid);
  // printing
  friend std::ostream& operator<<(std::ostream &os, const Atom &a);
}; // Atom

class Molecule : public Obj {
public:
  std::string        idx;
  std::string        descr;
  std::vector<Atom*> atoms; // own atoms here
  unsigned           nChains; // zero when the object doesn't represent chains
  unsigned           nGroups; // zero when the object doesn't have groups defined
  Molecule(const std::string &newDescr);
  Molecule(const Molecule &other);
  ~Molecule();
  void setDescr(const std::string &newDescr) {descr = newDescr;}
  void setIdx(const std::string &newIdx) {idx = newIdx;}
  unsigned numAtoms() const {return atoms.size();}
  void add(const Atom &a) { // doesn't detect bonds when one atom is added
    atoms.push_back((new Atom(a))->setMolecule(this));
  }
  void add(Atom *a) { // doesn't detect bonds when one atom is added // pass ownership of the object
    atoms.push_back(a->setMolecule(this));
  }
  void add(const Molecule &m) {
    for (auto a : m.atoms)
      atoms.push_back((new Atom(*a))->setMolecule(this));
    detectBonds();
  }
  void add(const Molecule &m, const Vec3 &shft, const Vec3 &rot) { // shift and rotation (normalized)
    for (auto a : m.atoms)
      atoms.push_back((new Atom(a->transform(shft, rot)))->setMolecule(this));
    detectBonds();
  }
  unsigned getNumAtoms() const {return atoms.size();}
  Atom* getAtom(unsigned idx) const {return atoms[idx];}
  void applyMatrix(const Mat3 &m) {
    for (auto a : atoms)
      a->applyMatrix(m);
  }
  unsigned numChains() const {return nChains;}
  unsigned numGroups() const {return nGroups;}
  std::vector<std::vector<Atom*>> findComponents() const;
  std::vector<Vec3> computeConvexHullFacets(std::vector<double> *withFurthestdist) const; // in molecule-qhull.cpp
  Atom* findFirst(Element elt) {
    for (auto a : atoms)
      if (a->elt == elt)
        return a;
    return nullptr;
  }
  Atom* findLast(Element elt) {
    for (auto i = atoms.rbegin(), ie = atoms.rend(); i != ie; i++)
      if ((*i)->elt == elt)
        return *i;
    return nullptr;
  }
  std::array<Atom*,3> findAaNterm();
  std::array<Atom*,5> findAaCterm();
  std::vector<Atom*> findAaLast();
  void detectBonds();
  bool isEqual(const Molecule &other) const; // compares if the data is exactly the same (including the order of atoms)
  // high-level append
  void appendAsAminoAcidChain(Molecule &aa); // ASSUME that aa is an amino acid XXX alters aa
  // remove
  void removeAtBegin(Atom *a) {
    for (auto i = atoms.begin(), ie = atoms.end(); i != ie; i++)
      if ((*i) == a) {
        while (!a->bonds.empty())
          a->unlink(a->bonds[0]);
        atoms.erase(i);
        delete a;
        return;
      }
    unreachable();
  }
  void removeAtEnd(Atom *a) {
    auto idx = atoms.size();
    for (auto i = atoms.rbegin(), ie = atoms.rend(); i != ie; i++, idx--)
      if ((*i) == a) {
        while (!a->bonds.empty())
          a->unlink(a->bonds[0]);
        atoms.erase(atoms.begin() + (idx-1));
        delete a;
        return;
      }
    unreachable();
  }
  void centerAt(const Vec3 pt) { // no reference (!)
    for (auto a : atoms)
      a->centerAt(pt);
  }
  std::string toString() const;
  // read external formats
  static Molecule* readXyzFileOne(const std::string &fname); // expects one xyz record
  static std::vector<Molecule*> readXyzFileMany(const std::string &fname); // expects 1+ xyz records
  static std::vector<Molecule*> readPdbFile(const std::string &newFname); // using dsrpdb
  static std::vector<Molecule*> readPdbBuffer(const std::string &pdbBuffer); // using our parser
  static std::vector<Molecule*> readMmtfFile(const std::string &fname);
  static std::vector<Molecule*> readMmtfBuffer(const std::vector<uint8_t> *buffer);
#if defined(USE_OPENBABEL)
  static Molecule* createFromSMILES(const std::string &smiles, const std::string &opt);
#endif
  // write external formats
  void writeXyzFile(const std::string &fname) const;
  // printing
  void prnCoords(std::ostream &os) const;
  Vec3 centerOfMass() const;
  void snapToGrid(const Vec3 &grid);
  friend std::ostream& operator<<(std::ostream &os, const Molecule &m);
  friend std::istream& operator>>(std::istream &is, Molecule &m); // Xyz reader
}; // Molecule

