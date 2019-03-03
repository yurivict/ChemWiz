#include "molecule.h"

#include <vector>
#include <limits>

extern "C" {
#include <libqhull_r/libqhull_r.h>
#include <libqhull_r/geom_r.h>
}

#include <stdio.h>

std::vector<Vec3> Molecule::computeConvexHullFacets(std::vector<double> *withFurthestdist) const {
  std::vector<Vec3> facetNormals;

  int dim = 3;
  char flags[25];
  sprintf(flags, "qhull s FA Pp"); // Pp removes the warnings about narrow hulls (we will get narrow hulls for flat molecules)

  // fill the 'points' array
  std::vector<coordT> points;
  for (auto a : atoms) {
    points.push_back(a->pos(X));
    points.push_back(a->pos(Y));
    points.push_back(a->pos(Z));
  }

  // create the qhT object
  qhT qhVal;
  qhT *qh = &qhVal;
  qh_zero(qh, stderr);
  qh_new_qhull(qh, dim, numAtoms(), &points[0], 0, flags, NULL, NULL);

  { // enumerate and output facets
    facetT *facet;
    FORALLfacets {
      if (withFurthestdist) {
#if 0 // QHull-based computation: always returns zero, it's unclear why, see https://github.com/qhull/qhull/issues/39
#  if qh_COMPUTEfurthest
#    error "qh_COMPUTEfurthest=1: need to compute furthestdist ourselves!"
#  endif
        withFurthestdist->push_back(facet->furthestdist);
#else // workaround for the above
        Float pmin = std::numeric_limits<Float>::max();
        Float pmax = std::numeric_limits<Float>::min();
        for (auto a : atoms) {
          auto p = a->pos*Vec3(facet->normal[0], facet->normal[1], facet->normal[2]);
          if (p < pmin)
            pmin = p;
          if (p > pmax)
            pmax = p;
        }
        withFurthestdist->push_back(pmax - pmin);
#endif
      }
      facetNormals.push_back(Vec3(facet->normal[0], facet->normal[1], facet->normal[2]));
    }
  }

  // free
  qh_freeqhull(qh, !qh_ALL);

  return facetNormals;
}
