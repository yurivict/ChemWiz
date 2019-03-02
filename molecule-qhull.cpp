#include "molecule.h"

#include <vector>
extern "C" {
  #include <libqhull_r/libqhull_r.h>
  #include <libqhull_r/geom_r.h>
}

#include <stdio.h>

std::vector<Vec3> Molecule::computeConvexHullFacets() const {
  std::vector<Vec3> facetNormals;

  int dim = 3;
  char flags[25];
  sprintf (flags, "qhull s FA");

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
      facetNormals.push_back(Vec3(facet->normal[0], facet->normal[1], facet->normal[2]));
    }
  }

  // free
  qh_freeqhull(qh, !qh_ALL);

  return facetNormals;
}
