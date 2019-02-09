#include "common.h"

#include <assert.h>

[[ noreturn ]] void unreachable() {
  assert(false);
}
