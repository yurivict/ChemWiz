#include "structure-db.h"

#include <algorithm>

StructureDb::StructureDb() {
}

void StructureDb::add(const Molecule *m, const std::string &id) {
  const MoleculeSignature signature = computeMoleculeSignature(m);
  signatures[signature] = id;
}

std::string StructureDb::find(const Molecule *m) {
  auto it = signatures.find(computeMoleculeSignature(m));
  return it != signatures.end() ? it->second : "";
}

/// internals

const StructureDb::MoleculeSignature StructureDb::computeMoleculeSignature(const Molecule *m) {
  MoleculeSignature moleculeSignature;
  for (auto a : m->atoms)
    moleculeSignature.push_back(computeAtomSignature(a));
  moleculeSignature.sort();
  return moleculeSignature;
}

const StructureDb::AtomSignature StructureDb::computeAtomSignature(const Atom *a) {
  // top-level atom defines a stack that neighbors are checked against
  std::array<const Atom*, STACK_SZ> stack;
  stack[0] = a;
  // similar to computeAtomNeighborSignature here
  AtomSignature atomSignature(a->elt);
  for (auto neighbor : a->bonds)
    atomSignature.addNeighbor(computeAtomNeighborSignature(neighbor, stack, 1/*current depth*/, STACK_SZ));
  return atomSignature;
}

const StructureDb::AtomSignature StructureDb::computeAtomNeighborSignature(const Atom *a, std::array<const Atom*, STACK_SZ> &stack, unsigned depth, unsigned maxDepth) {
  AtomSignature atomSignature(a->elt);
  if (depth < maxDepth) {
    stack[depth] = a;
    for (auto neighbor : a->bonds) {
      // exclude neighbors that we've seen in stack
      bool seen = false;
      for (unsigned d = 0; d < depth; d++)
        if (neighbor == stack[d]) {
          seen = true;
          break;
        }
      if (seen)
        continue;
      atomSignature.addNeighbor(computeAtomNeighborSignature(neighbor, stack, depth+1, STACK_SZ));
    }
  }
  return atomSignature;
}

/// StructureDb::MoleculeSignature methods

bool StructureDb::MoleculeSignature::operator<(const StructureDb::MoleculeSignature &other) const {
  if (size() < other.size())
    return true;
  else if (size() > other.size())
    return false;
  return false; // equal?
}

void StructureDb::MoleculeSignature::sort() {
  // sort individual AtomSignatures
  for (auto &as : *this)
    as.sort();
  // sort the array
  std::sort(begin(), end());
}

std::ostream& operator<<(std::ostream &os, const StructureDb::MoleculeSignature &ms) {
  os << "[" << ms[0];
  for (unsigned i = 1; i < ms.size(); i++)
    os << "," << ms[i];
  os << "]";
  return os;
}

/// StructureDb::AtomSignature methods

bool StructureDb::AtomSignature::operator<(const StructureDb::AtomSignature &other) const {
  if (elt < other.elt)
    return true;
  else if (elt > other.elt)
    return false;
  if (neighbors.size() < other.neighbors.size())
    return true;
  else if (neighbors.size() > other.neighbors.size())
    return false;
  for (unsigned i = 0; i < neighbors.size(); i++)
    if (neighbors[i] < other.neighbors[i])
      return true;
    else if (other.neighbors[i] < neighbors[i])
      return false;
  return false; // equal?
}

void StructureDb::AtomSignature::sort() {
  std::sort(neighbors.begin(), neighbors.end());
}

std::ostream& operator<<(std::ostream &os, const StructureDb::AtomSignature &as) {
  os << "{elt: \"" << as.elt << "\"";
  if (!as.neighbors.empty()) {
    os << ", n: [" << as.neighbors[0];
    for (unsigned i = 1; i < as.neighbors.size(); i++)
      os << "," << as.neighbors[i];
    os << "]";
  }
  os << "}";
  return os;
}

