#include "RTShader.h"
#include "Shader.h"
#include "../Exception.h"

namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision highp float;
attribute vec3 aPos;
attribute vec2 aRT, aMask;

varying vec2 vRT, vMask;
void main() {
	gl_Position = vec4(aPos, 1.0);
	vRT = aRT;
	vMask = aMask;
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
varying vec2 vRT, vMask;
uniform sampler2D uSmpRT, uSmpMask;
uniform vec2 uScreenSize;
uniform float uMaskOpacity, uMaskVRepeat;
float less(float v, float cmp) {
	return 1.0 - step(v, cmp);
}

float ge(float v, float cmp) {
	return step(v, cmp);
}

float in_between(float v, float l, float u) {
	return (1.0 - step(v, l)) * step(v, u);
}

void main() {
	vec2 fragment_pos = uScreenSize * vRT;
	vec4 frag = texture2D(uSmpRT, vec2(vRT.x, vRT.y)),
		mask = texture2D(uSmpMask, vec2(mod(vMask.x, uMaskVRepeat), vMask.y));
	/*vec2 scan = mod(fragment_pos, 4.0);
	float k1 = 0.4, k2 = 0.2;
	vec4 mask1 = vec4(1.0, k1, k2, 1.0),
		mask2 = vec4(k1, 1.0, k2, 1.0),
		mask3 = vec4(k1, k2, 1.0, 1.0),
		mask4 = vec4(0.0, k1, k2, 1.0) ;
	if (scan.x <= 1.0)
		frag *= mask1;
	else if (scan.x <= 2.0)
		frag *= mask2;
	else if (scan.x <= 3.0)
		frag *= mask3;
	else
		frag *= mask4;
	if (scan.y >= 3.0)
		frag *= vec4(0.5);*/
	gl_FragColor = mix(frag, frag * mask,  uMaskOpacity);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	RTShader::RTShader() {
		Reload();
	}
	void RTShader::Reload() {
		id = program.Load();
		if (!id) return;
		uSmpRT = glGetUniformLocation(id, "uSmpRT");
		uSmpMask = glGetUniformLocation(id, "uSmpMask");
		uScreenSize = glGetUniformLocation(id, "uScreenSize");
		uMaskOpacity = glGetUniformLocation(id, "uMaskOpacity");
		uMaskVRepeat = glGetUniformLocation(id, "uMaskVRepeat");
		aPos = glGetAttribLocation(id, "aPos");
		aRT = glGetAttribLocation(id, "aRT");
		aMask = glGetAttribLocation(id, "aMask");
	}
}
