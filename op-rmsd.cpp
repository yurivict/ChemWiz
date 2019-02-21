#include "op-rmsd.h"
#include "molecule.h"

#include "contrib/rmsd/rmsd++/kabsch.h"

#include <valarray>

namespace Op {

typedef std::valarray<double> matrix;

double rmsd(std::valarray<double> &v1, std::valarray<double> &v2) {
  std::valarray<double> v1ctr;
  std::valarray<double> v2ctr;

  assert(v1.size() == v2.size() && v1.size()%3 == 0);
  auto nPts = v1.size()/3;

  // find the center of the structures
  v1ctr = kabsch::centroid(v1);
  v2ctr = kabsch::centroid(v2);

  // recenter point sets at origins
  for (unsigned int p = 0; p < nPts; p++) {
    for(unsigned int d = 0; d < 3; d++) {
      v1[3*p + d] -= v1ctr[d];
      v2[3*p + d] -= v2ctr[d];
    }
  }

  // rotate and calculate rmsd
  return kabsch::kabsch_rmsd(v1, v2, nPts);
}

} // Op
