#include "CRTShader.h"
#include "Shader.h"
#include "../Exception.h"
#include "../Globals.h"
#include "../Logging.h"
// https://www.shadertoy.com/view/ls2SRD
// https://software.intel.com/en-us/blogs/2014/07/15/an-investigation-of-fast-real-time-gpu-based-image-blur-algorithms
// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
// http://xissburg.com/faster-gaussian-blur-in-glsl/
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

varying vec2 vUV, vMask;
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
uniform sampler2D uSmpRT;
uniform vec2 uScreenSize, uTexelSize;

const float scanLineHeight = 4., scanLineStep = 1./scanLineHeight,
	scanLineBrightness = .7;

vec4 Scan(vec2 pos){
	float d = pos.y * uScreenSize.y / scanLineHeight;
	d = abs(fract(d) - .5 + scanLineStep / 2.) * 2.;
	d = 1. - pow(d * scanLineBrightness, 4.);
	return vec4(d, d, d, 1.);
}

const float low=.5;
const float high=1.5;

float less(float v, float cmp) {
	return step(v, cmp);
}
float greater(float v, float cmp) {
	return step(cmp, v);
}
float between(float v, float l, float h) {
	return less(v, h) * greater(v, l);
}

// old-CRT
vec4 MaskCRT(vec2 pos) {
	pos *= uScreenSize;
	float remainder = mod(pos.x, 3.); // TODO:: float f = fract(pos.x / 3.); less(f, 1./3.);... etc. might be faster
	vec4 res;
	res.r = less(remainder, 1.);
	res.g = between(remainder, 1., 2.);
	res.b = greater(remainder, 2.);
	res+=low;
	res.w = 1.;
	return res;
}

// Shadow Mask
vec4 MaskShadow(vec2 pos){
	pos *= uScreenSize;
	pos.x+=pos.y * 3.;
	float remainder = mod(pos.x, 6.);	// TODO:: float f = fract(pos.x / 6.); less(f, 1./3.);... etc. might be faster
	vec4 res;
	res.r = less(remainder, 2.);
	res.g = between(remainder, 2., 4.);
	res.b = greater(remainder, 4.);
	res+=low;
	res.w = 1.;
	return res;
}

void main() {
	vec4 frag =  texture2D(uSmpRT, vUV);	
	gl_FragColor = Scan(vUV) * frag * MaskCRT(vUV);
	//gl_FragColor = Scan(vUV) * frag * MaskShadow(vUV);
}

)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	CRTShader::CRTShader() : id(0) {
		Reload();
	}
	void CRTShader::Reload() {
		LOG_INFO(">>>Compile CRTShader shader");
		if (id) glDeleteProgram(id);
		id = program.Load();
		if (!id) return;
		uSmpRT = glGetUniformLocation(id, "uSmpRT");
		uScreenSize = glGetUniformLocation(id, "uScreenSize");
		uTexelSize = glGetUniformLocation(id, "uTexelSize");
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
	}
}
