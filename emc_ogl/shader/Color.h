#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Color {
		GLuint id, aPos, uMVP, uCol;
		Color();
		void Reload();
	};
}
