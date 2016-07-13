#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#define LOG_ERR(error, msg) emscripten_log(EM_LOG_ERROR, "ERROR: %d : %s\n", error, msg)
#define LOG_INFO(msg) emscripten_log(EM_LOG_CONSOLE, "%s", msg)
#else
#include <iostream>
#include <iomanip>
#define LOG_ERR(error, msg) std::cerr << "ERROR: " << error << " : " << msg << "\n";
#define LOG_INFO(msg) std::cout << msg;
#endif
