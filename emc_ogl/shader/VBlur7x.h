#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct VBlur7x {
		GLuint id, aPos, aUV, uSmp, uOffset;
		VBlur7x();
		void Reload();
	};
}
