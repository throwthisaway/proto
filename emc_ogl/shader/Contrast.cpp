#include "Contrast.h"
#include "Shader.h"

namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision highp float;
attribute vec3 aPos;
attribute vec2 aUV;
varying vec2 vUV;
void main() {
	gl_Position = vec4(aPos, 1.0);
	vUV = aUV;
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
varying vec2 vUV;
uniform float uBrightness, uContrast;
uniform sampler2D uSmp;
void main()
{
	vec4 s = texture2D( uSmp, vUV);
	gl_FragColor = (s - 0.5) * uContrast + 0.5 + uBrightness;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Contrast::Contrast() { Reload(); }
	void Contrast::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uBrightness = glGetUniformLocation(id, "uBrightness");
		uContrast = glGetUniformLocation(id, "uContrast");
	}
}
