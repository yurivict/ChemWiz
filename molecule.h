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
  Kr = 36
};

std::ostream& operator<<(std::ostream &os, Element e);
std::istream& operator>>(std::istream &is, Element &e);
Element elementFromString(const std::string &str);

class Molecule;

class Atom : public Obj {
public:
  Molecule          *molecule; // Atom can only belong to one molecule
  Element            elt;
  Vec3               pos;
  bool               isHetAtm; // what exactly is this?
  std::string        name; // atom can be given a name, see group.atomNameList[] in MMTF spec
  std::vector<Atom*> bonds; // all bonds are listed
  void              *obj;
  Atom(Element newElt, const Vec3 &newPos) : molecule(nullptr), elt(newElt), pos(newPos), isHetAtm(false), obj(nullptr) {
    //std::cout << "Atom::Atom " << this << std::endl;
  }
  Atom(const Atom &other) : molecule(nullptr), elt(other.elt), pos(other.pos), isHetAtm(other.isHetAtm), obj(nullptr) { // all but bonds and obj
    //std::cout << "Atom::Atom(copy) " << this << std::endl;
  }
  ~Atom() {
    //std::cout << "Atom::~Atom " << this << std::endl;
  }
  Atom transform(const Vec3 &shft, const Vec3 &rot) const {
    return Atom(elt, Mat3::rotate(rot)*pos + shft);
  }
  Atom* setMolecule(Molecule *m) {molecule = m; return this;}
  unsigned nbonds() const {return bonds.size();}
  bool hasBond(const Atom *other) const;
  static Float atomBondAvgRadius(Element elt) {
    // based on the paper Raji Heyrovska "Atomic Structures of all the Twenty Essential Amino Acids and a Tripeptide, with Bond Lengths as Sums of Atomic Covalent Radii"
    // tolerances will be added to account for ranges
    if (elt == H) return 0.37;
    if (elt == C) return 0.70;
    if (elt == O) return 0.63;
    if (elt == N) return 0.66;
    if (elt == S) return 1.04;
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
  Atom* findOnlyC() const {
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
  // printing
  friend std::ostream& operator<<(std::ostream &os, const Atom &a);
}; // Atom

class Molecule : public Obj {
public:
  std::string        idx;
  std::string        descr;
  std::vector<Atom*> atoms; // own atoms here
  Molecule(const std::string &newDescr);
  Molecule(const Molecule &other);
  ~Molecule();
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
  void applyMatrix(const Mat3 &m) {
    for (auto a : atoms)
      a->applyMatrix(m);
  }
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
  static Molecule* readXyzFile(const std::string &fname);
  static std::vector<Molecule*> readPdbFile(const std::string &newFname);
  static std::vector<Molecule*> readMmtfFile(const std::string &fname);
  static std::vector<Molecule*> readMmtfBuffer(const std::vector<uint8_t> *buffer);
  // write extarnal formats
  void writeXyzFile(const std::string &fname) const;
  // printing
  friend std::ostream& operator<<(std::ostream &os, const Molecule &m);
}; // Molecule

