#include "CombineMAdd.h"
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
	gl_Position = vec4(aPos, 1.);
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
uniform sampler2D uSmp1, uSmp2;
uniform float uMix;

void main()
{
	gl_FragColor = texture2D( uSmp1, vUV) * uMix + texture2D( uSmp2, vUV);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	CombineMAdd::CombineMAdd() { Reload(); }
	void CombineMAdd::Reload() {
		LOG_INFO(">>>Compile CombineMAdd shader");
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp1 = glGetUniformLocation(id, "uSmp1");
		uSmp2 = glGetUniformLocation(id, "uSmp2");
		uMix = glGetUniformLocation(id, "uMix");
	}
}
