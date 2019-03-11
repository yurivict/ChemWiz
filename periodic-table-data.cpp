
#include "periodic-table-data.h"

#include <nlohmann/json.hpp>

#include <fstream>

#include <stdlib.h>

static PeriodicTableData singleInstance; // statically initialized instance

PeriodicTableData::PeriodicTableData() {
  using json = nlohmann::json;
  auto getStr = [](json &j) {return j.is_null() ? "" : j;};
  auto getFlt = [](json &j) -> double {return j.is_null() ? 0. : (double)j;};
  std::ifstream file;
  file.open("contrib/Periodic-Table-JSON/PeriodicTableJSON.json", std::ifstream::in);
  if (!file.is_open())
    abort();
  auto parsed = json::parse(file);
  auto elts = parsed["elements"];
  data.resize(elts.size());
  unsigned eIdx = 0;
  for (auto e : elts) {
    auto &eData = data[eIdx++];
   eData.name         = e["name"];
   eData.appearance   = getStr(e["appearance"]);
   eData.atomic_mass  = e["atomic_mass"];
   eData.boil         = getFlt(e["boil"]);
   eData.category     = e["category"];
  }
}

const PeriodicTableData& PeriodicTableData::get() {
  return singleInstance;
}

const PeriodicTableData::ElementData& PeriodicTableData::operator()(unsigned elt) const {
  return data[elt-1];
}
