#include <GLFW/glfw3.h>
#include <string>
#include <memory>
#include "Socket.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
static GLFWwindow * window = nullptr;
static const int WIDTH = 640, HEIGHT = 480;
static std::string msg;
static std::vector<std::string> messages;
void character_callback(GLFWwindow* window, unsigned int codepoint)
{
	msg += codepoint;
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ENTER && !msg.empty()) {
		messages.push_back(msg);
		msg.clear();
	}
}

void one_iter();
std::unique_ptr<Client> ws;
int main(int argc, char** argv) {
	glfwInit();
	window = glfwCreateWindow(WIDTH, HEIGHT, "glfw3_events", NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetCharCallback(window, character_callback);
	glfwSetKeyCallback(window, key_callback);
	if (argc > 1) {
		emscripten_log(EM_LOG_CONSOLE, "host: %s", argv[1]);
		if (!strcmp(argv[1], "0.0.0.1"))
			ws = std::make_unique<Client>("localhost", 8000, true);
		else
			ws = std::make_unique<Client>(argv[1], 8000, true);
	}
	else {
		emscripten_log(EM_LOG_CONSOLE, "no host provided, defaulting to localhost");
		ws = std::make_unique<Client>("localhost", 8000, true);
	}
	//ws = std::make_unique<Client>("127.0.0.1", 8080, true, false);
#ifdef __EMSCRIPTEN__
  // void emscripten_set_main_loop(em_callback_func func, int fps, int simulate_infinite_loop);
  emscripten_set_main_loop(one_iter, 60, 1);
#else
	glClearColor(1.f, 1.f, 1.f, 1.f);
  while (1) {
    one_iter();
    // Delay to keep frame rate constant (using SDL)
  }
#endif
}
void one_iter() {
  //glfwPollEvents();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // process input
  // render to screen
	while (messages.size()) {
		ws->Send(messages.back());
		messages.pop_back();
	}
}