#include "BloomPass1.h"
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
const float sum = 4070.,
	w0 = 924./sum, w1 = 792./sum, w2 = 495./sum, w3 = 220./sum, w4 = 66./sum,
	w12 = w1 + w2, w34 = w3 + w4,
	o12 = (w1 + 2.*w2)/w12,
	o34 = (3. * w3 + 4.*w4)/w34;

void main() {
	gl_Position = vec4(aPos, 1.0);
	vUV = aUV;
	vOffsetUVs[0] = vUV + vec2(-uOffset.x * o34, 0.); 
	vOffsetUVs[1] = vUV + vec2(-uOffset.x * o12, 0.);
	vOffsetUVs[2] = vUV + vec2(uOffset.x * o12, 0.); 
	vOffsetUVs[3] = vUV + vec2(uOffset.x * o34, 0.);
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
uniform float uThreshold, uRamp;
// 9-tap normalized weights with linear sampling
const float sum = 4070.,
	w0 = 924./sum, w1 = 792./sum, w2 = 495./sum, w3 = 220./sum, w4 = 66./sum,
	w12 = w1 + w2, w34 = w3 + w4;
vec4 Highlight(vec2 pos) {
	vec4 frag = texture2D( uSmp, pos);
	float sum = (frag.x + frag.y + frag.z) / 3.;
	frag *= smoothstep(uThreshold, uThreshold + uRamp, sum);
	frag.w = 1.;
	return frag;
}
void main()
{
	vec4 frag = Highlight(vUV) * w0 +
		Highlight(vOffsetUVs[0]) * w34 +
		Highlight(vOffsetUVs[1]) * w12 +
		Highlight(vOffsetUVs[2]) * w12 +
		Highlight(vOffsetUVs[3]) * w34;
	gl_FragColor = frag;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	BloomPass1::BloomPass1() { Reload(); }
	void BloomPass1::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uOffset = glGetUniformLocation(id, "uOffset");
		uThreshold = glGetUniformLocation(id, "uThreshold");
		uRamp = glGetUniformLocation(id, "uRamp");
	}
}
