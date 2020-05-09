#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#define STR(msg...) \
	([&]() { \
	  std::ostringstream ss; \
	  ss << msg; \
	  ss.flush(); \
	  return ss.str(); \
	}())
#define CSTR(msg...) (STR(msg).c_str())

#define PRINT(msg...) \
	{ \
		std::cout << STR(msg << std::endl); \
	}
#define WARNING(msg...) \
	{ \
		std::cerr << STR("WARNING: " << msg << std::endl); \
	}
#define PRINT_ERR(msg...) \
	{ \
		std::cerr << STR(msg << std::endl); \
	}
#define FAIL(msg...) \
	{ \
		std::cerr << STR("error: " << msg << std::endl); \
		fflush(stdout); \
		fflush(stderr); \
		exit(EXIT_FAILURE); \
	}
