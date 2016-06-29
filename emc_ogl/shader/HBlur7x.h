#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct HBlur7x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		HBlur7x();
		void Reload();
	};
}
