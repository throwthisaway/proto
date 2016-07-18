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
uniform float uR;
const float squircle_r = 1.;
void main()
{
	vec2 ab = vUV;
	ab = (ab - .5) * 2.;
	// this should be here: vec2 squircle = ab;
	ab = ab*uR/sqrt(uR * uR - dot(ab, ab))/*/6.28 stretch uv to the sphere diameter*/;
	vec2 squircle = ab;
	ab = ab / 2. + .5;
	// pow is buggy for negative numbers on webgl...
	if (pow(abs(squircle.x), 5.) + pow(abs(squircle.y), 5.) < pow(squircle_r, 5.))
		gl_FragColor = texture2D( uSmp, ab);
	else
		gl_FragColor = vec4(1., 1., 1., 1.);	//discard;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Spherical::Spherical() { Reload(); }
	void Spherical::Reload() {
		LOG_INFO(">>>Compile Spherical shader\n");
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV = glGetAttribLocation(id, "aUV");
		uSmp = glGetUniformLocation(id, "uSmp");
		uR = glGetUniformLocation(id, "uR");
	}
}
