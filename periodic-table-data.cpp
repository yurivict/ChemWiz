
#include <nlohmann/json.hpp>
#include <boost/format.hpp>

#include <fstream>

#include <stdlib.h>

#include "periodic-table-data.h"
#include "xerror.h"

PeriodicTableData PeriodicTableData::singleInstance; // statically initialized instance

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
    eData.symbol       = e["symbol"];
    symToElt[eData.symbol] = eIdx;
  }
}

const PeriodicTableData::ElementData& PeriodicTableData::operator()(unsigned elt) const {
  return data[elt-1];
}

unsigned PeriodicTableData::elementFromSymbol(const std::string &sym) const {
  auto i = symToElt.find(sym);
  if (i == symToElt.end())
    ERROR(str(boost::format("Not an element name: %1%") % sym));
  return i->second;
}
