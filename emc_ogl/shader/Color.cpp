#include "Color.h"
#include "Shader.h"
namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision highp float;
attribute vec3 pos;
uniform mat4 uMVP;
void main() {
	gl_Position = uMVP * vec4(pos, 1.0);
}
)", *fs =
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
uniform vec4 uCol;
void main() {
	gl_FragColor = uCol;
}
)";
	Shader::Program program{ vs, fs, 0 };
}
namespace Shader {
	Color::Color() { Reload(); }
	void Color::Reload() {
		id = program.Load();
		if (!id) return;
		uMVP = glGetUniformLocation(program.id, "uMVP");
		uCol = glGetUniformLocation(program.id, "uCol");
	}
}
