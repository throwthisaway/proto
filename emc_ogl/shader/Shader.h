#pragma once
#include <GL/glew.h>
namespace Shader {
	struct Program {
		const char* vs, *fs;
		GLuint id;
		GLuint Load();
		~Program();
	};
}
