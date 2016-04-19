#include "Simple.h"
#include "Shader.h"

namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision mediump float;
// Input vertex data, different for all executions of this shader.
attribute vec3 pos;
attribute vec2 uv1_in;

uniform mat4 uMVP;

varying vec2 uv1;

void main() {
	gl_Position = uMVP * vec4(pos, 1.);
	uv1 = uv1_in;
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision mediump float;
varying vec2 uv1;

uniform sampler2D smp1;
uniform vec2 uRes, uTexSize;
uniform float uElapsed, uTotal;
#define FACTOR 8.0
vec3 voronoi(vec2 x) {
	vec2 n = floor(x);
	vec2 f = fract(x);
	float min_d = FACTOR + 1.0;
	vec2 min_site, min_offset;
	for (int i = -1; i<=1; ++i)
	for (int j = -1; j<=1; ++j)
	{
		vec2 o = vec2(float(i), float(j));
		vec2 rnd_site = texture2D( smp1, (n + o +.5)/uTexSize).xy;
		rnd_site =.5 + sin(uTotal*.001*rnd_site)*.5;
		rnd_site += o - f ;	// distance to the current fragment
		float d = dot(rnd_site, rnd_site);
		if (d<min_d) {
			min_d = d;
			min_site = rnd_site;
			min_offset =  o;
		}
	}

			min_d = FACTOR + 1.;
	for (int i = -2; i<=2; ++i)
	for (int j = -2; j<=2; ++j)
	{
		vec2 o = min_offset + vec2(float(i), float(j));
		vec2 rnd_site = texture2D( smp1, (n + o +.5)/uTexSize).xy;
		rnd_site =.5 + sin(uTotal*.001*rnd_site)*.5;
		rnd_site += o - f ;	// distance to the current fragment
		vec2 vd = rnd_site - min_site;
		float d = dot(vd, vd);
		if (d>0.00001)
			min_d = min(min_d, dot(0.5*(min_site + rnd_site), normalize(vd)));
	}
	min_d = sqrt(min_d);
	return vec3(min_d, min_site.x, min_site.y);
}

void main()
{
	vec2 p = uv1 * FACTOR;
	vec3 res = voronoi(p);
	float dis = 1.0 - smoothstep( 0.0, 0.2, res.x );
	//color = vec3(HeatMapColor(smoothstep(.0,.8, res.x ), 0., 1.)) + dis * vec3(1.);
	gl_FragColor = (dis + smoothstep(.0,.8, res.x ))*vec4(1.);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Simple::Simple() { Reload(); }
	void Simple::Reload() {
		id = program.Load();
		if (!id) return;
		uSmp = glGetUniformLocation(id, "smp1");
		uElapsed = glGetUniformLocation(id, "uElapsed");
		uTotal = glGetUniformLocation(id, "uTotal");
		uRes = glGetUniformLocation(id, "uRes");
		uMVP = glGetUniformLocation(id, "uMVP");
		uTexSize = glGetUniformLocation(id, "uTexSize");
	}
}
