#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct HBlur9x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		HBlur9x();
		void Reload();
	};
}
