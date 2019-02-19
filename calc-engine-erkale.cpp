#include "common.h"
#include "calculators.h"
#include "xerror.h"
#include "molecule.h"
#include "util.h"
#include "process.h"
#include <cstring>
#include <sstream>
#include <fstream>
#include <map>

namespace Calculators {
namespace engines {

//
// helpers
//

namespace Helpers {

static std::vector<std::string> runProcess(unsigned num, const Molecule &m, const char *erkale_cmd, const std::map<std::string,std::string> &params) {
  std::ostringstream ssXyzFile;
  Util::OnExit rmXyzFile([&ssXyzFile]() {std::remove(ssXyzFile.str().c_str());});
  ssXyzFile << "tmp-mol-" << num << ".xyz";
  std::ostringstream ssRunfile;
  Util::OnExit rmRunfile([&ssRunfile]() {std::remove(ssRunfile.str().c_str());});
  ssRunfile << "tmp-runfile-" << num << ".run";
  { // write runfile
    std::ofstream fileRunfile(ssRunfile.str(), std::ios::out);
    fileRunfile << "System " << ssXyzFile.str() << std::endl;
    fileRunfile << "Method lda_x-lda_c_vwn" << std::endl;
    // add the supplied params
    for (auto p : params)
      fileRunfile << p.first << " " << p.second << std::endl;
  }
  { // write xyz-file
    std::ofstream fileXyzFile(ssXyzFile.str(), std::ios::out);
    fileXyzFile << m;
  }
  // run the process
  auto lines = Util::splitLines(Process::exec(str(boost::format("/bin/sh -c \"/usr/local/bin/%1% %2% 2>&1\" | tee out") % erkale_cmd % ssRunfile.str())));

  { // check if the error tag has been printed
    const char *errorTag = "error:";
    for (auto &line : lines)
      if (line.size() > std::strlen(errorTag) && line.substr(0, std::strlen(errorTag)) == errorTag)
        ERROR(str(boost::format("Erkale process failed: %1%") % line));
  }

  return lines;
}

static Params paramsToErkaleParams(const Params &params) {
  std::map<std::string,std::string> mres;
  for (auto p : params)
    if (p.first == "precision")
      mres["ConvThr"] = p.second;
    else if (p.first == "basis")
      mres["Basis"] = p.second;
    else
      ERROR(str(boost::format("Unknown parameter supplied to Erkale engine: %1%") % p.first));
  return mres;
}

static void defaultParams(Params &params) {
  if (params.find("Basis") == params.end())
    params["Basis"] = "3-21G";
}

}; // Helpers


//
// implementation functions
//

const char* Erkale::kind() const {
  return "erkale";
}

Float Erkale::calcEnergy(const Molecule &m, const Params &params) {
  num++;

  // params
  auto p = Helpers::paramsToErkaleParams(params);
  Helpers::defaultParams(p);

  // run
  auto res = Helpers::runProcess(num, m, "erkale_omp", p);

  // check output and extract the energy value
  Float energy = 0.;
  { // checks
    if (res.size() < 5)
      ERROR("Erkale::calcEnergy: erkale output is too short");
    // check signature
    static std::string signature = "RDFT converged";
    if ((*res.rbegin()).compare(0, signature.size(), signature) != 0)
      ERROR("Erkale::calcEnergy didn't find the signature in the output");
    // last result line
    auto lineSplit = Util::splitSpaces(*(res.rbegin()+1));
    if (lineSplit.size() < 3)
      ERROR("Erkale::calcEnergy didn't find the result line in the output");
    
    energy = std::stod(lineSplit[1]);
  }
  return energy;
}

Molecule* Erkale::calcOptimized(const Molecule &m, const Params &params) {
  num++;

  // result file
  std::ostringstream ssResultXyzFile;
  ssResultXyzFile << "tmp-result-mol-" << num << ".xyz";
  Util::OnExit rmResultXyzFile([&ssResultXyzFile]() {std::remove(ssResultXyzFile.str().c_str());});

  // params
  auto p = Helpers::paramsToErkaleParams(params) + Params({{"Result",ssResultXyzFile.str()}});
  Helpers::defaultParams(p);

  // run
  auto res = Helpers::runProcess(num, m, "erkale_geom_omp", p);

  // parse output
/*
  Molecule *mopt = new Molecule(str(boost::format("%1%:optimized") % m.descr));
  char state = 'I';
  for (auto const &line : res) {
    std::cout << "LINE: --->" << line << "<--- (state=" << state << ")" << std::endl;
    switch (state) {
    case 'I': {
      if (line == "List of nuclei, geometry in Ångström with three decimal places:")
        state = 'H';
      break;
    } case 'H': {
      if (line == "                 Z          x       y       z")
        state = 'M';
      else
        ERROR("erkale engine: couldn't find the molecule header in the output");
      break;
    } case 'M': {
      if (line.empty())
        continue;
      std::istringstream is(line);
      unsigned num, Z;
      Element elt;
      Float x;
      Float y;
      Float z;
      if (is >> num >> elt >> Z >> x >> y >> z)
        mopt->add(Atom(elt, Vec3(x, y, z)));
      break;
    } default:
      unreachable();
    }
  }

  // detect bonds
  mopt->detectBonds();

  return mopt;
*/

  return Molecule::readXyzFile(ssResultXyzFile.str());
}

}; // engines
}; // Calculators
