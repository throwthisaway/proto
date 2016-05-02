#include "VBlur3x.h"
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
uniform vec2 uOffset;
varying vec2 vUV;
uniform sampler2D uSmp;
void main()
{
	vec4 s0 = texture2D( uSmp, vUV),
		s1 = texture2D( uSmp, vUV + vec2(-uOffset.x, 0.)),
		s2 = texture2D( uSmp, vUV + vec2(uOffset.x, 0.));
	gl_FragColor = (s0 + s1 * .5 + s2 * .5) / 3.;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	VBlur3x::VBlur3x() { Reload(); }
	void VBlur3x::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uOffset = glGetUniformLocation(id, "uOffset");
	}
}
