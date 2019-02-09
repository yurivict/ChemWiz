
#include <iostream>
#include <fstream>
#include <dsrpdb/PDB.h>
#include <dsrpdb/Model.h>
#include <dsrpdb/Protein.h>

int main() {
  std::ifstream file;
  file.open("1xda.pdb");
  dsrpdb::PDB pdb(file, true);
  std::cout << "number_of_models=" << pdb.number_of_models() << std::endl;

  auto &model = pdb.model(0);
  unsigned residueCount = 0;
  std::cout << "number_of_chains=" << model.number_of_chains() << std::endl;
  for (unsigned c = 0; c < model.number_of_chains(); c++) {
    auto &chain = model.chain(c);
    std::cout << "chain#" << c << ": number_of_residues=" << chain.number_of_residues() << std::endl;
    residueCount += chain.number_of_residues();
  }
  std::cout << "residueCount=" << residueCount << std::endl;
}
