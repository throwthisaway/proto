#include "VBlur3x.h"
#include "Shader.h"
#include "../Globals.h"

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
uniform vec2 uOffset;
varying vec2 vUV, vOffsetUVs[6];
void main() {
	gl_Position = vec4(aPos, 1.0);
	vUV = aUV;
	vOffsetUVs[0] = vUV + vec2(0., -uOffset.y); 
	vOffsetUVs[1] = vUV + vec2(0., -uOffset.y * 2.);
	vOffsetUVs[2] = vUV + vec2(0., -uOffset.y * 3.);
	vOffsetUVs[3] = vUV + vec2(0., uOffset.y); 
	vOffsetUVs[4] = vUV + vec2(0., uOffset.y * 2.);
	vOffsetUVs[5] = vUV + vec2(0., uOffset.y * 3.); 
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
"#line " TOSTRING(__LINE__)
R"(
precision highp float;

varying vec2 vUV, vOffsetUVs[6];
uniform sampler2D uSmp;
// 5-tap normalized weights
//const float w0 = 70./238., w1 = 56./238., w2 = 28./238.;
// 7-tap normalized weights
const float w0 = 252./1022., w1 = 210./1022., w2 = 120./1022., w3 = 45./1022.;
void main()
{
	vec4 frag = texture2D( uSmp, vUV) * w0 +
		texture2D( uSmp, vOffsetUVs[0]) * w1 +
		texture2D( uSmp, vOffsetUVs[1]) * w2 +
		texture2D( uSmp, vOffsetUVs[2]) * w3 +
		texture2D( uSmp, vOffsetUVs[3]) * w1 +
		texture2D( uSmp, vOffsetUVs[4]) * w2 +
		texture2D( uSmp, vOffsetUVs[5]) * w3;
	gl_FragColor = frag;
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
