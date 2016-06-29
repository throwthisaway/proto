#pragma once
#include <GL/glew.h>
namespace Shader {
	struct CRTShader {
		CRTShader();
		GLuint id, aPos, aUV,
			uSmpRT,
			uScreenSize, uTexelSize;
		void Reload();
	};
}
