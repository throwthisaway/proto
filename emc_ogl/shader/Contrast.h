#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Contrast {
		GLuint id, aPos, aUV, uSmp, uContrast, uBrightness;
		Contrast();
		void Reload();
	};
}
