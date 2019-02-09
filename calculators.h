#pragma once

#include "common.h"
#include <map>
class Molecule;

namespace Calculators {

typedef std::map<std::string,std::string> Params;

class Engine {
public:
  virtual ~Engine() { }
  // overridables
  virtual Float calcEnergy(const Molecule &m, const Params &params) = 0;
  virtual Molecule* calcOptimized(const Molecule &m, const Params &params) = 0;
}; // Engine

namespace engines {

class Erkale : public Engine {
  unsigned num;
public:
  Erkale() : num(0) { }
  Float calcEnergy(const Molecule &m, const Params &params);
  Molecule* calcOptimized(const Molecule &m, const Params &params);
}; // Erkale

}; // engines

}; // Calculators

typedef Calculators::Engine CalcEngine;
