#pragma once
#pragma once
#include <GL/glew.h>
namespace Shader {
	struct CombineMAdd {
		GLuint id, aPos, aUV, uSmp1, uSmp2, uMix;
		CombineMAdd();
		void Reload();
	};
}
