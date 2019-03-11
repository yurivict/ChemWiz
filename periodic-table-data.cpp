
#include "periodic-table-data.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>

#include <stdlib.h>

static PeriodicTableData singleInstance; // statically initialized instance

PeriodicTableData::PeriodicTableData() {
  using json = nlohmann::json;
  std::ifstream file;
  file.open("contrib/Periodic-Table-JSON/PeriodicTableJSON.json", std::ifstream::in);
  if (!file.is_open())
    abort();
  auto parsed = json::parse(file);
  auto elts = parsed["elements"];
  std::cout << "PeriodicTableData::PeriodicTableData nElts=" << elts.size() << std::endl;
  data.resize(1+elts.size());
  unsigned eIdx = 1;
  for (auto e : elts) {
    auto &eData = data[eIdx++];
   eData.name         = e["name"];
   eData.appearance   = e["appearance"];
   eData.atomic_mass  = e["atomic_mass"];
   eData.boil         = e["boil"];
   eData.category     = e["category"];
  }
}

const PeriodicTableData& PeriodicTableData::get() {
  return singleInstance;
}
