
#include "molecule.h"
#include "xerror.h"
#include "periodic-table-data.h"

#include <string>
#include <memory>

#include <mmtf.hpp>


/// Molecule: MMTF chemical structure format reader

static std::vector<Molecule*> readMolecule(const mmtf::StructureData &sd) {
  std::vector<Molecule*> res;

  assert(sd.secStructList.empty() || (int)sd.secStructList.size() == sd.numGroups);

  auto &ptd = PeriodicTableData::get();

  if (!sd.hasConsistentData(true))
    ERROR("MMTF doesn't have consisent data")

  int modelIndex = 0;
  int chainIndex = 0; // through index
  int groupIndex = 0; // through index

  // traverse models
  for (int im = 0; im < sd.numModels; im++, modelIndex++) {
    std::unique_ptr<Molecule> m(new Molecule("")); // molecule per model
    // traverse chains
    for (int ic = 0; ic < sd.chainsPerModel[modelIndex]; ic++, chainIndex++) {
      // increase chain count in the molecule
      m->nChains++;
      // traverse groups
      for (int ig = 0; ig < sd.groupsPerChain[chainIndex]; ig++, groupIndex++) {
        // increase group count in the molecule
        m->nGroups++;
        //
        const mmtf::GroupType& group = sd.groupList[sd.groupTypeList[groupIndex]];
        int groupAtomCount = group.atomNameList.size();
        // traverse ATOMs or HETATMs
        for (int ia = 0; ia < groupAtomCount; ia++) {
          int atomId = sd.atomIdList[ia]; // through index

            // serial is atomIndex+1
            // otherwise, it is sd.atomIdList[atomIndex], XXX how should we use it?
          // Group name
          // TODO save group name: group.groupName has VAL/LEU/etc - AA names

          // Chain
          // TODO save chain info: sd.chainIdList[chainIndex] has "A"/"B"/... or other names

          // Group serial: they are numbered per chain
          // TODO std::cout << "... GRP: @groupIndex=" << groupIndex << " sd.groupIdList[groupIndex]=" << sd.groupIdList[groupIndex] << std::endl;
          // TODO assert(sd.groupIdList[groupIndex] == groupIndex+1); // how to handle weird (non-sequential) GroupIds?
          // x, y, z: use later

          // Occupancy
          // TODO use occupancy: assert(mmtf::isDefaultValue(sd.occupancyList)); // use sd.occupancyList[atomIndex] if this fails, see PDB conversion example in the MMTF project how

          // create the atom object
          std::unique_ptr<Atom> a(new Atom(Element(ptd.elementFromSymbol(group.elementList[ia])), Vec3(sd.xCoordList[atomId-1], sd.yCoordList[atomId-1], sd.zCoordList[atomId-1])));
          a->name = group.atomNameList[ia]; // meaningful name as related to the structure it is involved in
          if (mmtf::is_hetatm(group.chemCompType.c_str()))
            a->isHetAtm = true;

          // set chain/group/secStructKind
          a->chain = m->nChains;
          a->group = m->nGroups;
          if (!sd.secStructList.empty())
            a->secStructKind = (SecondaryStructureKind)sd.secStructList[a->group - 1];

          // add atom to molecule
          m->add(a.release());
        } // atom
      } // groups
    } // chains
    res.push_back(m.release());
  } // models

  return res;
}

/// iface

std::vector<Molecule*> Molecule::readMmtfFile(const std::string &fname) {
  mmtf::StructureData sd;
  mmtf::decodeFromFile(sd, fname);

  return readMolecule(sd);
}

std::vector<Molecule*> Molecule::readMmtfBuffer(const std::vector<uint8_t> *buffer) {
  mmtf::StructureData sd;
  mmtf::decodeFromBuffer(sd, (const char*)&(*buffer)[0], buffer->size());

  return readMolecule(sd);
}

