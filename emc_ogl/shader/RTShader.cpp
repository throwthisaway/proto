#include "RTShader.h"
#include "Shader.h"
#include "../Exception.h"
#include "../Globals.h"
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
attribute vec2 aUV, aMask;

varying vec2 vUV, vMask;
void main() {
	gl_Position = vec4(aPos, 1.0);
	vUV = aUV;
	vMask = aMask;
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
"#line " TOSTRING(__LINE__)
R"(
precision highp float;
varying vec2 vUV, vMask;
uniform sampler2D uSmpRT, uSmpMask;
uniform vec2 uScreenSize, uTexelSize;
uniform float uMaskOpacity, uMaskVRepeat;

// 9-tap normalized weights
//uniform float w[] = float[]( .2270270270, .1945945946, .1216216216, .0540540541, .0162162162 );
// 5-tap normalized weights
const float w2 = 28./238., w1 = 56./238., w0 = 70./238.;

vec4 Blur5x(vec2 pos, vec2 d) {
	return texture2D(uSmpRT, pos)*w0
		+ texture2D(uSmpRT, pos - vec2(d.x * 2., d.y))*w2
		+ texture2D(uSmpRT, pos - vec2(d.x, d.y))*w1
		+ texture2D(uSmpRT, pos + vec2(d.x * 2., d.y))*w2
		+ texture2D(uSmpRT, pos + vec2(d.x, d.y))*w1;
}

void main() {
	////vec2 pix = vUV * uRes;
	////vec2 pos = (floor(pix) + .5) / uRes;
	////float d = length(fract(pix)-.5);
	////vec2 vd = (vUV - pos);
	//vec2 vd = uTexelSize;

	//vec4 frag = Blur5x(vUV, vec2(vd.x, 0.)) *w0
	//	+ Blur5x(vUV, vec2(vd.x, vd.y * 2.)) *w2
	//	+ Blur5x(vUV, vec2(vd.x, vd.y)) *w1
	//	+ Blur5x(vUV, vec2(vd.x, -vd.y * 2.)) *w2
	//	+ Blur5x(vUV, vec2(vd.x, -vd.y)) *w1;
	//gl_FragColor = frag;

	vec4 frag =  texture2D(uSmpRT, vUV),
		mask = texture2D(uSmpMask, vec2(mod(vMask.x, uMaskVRepeat), vMask.y));
	gl_FragColor = mix(frag, frag * mask, uMaskOpacity);
}

)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	RTShader::RTShader() : id(0) {
		Reload();
	}
	void RTShader::Reload() {
		if (id) glDeleteProgram(id);
		id = program.Load();
		if (!id) return;
		uSmpRT = glGetUniformLocation(id, "uSmpRT");
		uSmpMask = glGetUniformLocation(id, "uSmpMask");
		uScreenSize = glGetUniformLocation(id, "uScreenSize");
		uTexelSize = glGetUniformLocation(id, "uTexelSize");
		uMaskOpacity = glGetUniformLocation(id, "uMaskOpacity");
		uMaskVRepeat = glGetUniformLocation(id, "uMaskVRepeat");
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		aMask = glGetAttribLocation(id, "aMask");
	}
}

/*
precision highp float;
varying vec2 vUV, vMask;
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
vec2 fragment_pos = uScreenSize * vUV;

vec2 scan = mod(fragment_pos, 4.0);
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
frag *= vec4(0.5);
gl_FragColor = frag;
}*/

//vec4 HBlur3x(vec2 pos) {
//	vec4 frag = texture2D(uSmpRT, vec2(pos.x, pos.y)),
//		frag1 = texture2D(uSmpRT, vec2(pos.x - uTexelSize.x * 3., pos.y)),
//		frag2 = texture2D(uSmpRT, vec2(pos.x - uTexelSize.x * 2., pos.y)),
//		frag3 = texture2D(uSmpRT, vec2(pos.x - uTexelSize.x, pos.y)),
//		frag4 = texture2D(uSmpRT, vec2(pos.x + uTexelSize.x, pos.y)),
//		frag5 = texture2D(uSmpRT, vec2(pos.x + uTexelSize.x * 2., pos.y)),
//		frag6 = texture2D(uSmpRT, vec2(pos.x + uTexelSize.x * 3., pos.y));
//	return (frag + frag1*.12 + frag2*.25 + frag3*.5 + frag4*.5 + frag5 * .25 + frag6 * .12) / 2.75;
//}
//void main() {
//	//vec2 fpos = uScreenSize * vUV;
//	vec4 frag = HBlur3x(vUV);
//	gl_FragColor = frag;
//	/*vec4 frag = texture2D(uSmpRT, vec2(vUV.x, vUV.y)),
//	mask = texture2D(uSmpMask, vec2(mod(vMask.x, uMaskVRepeat), vMask.y));
//
//	gl_FragColor = mix(frag, frag * mask,  uMaskOpacity);*/
//}
