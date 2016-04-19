#pragma once
#include <GL/glew.h>
namespace Shader {
	struct ColorPosAttrib {
		GLuint id, aVertex, aPos, uMVP, uCol;
		ColorPosAttrib();
		void Reload();
	};
}
