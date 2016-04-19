#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Texture {
		GLuint id, aPos, aUV1, uSmp, /*uElapsed, uTotal,*/ uMVP;
		Texture();
		void Reload();
	};
}
