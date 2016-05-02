#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct VBlur3x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		VBlur3x();
		void Reload();
	};
}
