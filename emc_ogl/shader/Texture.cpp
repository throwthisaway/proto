#include "Texture.h"
#include "Shader.h"
namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision mediump float;
attribute vec3 aPos;
attribute vec2 aUV1;

uniform mat4 uMVP;
varying vec2 uv1;

void main() {
	gl_Position = uMVP * vec4(aPos, 1.);
	uv1 = aUV1;
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision mediump float;
varying vec2 uv1;

uniform sampler2D smp1;
//uniform float uElapsed, uTotal;

void main()
{
	gl_FragColor = texture2D( smp1, uv1);
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Texture::Texture() { Reload(); }
	void Texture::Reload() {
		id = program.Load();
		if (!id) return;
		aPos = glGetAttribLocation(id, "aPos");
		aUV1 = glGetAttribLocation(id, "aUV1");
		uSmp = glGetUniformLocation(id, "smp1");
		//uElapsed = glGetUniformLocation(program.id, "uElapsed");
		//uTotal = glGetUniformLocation(program.id, "uTotal");
		uMVP = glGetUniformLocation(id, "uMVP");
	}
}
