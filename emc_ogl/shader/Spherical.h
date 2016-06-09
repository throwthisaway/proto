#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Spherical {
		GLuint id, aPos, aUV, uSmp, uAspect, uR;
		Spherical();
		void Reload();
	};
}
