#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct BloomPass2 {
		GLuint id, aPos, aUV, uSmp1, uSmp2, uOffset, uMix;
		BloomPass2();
		void Reload();
	};
}
