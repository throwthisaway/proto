#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct BloomPass1 {
		GLuint id, aPos, aUV, uSmp, uOffset, uThreshold, uRamp;
		BloomPass1();
		void Reload();
	};
}
