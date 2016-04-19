#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Simple {
		GLuint id, uSmp, uElapsed, uTotal, uRes, uMVP, uTexSize;
		Simple();
		void Reload();
	};
}
