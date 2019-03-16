#pragma once

#include <string>
#include <vector>
#include <map>

// data parsed in from contrib/Periodic-Table-JSON/PeriodicTableJSON.json

class PeriodicTableData {
public: // data
  struct ElementData { // field names are the same as json keys in contrib/Periodic-Table-JSON/PeriodicTableJSON.json
    std::string    name;
    std::string    appearance;
    double         atomic_mass;
    double         boil;
    std::string    category;
    std::string    symbol;
  }; // ElementData
  std::map<std::string, unsigned> symToElt;
private: // data
  std::vector<ElementData> data; // by element, this array contains them in sequential order beginning with H at index 0
public: // iface
  PeriodicTableData();
  static const PeriodicTableData& get();
  const ElementData& operator()(unsigned elt) const;
  unsigned elementFromSymbol(const std::string &sym) const;
}; // PeriodicTableData
