#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct VBlur9x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		VBlur9x();
		void Reload();
	};
}
