#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Highlight {
		GLuint id, aPos, aUV, uSmp, uThreshold, uRamp;
		Highlight();
		void Reload();
	};
}
