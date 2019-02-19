
#include <iostream>

#include "xerror.h"
#include "common.h"
#include "js-binding.h"
#include "util.h"
#include "tm.h"

#include <mujs.h>

static void printTime(std::time_t tm, const char *when) {
  std::cout << "-- " << PROGRAM_NAME << " is " << when << " at " << ctime(&tm); // ctime contains EOL
}

static int main_guarded(int argc, char* argv[]) {
  // arguments parsing
  if (argc < 2) {
    std::cerr << "expect an argument: {xyz-file}" << std::endl;
    return 1;
  }

  // time printouts
  printTime(Tm::start(), "starting");
  static Util::OnExit doPrintTime([](){
    auto tmFinish = Tm::now();
    printTime(tmFinish, "finishing");
    std::cout << "-- ran for " << (tmFinish-Tm::start()) << " second(s) (wallclock) --" << std::endl;
  });

  // run JavaScript, everything else is done from there
  js_State *J = js_newstate(NULL, NULL, JS_STRICT);
  JsBinding::registerFunctions(J);
  js_dofile(J, argv[1]);
  js_freestate(J);

  return 0;
}

int main(int argc, char* argv[]) {
#if defined(USE_EXCEPTIONS)
  try {
    return main_guarded(argc, argv);
  } catch (const Exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  } catch (const std::runtime_error &e) {
    std::cerr << "error (runtime): " << e.what() << std::endl;
    return 1;
  }
#else
  return main_guarded(argc, argv);
#endif
}
