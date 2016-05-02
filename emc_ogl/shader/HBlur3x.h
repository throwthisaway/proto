#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct HBlur3x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		HBlur3x();
		void Reload();
	};
}
