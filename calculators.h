#pragma once

#include "common.h"
#include <map>
class Molecule;

namespace Calculators {

typedef std::map<std::string,std::string> Params;

class Engine {
public:
  virtual ~Engine() { }
  static Engine* create(const std::string &kind); // returns nullptr when this kind doesn't exist
  // overridables
  virtual const char* kind() const = 0;
  virtual Float calcEnergy(const Molecule &m, const Params &params) = 0;
  virtual Molecule* calcOptimized(const Molecule &m, const Params &params) = 0;
}; // Engine

namespace engines {

class Erkale : public Engine {
  unsigned num;
public:
  Erkale() : num(0) { }
  const char* kind() const;
  Float calcEnergy(const Molecule &m, const Params &params);
  Molecule* calcOptimized(const Molecule &m, const Params &params);
}; // Erkale

}; // engines

}; // Calculators

typedef Calculators::Engine CalcEngine;
