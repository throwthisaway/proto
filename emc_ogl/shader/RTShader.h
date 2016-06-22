#pragma once
#include <GL/glew.h>
namespace Shader {
	struct RTShader {
		RTShader();
		GLuint id, aPos, aUV, aMask,
			uSmpRT, uSmpMask,
			uScreenSize, uTexelSize, uRes,
			uMaskOpacity, uMaskVRepeat;
		void Reload();
	};
}
