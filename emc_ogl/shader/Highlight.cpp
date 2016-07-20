#include "Highlight.h"
#include "Shader.h"
#include "../Globals.h"
#include "../Logging.h"

namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		"#line " TOSTRING(__LINE__)
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
"#line " TOSTRING(__LINE__)
R"(
precision highp float;
varying vec2 vUV;
uniform sampler2D uSmp;
uniform float uThreshold, uRamp;
vec4 Highlight(vec2 pos) {
	vec4 frag = texture2D( uSmp, pos);
	//float sum = (frag.x + frag.y + frag.z) / 3.;
	float sum = max(frag.x, max(frag.y, frag.z));
	frag *= smoothstep(uThreshold, uThreshold + uRamp, sum);
	frag.w = 1.;
	return frag;
}
void main()
{
	gl_FragColor = Highlight(vUV);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Highlight::Highlight() { Reload(); }
	void Highlight::Reload() {
		LOG_INFO(">>>Compile Highlight shader\n");
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uThreshold = glGetUniformLocation(id, "uThreshold");
		uRamp = glGetUniformLocation(id, "uRamp");
	}
}
