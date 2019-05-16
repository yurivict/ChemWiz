#include "util.h"
#include "xerror.h"

#include <boost/algorithm/string.hpp> 


namespace Util {

bool strAsBool(const std::string &str) {
  auto strl = boost::algorithm::to_lower_copy(str);
  if (strl == "yes" || strl == "true" || strl == "on" || strl == "1")
    return true;
  if (strl == "no" || strl == "false" || strl == "off" || strl == "0")
    return false;
  ERROR("the string '" << str << "' can't be converted to boolean")
}

}; // Util
