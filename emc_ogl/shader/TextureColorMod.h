#pragma once
#include <GL/glew.h>
namespace Shader {
	struct TextureColorMod {
		GLuint id, aPos, aUV1, uSmp, /*uElapsed, uTotal,*/ uMVP, uCol;
		TextureColorMod();
		void Reload();
	};
}
