#pragma once

#include <string>
#include <vector>

// data parsed in from contrib/Periodic-Table-JSON/PeriodicTableJSON.json

class PeriodicTableData {
public: // data
  struct ElementData { // field names are the same as json keys in contrib/Periodic-Table-JSON/PeriodicTableJSON.json
    std::string    name;
    std::string    appearance;
    double         atomic_mass;
    double         boil;
    std::string    category;
  }; // ElementData
private: // data
  std::vector<ElementData> data; // by element
public: // iface
  PeriodicTableData();
  static const PeriodicTableData& get();
}; // PeriodicTableData
