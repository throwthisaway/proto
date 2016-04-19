#pragma once
#include <GL/glew.h>
namespace Shader {
	struct RTShader {
		RTShader();
		GLuint id, aPos, aRT, aMask, uSmpRT, uSmpMask, uScreenSize, uMaskOpacity, uMaskVRepeat;
		void Reload();
	};
}
