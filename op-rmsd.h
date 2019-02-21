#pragma once

#include <valarray>

namespace Op {

// rmsd vector function: it alters the arguments for the sake of efficiency
double rmsd(std::valarray<double> &v1, std::valarray<double> &v2);

}; // Op
