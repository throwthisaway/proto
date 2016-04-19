#include "Shader.h"
#include <vector>
#include "../Logging.h"
#include "../Exception.h"

namespace Shader {
	GLuint Program::Load() {
		using namespace std;
		if (id) glDeleteProgram(id);
		id = 0;
		// Create the shaders
		GLuint vid = glCreateShader(GL_VERTEX_SHADER);
		GLuint fid = glCreateShader(GL_FRAGMENT_SHADER);
		GLint result = GL_FALSE;
		int InfoLogLength;

		glShaderSource(vid, 1, &vs, NULL);
		glCompileShader(vid);

		// Check Vertex Shader
		glGetShaderiv(vid, GL_COMPILE_STATUS, &result);
		glGetShaderiv(vid, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (!result) {
			std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
			glGetShaderInfoLog(vid, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
			LOG_INFO("Vertex Shader:");
			LOG_ERR(-1, &VertexShaderErrorMessage[0]);
#ifdef EXCEPTIONS
			throw custom_exception("Vertex shader compilation error");
#else
			return 0;
#endif
		}

		glShaderSource(fid, 1, &fs, NULL);
		glCompileShader(fid);

		// Check Fragment Shader
		glGetShaderiv(fid, GL_COMPILE_STATUS, &result);
		glGetShaderiv(fid, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (!result) {
			std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
			glGetShaderInfoLog(fid, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
			LOG_INFO("Fragment Shader:");
			LOG_ERR(-1, &FragmentShaderErrorMessage[0]);
#ifdef EXCEPTIONS
			ThrowIf(!fid, "Fragment shader compilation error");
#else
			return 0;
#endif
		}

		id = glCreateProgram();
		glAttachShader(id, vid);
		glAttachShader(id, fid);
		glLinkProgram(id);
		glDeleteShader(vid);
		glDeleteShader(fid);
		// Check the program
		glGetProgramiv(id, GL_LINK_STATUS, &result);
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (!result) {
			std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
			glGetProgramInfoLog(id, InfoLogLength, NULL, &ProgramErrorMessage[0]);
			LOG_ERR(-1, &ProgramErrorMessage[0]);
			glDeleteProgram(id);
			id = 0;
#ifdef EXCEPTIONS
			ThrowIf(!fid, "Shader program link error");
#else
			return 0;
#endif
		}
		return id;
	}

	Program::~Program() {
		glDeleteShader(id);
	}
}
