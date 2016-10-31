#pragma once

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define EMSCRIPTEN_LOG_CONSOLE
//#define VAO_SUPPORT
#define DEBUG_REL
#define MAX_NPC 20
#define CLIENTID_LEN 5
//#define LINE_RENDER
#define LINE_WIDTH 3.f
#define VP_RATIO(h) h * 3 / 4
#define HUD_RATIO(h) h / 4
#define CULL 2.f	// two screen
