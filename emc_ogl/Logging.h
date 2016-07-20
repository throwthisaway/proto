#include "Globals.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#define LOG_ERR(error, msg) emscripten_log(EM_LOG_ERROR, "ERROR: %d : %s\n", error, msg)
#ifdef EMSCRIPTEN_LOG_CONSOLE
#define LOG_INFO(args...) emscripten_log(EM_LOG_CONSOLE, args)
#else
#define LOG_INFO(args...) printf(args)
#endif
#else
#include <iostream>
#include <iomanip>
#define LOG_ERR(error, msg) std::cerr << "ERROR: " << error << " : " << msg << "\n";
#define LOG_INFO(...) printf(__VA_ARGS__)
#endif
