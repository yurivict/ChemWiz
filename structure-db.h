#pragma once

#include "molecule.h"

#include <string>
#include <vector>
#include <map>
#include <ostream>

class StructureDb {
  enum {STACK_SZ = 3}; // how far are we looking out
private: // structs
  class AtomSignature;
  class MoleculeSignature : public std::vector<AtomSignature> { // an array of AtomSignature objects
  public:
    bool operator<(const MoleculeSignature &other) const;
    void sort();
    friend std::ostream& operator<<(std::ostream &os, const MoleculeSignature &ms);
  }; // MoleculeSignature
  class AtomSignature {
    Element elt;           // atom element
    std::vector<AtomSignature> neighbors;
  public:
    AtomSignature(Element newElt) : elt(newElt) { }
    void addNeighbor(const AtomSignature &neighbor) {neighbors.push_back(neighbor);}
    bool operator<(const AtomSignature &other) const;
    void sort();
    friend std::ostream& operator<<(std::ostream &os, const AtomSignature &as);
  }; // AtomSignature
private: // data
  std::map<MoleculeSignature, std::string> signatures;
public: // constr/iface
  StructureDb();
  void add(const Molecule *m, const std::string &id);
  std::string find(const Molecule *m);
  size_t size() const {return signatures.size();}
  static const MoleculeSignature computeMoleculeSignature(const Molecule *m);
private: // internals
  static const AtomSignature computeAtomSignature(const Atom *m);
  static const AtomSignature computeAtomNeighborSignature(const Atom *m, std::array<const Atom*, STACK_SZ> &stack, unsigned depth, unsigned maxDepth);
  friend std::ostream& operator<<(std::ostream &os, const MoleculeSignature &ms);
  friend std::ostream& operator<<(std::ostream &os, const AtomSignature &as);
}; // StructureDb
