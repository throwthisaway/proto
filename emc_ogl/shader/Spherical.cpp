#include "Spherical.h"
#include "Shader.h"
#include "../Logging.h"

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
const float squircle_r = 1.;
void main()
{
	vec2 ab = vUV;
	ab = (ab - .5) * 2.;
	ab = ab*uR/sqrt(uR * uR - dot(ab, ab))/*/6.28 stretch uv to the sphere diameter*/;
	vec2 squircle = ab;
	ab = ab / 2. + .5;
	if (pow(squircle.x, 5.) + pow(squircle.y, 5.) < pow(squircle_r, 5.))
		gl_FragColor = texture2D( uSmp, ab);
	else
		gl_FragColor = vec4(0., 0., 0., 1.f);	//discard;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Spherical::Spherical() { Reload(); }
	void Spherical::Reload() {
		#ifdef __EMSCRIPTEN__
			emscripten_log(EM_LOG_CONSOLE, ">>>Compile spherical shader");
		#endif
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uAspect = glGetUniformLocation(id, "uAspect");
		uR = glGetUniformLocation(id, "uR");
	}
}
