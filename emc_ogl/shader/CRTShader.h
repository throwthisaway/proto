#pragma once
#include <GL/glew.h>
namespace Shader {
	struct CRTShader {
		CRTShader();
		GLuint id, aPos, aUV, aMask,
			uSmpRT, uSmpMask,
			uScreenSize, uTexelSize,
			uMaskOpacity, uMaskVRepeat;
		void Reload();
	};
}
