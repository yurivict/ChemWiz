#include "molecule.h"

#include <openbabel/mol.h>
#include <openbabel/atom.h>
#include <openbabel/obconversion.h>
#include <openbabel/op.h>

using namespace OpenBabel;

Molecule* Molecule::createFromSMILES(const std::string &smiles, const std::string &opt) {
  // convertor
  OBConversion obConv;
  obConv.SetInFormat("smi");

  // molecule
  OBMol obMol;

  // read smiles
  obConv.ReadString(&obMol, smiles);

  // gen3D plugin
  OBOp* pgen = OBOp::FindType("gen3D");
  pgen->Do(&obMol, opt.c_str());

  // our molecule
  auto m = new Molecule("from smiles via babel");

  // read molecule into out Molecule class
  for (auto ita = obMol.BeginAtoms(); ita != obMol.EndAtoms(); ++ita) {
    OBAtom *a = *ita;
    m->add(Atom(Element(a->GetAtomicNum()), Vec3(a->GetX(), a->GetY(), a->GetZ())));
  }

  //
  m->detectBonds(); // TODO read bonds from OpenBabel structures
  return m;
}

