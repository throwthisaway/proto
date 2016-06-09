#include "Spherical.h"
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
uniform sampler2D uSmp;
uniform float uR, uAspect;
void main()
{
	//vec2 ab = vUV - vec2(.25 * uAspect, .5);
	//ab.x *= uAspect;
	vec2 ab = vUV - vec2(.5*uAspect, .5);
	float dz = uR - sqrt(uR * uR - ab.x * ab.x - ab.y * ab.y);
	gl_FragColor = texture2D( uSmp, vUV + vec2(dz, dz));
	//gl_FragColor = vec4(vec2(dz, dz), 0.f, 1.f);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Spherical::Spherical() { Reload(); }
	void Spherical::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uAspect = glGetUniformLocation(id, "uAspect");
		uR = glGetUniformLocation(id, "uR");
	}
}
