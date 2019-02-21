#pragma once

#include <valarray>

class Molecule;

namespace Op {

// vector function: it alters the arguments for the sake of efficiency
double rmsdVec(std::valarray<double> &v1, std::valarray<double> &v2);
double rmsd(const Molecule &m1, const Molecule &m2);

}; // Op
