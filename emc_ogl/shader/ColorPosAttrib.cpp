#include "ColorPosAttrib.h"
#include "Shader.h"

namespace {
	const char * vs =
#ifndef __EMSCRIPTEN__
		"#version 130\n"
#endif
		R"(
precision highp float;
attribute vec3 aVertex, aPos;
uniform mat4 uMVP;
void main() {
	gl_Position = uMVP * vec4(aVertex + aPos, 1.0);
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
	ColorPosAttrib::ColorPosAttrib() { Reload(); }
	void ColorPosAttrib::Reload() {
		id = program.Load();
		if (!id) return;
		aVertex = glGetAttribLocation(id, "aVertex");
		aPos = glGetAttribLocation(id, "aPos");
		uMVP = glGetUniformLocation(program.id, "uMVP");
		uCol = glGetUniformLocation(program.id, "uCol");
	}
}
