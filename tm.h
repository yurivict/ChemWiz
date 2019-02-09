#pragma once

#include <ctime>

namespace Tm {

std::time_t start(); // time when the program started
std::time_t now();
std::time_t wallclock();

}; // Tm
