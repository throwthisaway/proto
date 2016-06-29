#include "VBlur7x.h"
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
varying vec2 vUV, vOffsetUVs[4];
const float sum = 1022.,
	w0 = 252./sum, w1 = 210./sum, w2 = 120./sum, w3 = 45./sum,
	w12 = w1 + w2,
	o12 = (w1 + 2.*w2)/w12;

void main() {
	gl_Position = vec4(aPos, 1.0);
	vUV = aUV;
	vOffsetUVs[0] = vUV + vec2(0., -uOffset.y * 3.); 
	vOffsetUVs[1] = vUV + vec2(0., -uOffset.y * o12);
	vOffsetUVs[2] = vUV + vec2(0., uOffset.y * o12); 
	vOffsetUVs[3] = vUV + vec2(0., uOffset.y * 3.);
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
"#line " TOSTRING(__LINE__)
R"(
precision highp float;

varying vec2 vUV, vOffsetUVs[4];
uniform sampler2D uSmp;
// 7-tap normalized weights with linear sampling
const float sum = 1022.,
	w0 = 252./sum, w1 = 210./sum, w2 = 120./sum, w3 = 45./sum,
	w12 = w1 + w2;
void main()
{
	vec4 frag = texture2D( uSmp, vUV) * w0 +
		texture2D( uSmp, vOffsetUVs[0]) * w3 +
		texture2D( uSmp, vOffsetUVs[1]) * w12 +
		texture2D( uSmp, vOffsetUVs[2]) * w12 +
		texture2D( uSmp, vOffsetUVs[3]) * w3;
	gl_FragColor = frag;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	VBlur7x::VBlur7x() { Reload(); }
	void VBlur7x::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uOffset = glGetUniformLocation(id, "uOffset");
	}
}
