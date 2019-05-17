#pragma once

#include <ctime>
#include <string>

namespace Tm {

std::time_t start(); // time when the program started
std::time_t now();
std::time_t wallclock();

// printing
std::string strYearToMicrosecond();

}; // Tm
