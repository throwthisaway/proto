#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#else
#include <iostream>
#include <iomanip>
#endif
#include <assert.h>
#include <string.h>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <chrono>

#include <stdio.h>
#include <vector>
#include <array>
#include <queue>
#include <list>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
//#define VAO_SUPPORT

static const int width = 640, height = 480;
static const size_t texw = 256, texh = 256;
static GLFWwindow * window;
static std::random_device rd;
static std::mt19937 mt(rd());
struct AABB {
	float l, t, r, b;
	AABB Translate(const glm::vec3& pos) const {
		return{ l + pos.x, t + pos.y, r + pos.x, b + pos.y };
	}
	AABB Scale(float s) {
		return{ l * s, t * s, r * s, b * s };
	}
};
AABB Union(const AABB& l, const AABB& r) {
	return{ std::min(l.l, r.l), std::max(l.t, r.t), std::max(l.r, r.r), std::min(l.b, r.b) };
}

static AABB CalcAABB(std::vector<glm::vec3> v, GLint first, GLsizei count) {
	AABB aabb{ std::numeric_limits<float>::max(),
		-std::numeric_limits<float>::max(),
		-std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max() };
	for (auto i = first; i < first + count; ++i) {
		aabb.l = std::min(v[i].x, aabb.l);
		aabb.t = std::max(v[i].y, aabb.t);
		aabb.r = std::max(v[i].x, aabb.r);
		aabb.b = std::min(v[i].y, aabb.b);
	}
	return aabb;
}
static AABB RecalcAABB(const AABB& aabb, const float rot) {
	auto m = glm::rotate(glm::mat4{}, rot, { 0.f, 0.f, 1.f });
	auto lt = m * glm::vec4{ aabb.l, aabb.t, 0.f, 1.f },
		lb = m * glm::vec4{ aabb.l, aabb.b, 0.f, 1.f }, 
		rt = m * glm::vec4{ aabb.r, aabb.t, 0.f, 1.f },
		rb = m * glm::vec4{ aabb.r, aabb.b, 0.f, 1.f };
	return{ std::min(lt.x, std::min(lb.x, std::min(rt.x, rb.x))),
		std::max(lt.y, std::max(lb.y, std::max(rt.y, rb.y))),
		std::max(lt.x, std::max(lb.x, std::max(rt.x, rb.x))),
		std::min(lt.y, std::min(lb.y, std::min(rt.y, rb.y))) };
}
namespace Asset {
	static const std::initializer_list<glm::vec3> proto_v_data = { /*thruster1*/ {-0.1f, .0f, 0.f},
		{-.1f, -.2f, 0.f },
		{-.02f,  -.15f, 0.f},
		{.1f, -.25f, 0.f},
		{.1f, 0.f, 0.f},
		/*thruster2*/{-0.1f, .0f, 0.f},
		{-.1f, -.25f, 0.f},
		{.02f, -.35f, 0.f},
		{.1f, -.2f, 0.f},
		{.1f, 0.f, 0.f},
		/*lower part*/{-.5f, .5f, 0.f},
		{-.5f, -.5f, 0.f},
		{.5f, -.5f, 0.f},
		{.5f, .5f, 0.f},
		{.3f, .6f, 0.f},
		{-.3f, .6f, 0.f},
		{-.5f, .5f, 0.f},
		/*upper part*/{0.5f, 0.f, .0f},
		{0.46194f, 0.191342f, .0f},
		{0.353553f, 0.353553f, .0f},
		{0.191342f, 0.46194f, .0f},
		{-2.18557e-08f, 0.5f, .0f},
		{-0.191342f, 0.46194f, .0f},
		{-0.353553f, 0.353553f, .0f},
		{-0.46194f, 0.191342f, .0f},
		{-0.5f, -4.37114e-08f, .0f},
		{-0.46194f, -0.191342f, .0f},
		{-0.353553f, -0.353553f, .0f},
		{-0.191342f, -0.46194f, .0f},
		{5.96244e-09f, -0.5f, .0f},
		{0.191342f, -0.46194f, .0f},
		{0.353554f, -0.353553f, .0f},
		{0.46194f, -0.191342f, .0f},
		{0.5f, 8.74228e-08f, .0f},
		{0.7f, 8.74228e-08f, .0f} };
	static const std::initializer_list<glm::vec3> missile_v_data = { {0.f, 0.f, 0.f},
		{.5f, 0.f, 0.f},
		{1.f, 0.f, 0.f } };
	static const std::initializer_list<glm::vec2> missile_texcoord_data = { {0.f, 0.f}, {.5f, 0.f}, {1.f, 0.f} };

	struct PartInfo {
		GLint first;
		GLsizei count;
		glm::vec3 offset;
		AABB aabb;
	};
	struct Model {
		const std::vector<glm::vec3> vertices;
		std::vector<PartInfo> parts;
		const std::vector<glm::vec2> uv;
	};

	enum class Parts { Thruster1, Thruster2, LowerBody, UpperBody, Count };

	struct Assets {

		const std::vector<PartInfo> proto_parts = { {0, 5}, {5, 5}, {10, 7}, {17, 18, { 0.f, 1.1f, 0.f } } };
		const std::vector<PartInfo> missile_parts = { {0, 3} };
		const size_t PROTOX = 0;
		const size_t MISSILE = 1;
		std::vector<Model> models = { {proto_v_data, proto_parts },
			{ missile_v_data, missile_parts, missile_texcoord_data } };
		Assets() {
			// TODO:: calc aabb by parts and store them by parts
			for (auto& m : models)
				for(auto& p : m.parts) 
					p.aabb = CalcAABB(m.vertices, p.first, p.count);
		}
	};
}
#ifdef __EMSCRIPTEN__
#define LOG_ERR(error, msg) emscripten_log(EM_LOG_ERROR, "ERROR: %d : %s\n", error, msg)
#define LOG_INFO(msg) emscripten_log(EM_LOG_CONSOLE, "%s", msg)
#else
#define LOG_ERR(error, msg) std::cerr << "ERROR: " << error << " : " << msg << "\n";
#define LOG_INFO(msg) std::cerr << msg;
#endif

static void errorcb(int error, const char *msg) {
	LOG_ERR(error, msg);
}
class custom_exception : public std::exception {
	const char* _what;
public:
	custom_exception(const char* what) : _what(what) {}
	const char * what() const _NOEXCEPT override { return _what; }
};
void ThrowIf(bool exp, const char* msg) {
	if (exp) throw custom_exception(msg);
}

GLuint LoadShaders(const char* vs, const char* fs) {

	using namespace std;
	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint ProgramID = 0;
	GLint result = GL_FALSE;
	int InfoLogLength;

	glShaderSource(VertexShaderID, 1, &vs, NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (!result) {
		std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		LOG_INFO("Vertex Shader:");
		LOG_ERR(-1, &VertexShaderErrorMessage[0]);
		return 0;
	}

	glShaderSource(FragmentShaderID, 1, &fs, NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (!result) {
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
		LOG_INFO("Fragment Shader:");
		LOG_ERR(-1, &FragmentShaderErrorMessage[0]);
		return 0;
	}

	ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (!result) {
		glDeleteProgram(ProgramID);
		std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		LOG_ERR(-1, &ProgramErrorMessage[0]);
		return 0;
	}

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);
	return ProgramID;
}
// http://gizma.com/easing/
// cubic easing out
// t - current time
// b - start value
// c - change in value
// d - duration
auto easeOutCubic(double t, double b, double c, double d) {
	t /= d;
	t--;
	return c*(t*t*t + 1) + b;
};
auto linearTween(double t, double b, double c, double d) {
	return c*t / d + b;
};
namespace Shader {
	struct Program {
		const char* vs, *fs;
		GLuint id;
		~Program() {
			glDeleteShader(id);
		}
	};
	struct Simple {
		Program program{
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
)",
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
		vec2 rnd_site = texture2D( smp1, (n + o + .5)/uTexSize).xy;
		rnd_site = .5 + sin(uTotal*.001*rnd_site)*.5;
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
		vec2 rnd_site = texture2D( smp1, (n + o + .5)/uTexSize).xy;
		rnd_site = .5 + sin(uTotal*.001*rnd_site)*.5;
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
	//color = vec3(HeatMapColor(smoothstep(.0, .8, res.x ), 0., 1.)) + dis * vec3(1.);
	gl_FragColor = (dis + smoothstep(.0, .8, res.x ))*vec4(1.);
}
)", 0 };
		GLuint uSmp, uElapsed, uTotal, uRes, uMVP, uTexSize;
		void ReloadProgram() {
			if (program.id) glDeleteProgram(program.id);
			program.id = LoadShaders(program.vs, program.fs);
			if (!program.id) throw custom_exception("Shader program compilation/link error");
			uSmp = glGetUniformLocation(program.id, "smp1");
			const GLubyte * err = glewGetErrorString(glGetError());
			uElapsed = glGetUniformLocation(program.id, "uElapsed");
			uTotal = glGetUniformLocation(program.id, "uTotal");
			uRes = glGetUniformLocation(program.id, "uRes");
			uMVP = glGetUniformLocation(program.id, "uMVP");
			uTexSize = glGetUniformLocation(program.id, "uTexSize");
		}
	};

	struct RTShader {
		Program program{
	#ifndef __EMSCRIPTEN__
	"#version 130\n"
	#endif
	R"(
precision highp float;
attribute vec3 pos;
attribute vec2 uv_in;

varying vec2 uv_fs;
void main() {
	gl_Position = vec4(pos, 1.0);
	uv_fs = uv_in;
}
)",
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
varying vec2 uv_fs;
uniform sampler2D uSmp;
uniform vec2 uScreenSize;
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
	vec2 fragment_pos = uScreenSize * uv_fs;
	//vec4 frag1 = texture2D(uSmp, vec2(uv_fs.x, uv_fs.y)),
	//	frag2 = texture2D(uSmp, vec2(uv_fs.x + 1.0, uv_fs.y)),
	//	frag3 = texture2D(uSmp, vec2(uv_fs.x + 1.0, uv_fs.y + 1.0)),
	//	frag4 = texture2D(uSmp, vec2(uv_fs.x, uv_fs.y + 1.0)); //*vec4(uv_fs, 1.0, 1.0);
	//vec4 frag = max(frag1, max(frag2, max(frag3, frag4)));

	vec4 frag1 = texture2D(uSmp, vec2(uv_fs.x, uv_fs.y)),
		frag2 = texture2D(uSmp, vec2(uv_fs.x + 1.0, uv_fs.y)),
		frag3 = texture2D(uSmp, vec2(uv_fs.x + 1.0, uv_fs.y + 1.0)),
		frag4 = texture2D(uSmp, vec2(uv_fs.x, uv_fs.y + 1.0)),
		frag5 = texture2D(uSmp, vec2(uv_fs.x - 1.0, uv_fs.y + 1.0)),
		frag6 = texture2D(uSmp, vec2(uv_fs.x - 1.0, uv_fs.y)),
		frag7 = texture2D(uSmp, vec2(uv_fs.x - 1.0, uv_fs.y - 1.0)),
		frag8 = texture2D(uSmp, vec2(uv_fs.x, uv_fs.y - 1.0)),
		frag9 = texture2D(uSmp, vec2(uv_fs.x + 1.0, uv_fs.y - 1.0));

	vec4 frag = max(frag1, max(frag2, max(frag3, max(frag4, max(frag5, max(frag6, max(frag7, max(frag8, frag9))))))));
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
}
)", 0 };
		GLuint uSmp, uScreenSize;
		RTShader() {
			ReloadProgram();
		}
		void ReloadProgram() {
			if (program.id) glDeleteProgram(program.id);
			program.id = LoadShaders(program.vs, program.fs);
			if (!program.id) throw custom_exception("Shader program compilation/link error");
			uSmp = glGetUniformLocation(program.id, "uSmp");
			uScreenSize = glGetUniformLocation(program.id, "uScreenSize");
		}
	};

	struct Color {
		Program program{
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
)",
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
uniform vec3 uCol;
void main() {
	gl_FragColor = vec4(uCol, 1.0);
}
)", 0 };
		GLuint uMVP, uCol;
		Color() {
			ReloadProgram();
		}
		void ReloadProgram() {
			if (program.id) glDeleteProgram(program.id);
			program.id = LoadShaders(program.vs, program.fs);
			if (!program.id) throw custom_exception("Shader program compilation/link error");
			uMVP = glGetUniformLocation(program.id, "uMVP");
			uCol = glGetUniformLocation(program.id, "uCol");
		}
	};

	struct Texture {
		Program program{
	#ifndef __EMSCRIPTEN__
			"#version 130\n"
	#endif
			R"(
precision mediump float;
attribute vec3 pos;
attribute vec2 uv1_in;

uniform mat4 uMVP;
varying vec2 uv1;

void main() {
	gl_Position = uMVP * vec4(pos, 1.);
	uv1 = uv1_in;
}
)",
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
)", 0 };
		GLuint uSmp, uElapsed, uTotal, uMVP;
		Texture() {
			ReloadProgram();
		}
		void ReloadProgram() {
			if (program.id) glDeleteProgram(program.id);
			program.id = LoadShaders(program.vs, program.fs);
			if (!program.id) throw custom_exception("Shader program compilation/link error");
			uSmp = glGetUniformLocation(program.id, "smp1");
			const GLubyte * err = glewGetErrorString(glGetError());
			uElapsed = glGetUniformLocation(program.id, "uElapsed");
			uTotal = glGetUniformLocation(program.id, "uTotal");
			uMVP = glGetUniformLocation(program.id, "uMVP");
		}
	};
}
class Timer
{
	std::chrono::duration<double> _elapsed;
	std::chrono::time_point<std::chrono::high_resolution_clock> _current, _prev, _start;
public:
	Timer();
	inline void Tick(void) {
		_prev = _current;
		_current = std::chrono::high_resolution_clock::now();
	}
	inline int64_t Elapsed(void) { return std::chrono::duration_cast< std::chrono::milliseconds>(_current - _prev).count(); }
	inline int64_t Total(void) { return std::chrono::duration_cast< std::chrono::milliseconds>(_current - _start).count(); }
};
Timer::Timer() :_current(std::chrono::high_resolution_clock::now()),
_prev(_current),
_start(_current) {};

struct Time {
	double total, frame;
};

struct Mesh {
	glm::vec3 pos;
	glm::mat4 model;
};
struct Camera {
	glm::vec3 pos{ 0.f, 0.f, -10.f };
	glm::mat4 view, proj, vp;
	void SetPos(float x, float y, float z) {
		pos = { x, y, z };
		view = glm::translate({}, pos);
		vp = proj * view;
	}
	void Translate(float x, float y, float z) {
		pos.x += x; pos.y += y; pos.z += z;
		view = glm::translate({}, pos);
		vp = proj * view;
	}
	Camera(int w, int h) : view(glm::translate(glm::mat4{}, pos)),
		proj(glm::ortho((float)(-(w >> 1)), (float)(w>>1), (float)(-(h >> 1)), (float)(h>>1), 0.1f, 100.f)),
		vp(proj * view) {
	}
	void Update(const Time&) {}
	void Tracking(const glm::vec2& tracking_pos) {
		// TODO:: camer atracking bounds
		const float max_x = float(width>>2), top =  float(height >> 2) + 60.f, bottom = -float((height >> 2) + 80.f);
		auto d = tracking_pos + glm::vec2(pos);

		auto dx = max_x + d.x;
		if (d.x < -max_x) {
			pos.x -= d.x + max_x;
		}
		else if (d.x > max_x)
			pos.x -= d.x - max_x;

		auto dy = bottom + d.y;
		if (d.y < bottom) {
			pos.y -= d.y - bottom;
		}
		dy = top + d.y;
		if (d.y > top)
			pos.y -= d.y - top;
		view = glm::translate({}, pos);
		vp = proj * view;
	}
};
class RT {
	Shader::RTShader shader;
	GLuint vao, vbo1, vbo2, txt, rbo, fbo;
	const size_t w = 256, h = 256;
public:
	RT() {
		static const GLfloat g_vertex_buffer_data[] = { -1.0f, -1.0f, 0.0f,
			1.f, -1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 
			1.f, 1.f, 0.0f};
		glGenBuffers(1, &vbo1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo1);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

		static const GLfloat g_uv_buffer_data[] = { 0.f, 0.f,
			1.f, .0f,
			0.f,  1.0f,
			1.f, 1.f};
		glGenBuffers(1, &vbo2);
		glBindBuffer(GL_ARRAY_BUFFER, vbo2);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);
#ifdef VAO_SUPPORT
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo1);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo2);
		glVertexAttribPointer(1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glBindVertexArray(0);
#endif
		glGenTextures(1, &txt);
		glBindTexture(GL_TEXTURE_2D, txt);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Depth texture...
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		//...Depth texture

		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, txt, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	void Set() {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, w, h);
	}
	void Reset() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);
		glDisable(GL_DEPTH_TEST);
	}
	void Render() {
		Reset();
		glEnable(GL_TEXTURE_2D);
		glUseProgram(shader.program.id);
#ifdef VAO_SUPPORT
		glBindVertexArray(vao);
#else
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo1);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo2);
		glVertexAttribPointer(1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);	
#endif
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, txt);
		glUniform1i(shader.uSmp, 0);
		glUniform2f(shader.uScreenSize, (GLfloat)width, (GLfloat)height);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisable(GL_TEXTURE_2D);
#ifndef VAO_SUPPORT
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
#endif
	}
	~RT() {
		glDeleteFramebuffers(1, &fbo);
		glDeleteRenderbuffers(1, &rbo);
		glDeleteBuffers(1, &vbo1);
		glDeleteBuffers(1, &vbo2);
#ifdef VAO_SUPPORT
		glDeleteVertexArrays(1, &vao);
#endif
	}
};

struct Landscape {
	void Update(const Time& t) {}
};

struct ProtoX;
struct Missile {
	static const float size; 
	static const glm::vec3 scale;
	glm::vec3 pos;
	float rot;
	float vel;
	ProtoX* owner;
	Missile& operator=(const Missile&) = default;
	void Update(const Time& t) {
		pos += glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * vel * (float)t.frame;
	}
	bool HitTest(const AABB& bounds, glm::vec3& end) {
		/* AABB intersection
		const glm::vec3 end = glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * size + pos;
		const glm::vec2 min{ std::min(end.x, pos.x), std::min(end.y, pos.y) }, max{ std::max(end.x, pos.x), std::max(end.y, pos.y) };
		const AABB aabb_union{ std::min(min.x, bounds.l), std::max(max.y, bounds.t), std::max(max.x, bounds.r), std::min(min.y, bounds.b)};
		const float xd1 = max.x - min.x, yd1 = max.y - min.y, xd2 = bounds.r - bounds.l, yd2 = bounds.t - bounds.b;
		return xd1 + xd2 >= aabb_union.r - aabb_union.l && yd1 + yd2 >= aabb_union.t - aabb_union.b;*/
		end = glm::vec3{ std::cos(rot), std::sin(rot), 0.f } *size + pos;
		return end.x >= bounds.l && end.x <= bounds.r && end.y >= bounds.b && end.y <= bounds.t;
	}
};
const float Missile::size = 60.f;
const glm::vec3 Missile::scale{ Missile::size, Missile::size, 1.f };

struct ProtoX {
	const size_t id, model_idx;
	AABB aabb;
	const float size = 35.f, max_vel = .3f, max_acc = .0005f, force = .0001f, slowdown = .0003f,
		g = -.000151f, /* m/ms2 */
		ground_level = 40.f;
	glm::vec3 pos{ 0.f, 0.f, 0.f }, col{ 1.f, 0.f, 1.f }, scale{ size, size, 1.f },
		vel{}, acc{ 0.f, g, 0.f };
	const glm::vec3 missile_start_offset;
	struct {
		const double frame_time = 100.; //ms
		bool lthruster = false, rthruster = false, bthruster = false;
		double lt_start = 0., rt_start= 0., bt_start = 0.;
		size_t lt_frame = 0, rt_frame = 0, bt_frame = 0;
	}state;
	struct UpperPart {
		float rot = 0.f;
		const float min_rot = -glm::radians(45.f),
			max_rot = glm::radians(225.f);
		void Update(const Time& t) {
			rot = std::max(min_rot, rot);
			rot = std::min(max_rot, rot);
		}
	}upperPart;
	glm::mat4 rthruster_model = glm::translate({}, glm::vec3{ -.5f, -.3f, 0.f }) * glm::rotate(glm::mat4{}, -glm::half_pi<float>(), { 0.f, 0.f, 1.f }),
		lthruster_model = glm::translate({}, glm::vec3{ .5f, -.3f, 0.f }) * glm::rotate(glm::mat4{}, glm::half_pi<float>(), { 0.f, 0.f, 1.f }),
		bthruster1_model = glm::translate({}, glm::vec3{ -.4f, -.5f, 0.f }),
		bthruster2_model = glm::translate({}, glm::vec3{ .4f, -.5f, 0.f });
	ProtoX(const size_t, const size_t model_idx, const Asset::Model& model) : id(id), model_idx(model_idx),
		missile_start_offset(model.parts[(size_t)Asset::Parts::UpperBody].offset * scale) {
		const auto& lb = model.parts[(size_t)Asset::Parts::LowerBody], &ub = model.parts[(size_t)Asset::Parts::UpperBody];
		aabb = Union(lb.aabb.Translate(lb.offset).Scale(size), ub.aabb.Translate(ub.offset).Scale(size));
	}
	void Shoot(std::vector<Missile>& missiles) {
		
		const float missile_vel = .5f;
		const glm::vec3 missile_vec{ std::cos(upperPart.rot) * missile_vel, std::sin(upperPart.rot) * missile_vel, .0f};
		missiles.push_back({ pos + missile_start_offset, upperPart.rot, glm::length(missile_vec + vel), this });
	}
	void Move(const Time& t, bool lt, bool rt, bool bt) {
		if (state.lthruster != lt)
			state.lt_start = t.total;
		if (state.rthruster != rt)
			state.rt_start = t.total;
		if (state.bthruster != bt)
			state.bt_start = t.total;
		
		state.lthruster = lt;
		state.rthruster = rt;
		state.bthruster = bt;
	}
	void Update(const Time& t, const AABB& bounds) {
		upperPart.Update(t);
		if (state.lthruster) {
			acc.x = std::max(-max_acc, acc.x - force);
			state.lt_frame = size_t(t.total - state.lt_start);
		}
		if (state.rthruster) {
			acc.x = std::min(max_acc, acc.x + force);
			state.rt_frame = size_t(t.total - state.rt_start);
		}
		if (!state.rthruster && !state.lthruster && vel.x != 0.f) {
			if (vel.x<0.f)
				acc.x = slowdown;
			else if (vel.x>0.f)
				acc.x = -slowdown;
		}
		if (state.bthruster) {
			acc.y = std::min(max_acc, acc.y + force);
			state.bt_frame = size_t(t.total - state.bt_start);
		}
		else {
			acc.y = std::max(g, acc.y - force);
		}
		pos += (vel + acc * (float)t.frame / 2.f) * (float)t.frame;
		vel += acc * (float)t.frame;
		vel.x = std::max(-max_vel, std::min(max_vel, vel.x));
		vel.y = std::min(max_vel, vel.y);
		// ground constraint
		if (pos.y <= bounds.b + ground_level) {
			pos.y = bounds.b + ground_level;
			if (std::abs(vel.y) < 0.001f)
				vel.y = 0.f;
			else
				vel = { 0.f, -vel.y / 2.f, 0.f };
		}
		else if (pos.y >= bounds.t) {
			pos.y = bounds.t;
			vel.y = 0.f;
		}

		if (!state.rthruster && !state.lthruster && std::abs(vel.x) < 0.001f) {
			acc.x = 0.f;
			vel.x = 0.f;
		}
	}
	float WrapAround(float min, float max) {
		float dif = pos.x - max;
		if (dif >= 0) {
			pos.x = min + dif;
			return dif;
		}
		dif = pos.x - min;
		if (dif < 0) {
			pos.x = max + dif;
			return dif;
		}
		return 0.f;
	}
};
void GenerateSquare(float x, float y, float s, std::vector<glm::vec3>& data) {
	s *= .5f;
	data.emplace_back(x - s, y - s, 0.f);
	data.emplace_back(x + s, y - s, 0.f);
	data.emplace_back(x - s, y + s, 0.f);

	data.emplace_back(x + s, y - s, 0.f);
	data.emplace_back(x + s, y + s, 0.f);
	data.emplace_back(x - s, y + s, 0.f);
}
struct Renderer {
	const Asset::Assets& assets;
	RT rt;
	Shader::Color colorShader;
	Shader::Texture textureShader;
	static const size_t VBO_PROTOX = 0,
		VBO_MISSILE_UV = 1,
		VBO_LANDSCAPE = 2,
		VBO_MISSILE_VERTEX = 3,
		VBO_AABB = 4,
		VBO_STARFIELD = 5,
		VBO_PARTICLE = 6,
		VBO_COUNT = 7;
	struct {
		size_t verex_count;
		glm::vec3 col{ 1.f, 1.f, 1.f };
		float start_x, end_x;
	}landscape;
	struct Missile{
		GLuint texID;
		~Missile() {
			glDeleteTextures(1, &texID);
		}
	}missile;
	struct Particles {
		static GLuint vbo;
		static GLsizei vertex_count;
		static const size_t count = 100;
		static constexpr float slowdown = .01f, g = .00001f, init_mul = 1.f, max_decay = .0001f, min_decay = .00005f;
		glm::vec3 pos;
		bool kill = false;
		struct Particle {
			glm::vec3 pos, v, col;
			float life, decay;
		};
		std::array<Particle, count> arr;
		Particles(const glm::vec3& pos) : pos(pos) {
			static std::uniform_real_distribution<> col_dist(0., 1.);
			static std::uniform_real_distribution<> rad_dist(.0, glm::two_pi<float>());
			static std::uniform_real_distribution<> decay_dist(min_decay, max_decay);
			static std::uniform_real_distribution<> v_dist(.001f, .05f);
			for (size_t i = 0; i < count; ++i) {
				arr[i].col = { col_dist(mt), col_dist(mt), col_dist(mt) };
				arr[i].pos = {};
				auto r = rad_dist(mt);
				arr[i].v = { std::cos(r) * v_dist(mt) * init_mul, std::sin(r) * v_dist(mt) * init_mul, 0.f };
				arr[i].life = 1.f;
				arr[i].decay = decay_dist(mt);
			}
		}
		void Update(const Time& t) {
			bool kill = true;
			for (size_t i = 0; i < count; ++i) {
				if (arr[i].life <= 0.f) continue;
				kill = false;
				arr[i].pos += arr[i].v * (float)t.frame;
				arr[i].v *= 1.f - arr[i].decay * (float)t.frame;
				arr[i].v.y *= 1.f + g * (float)t.frame;
				//arr[i].col *= arr[i].life;
				arr[i].life -= arr[i].decay * (float)t.frame;
				if (arr[i].life <= 0.01f) arr[i].life = 0.f;
			}
		}
	};
	struct StarField {
		GLuint vbo;
		size_t count;
		struct Layer {
			float z;
			glm::vec3 color;
		}layer1, layer2, layer3;
	}starField;
	GLuint vbo[VBO_COUNT], tex;
#ifdef VAO_SUPPORT
	GLuint vao;
#endif
	static void DumpCircle(const size_t steps = 16) {
		const float ratio = 0.5f;
		for (size_t i = 0; i <= steps; ++i) {
			std::cout << std::cos(glm::two_pi<float>() / steps * i) * ratio << "f, " <<
				std::sin(glm::two_pi<float>() / steps * i) * ratio << "f, .0f,\n";
		}
	}
	Renderer(Asset::Assets& assets) : assets(assets) {
		Init();
	}
	void Init() {
		glGenBuffers(sizeof(vbo) / sizeof(vbo[0]), vbo);
		// ProtoX
		{
			auto& proto = assets.models[assets.PROTOX];
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROTOX]);
			glBufferData(GL_ARRAY_BUFFER, proto.vertices.size() * sizeof(proto.vertices[0]), &proto.vertices.front(), GL_STATIC_DRAW);

#ifdef VAO_SUPPORT
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROTOX]);
			glVertexAttribPointer(0,
				3,
				GL_FLOAT,
				GL_FALSE,
				0,
				(void*)0);
			glBindVertexArray(0);
#else
			glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
		}
		// Landscape
		{
			std::vector<glm::vec3> landscape_data;
			const size_t count = 30;
			const int w = 2048, h = height >> 1, mid = -(h >> 1);
			int lb = -h, rb = 0, start = -(w >> 1) - w;
			landscape.start_x = -(w >> 1);
			int prev = mid;
			landscape_data.emplace_back((float)start, (float)mid, 0.f);
			for (;;) {
				if (start >= -(w >> 1))
					break;
				std::uniform_int_distribution<> dist(lb, rb);
				int rnd = dist(mt);
				start += std::abs(prev - rnd);
				prev = rnd;
				landscape_data.emplace_back((float)start, (float)rnd, 0.f);
			}
			landscape_data.emplace_back((float)(landscape_data.back().x + std::abs(landscape_data.back().y - mid)), (float)mid, 0.f);
			landscape.end_x = -(w >> 1) + landscape_data.back().x - landscape_data.front().x;
			size_t transform_start = landscape_data.size();
			landscape_data.insert(landscape_data.end(), landscape_data.begin(), landscape_data.end());
			float offset = landscape_data.back().x - landscape_data.front().x;
			std::transform(landscape_data.begin() + transform_start, landscape_data.end(), landscape_data.begin() + transform_start, [=](glm::vec3 d) {
				d.x += offset;
				return d;
			});
			transform_start = landscape_data.size();
			landscape_data.insert(landscape_data.end(), landscape_data.begin(), landscape_data.begin() + transform_start);
			std::transform(landscape_data.begin() + transform_start, landscape_data.end(), landscape_data.begin() + transform_start, [&](glm::vec3 d) {
				d.x += offset + offset;
				return d;
			});
			landscape.verex_count = landscape_data.size();
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LANDSCAPE]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * landscape_data.size(), &landscape_data.front(), GL_STATIC_DRAW);
		}

		// Missile
		{
			{
				auto& missile = assets.models[assets.MISSILE];
				glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_UV]);
				glBufferData(GL_ARRAY_BUFFER, missile.uv.size() * sizeof(missile.uv[0]), &missile.uv.front(), GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_VERTEX]);
				glBufferData(GL_ARRAY_BUFFER, missile.vertices.size() * sizeof(missile.vertices[0]), &missile.vertices.front(), GL_STATIC_DRAW);
			}
			std::normal_distribution<> dist(32., 10.);
			const size_t count = 128, size = 32;
			std::vector<GLubyte> texture_data(size * 4);
			std::uniform_int_distribution<> u_dist(0, 32);
			for (size_t i = 0; i < count; ++i) {
				auto rnd = dist(mt);
				size_t idx = (size_t)std::max(0., std::min(double(size - 1), rnd));
				idx *= 4;
				texture_data[idx++] += u_dist(mt);
				texture_data[idx++] += u_dist(mt);
				texture_data[idx++] += u_dist(mt);
				texture_data[idx] = 255;
			}
			
			glGenTextures(1, &missile.texID);
			glBindTexture(GL_TEXTURE_2D, missile.texID);
			GLint fmt, internalFmt;
			fmt = internalFmt = GL_RGBA;
			const unsigned char* p = &texture_data.front();
			glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, size, 1, 0, fmt, GL_UNSIGNED_BYTE, p);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		{
			// AABB
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_AABB]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 5, nullptr, GL_STATIC_DRAW);
		}
		{
			// Starfield
			const float dist_mul_x = 2.f, dist_mul_y = 1.5f, star_size = 3.f;
			const size_t count = 3000;
			std::uniform_real_distribution<> dist_x(-width * dist_mul_x, width * dist_mul_x),
				dist_y(-height * dist_mul_y, height * dist_mul_y);
			std::vector<glm::vec3> data;
			data.reserve(count * 6);
			for (size_t i = 0; i < count; ++i) {
				GenerateSquare(dist_x(mt), dist_y(mt), star_size, data);
			}
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_STARFIELD]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			starField = { vbo[VBO_STARFIELD], count, {-2.f, {1.f, 1.f, 1.f}}, {-3.f,{ 1.f, 0.f, 1.f }}, {-4.f,{ 0.f, 1.f, 1.f } } };
		}
		{
			const float particle_size = 3.f;
			glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo = vbo[VBO_PARTICLE]);
			std::vector<glm::vec3> data;
			GenerateSquare(0.f, 0.f, particle_size, data);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			Particles::vertex_count = data.size();
		}

	}
	void PreRender() {
		//rt.Set();
		glClear(GL_COLOR_BUFFER_BIT);
		glLineWidth(2.f);
	}
	void PostRender() {
		//rt.Render();
		//rt.Reset();
	}
	void DrawBackground(Camera& cam) {
		const auto& shader = colorShader;
		glUseProgram(shader.program.id);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, starField.vbo);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glm::vec3 pos = cam.pos / starField.layer3.z;
		glm::mat4 mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform3f(shader.uCol, starField.layer3.color.r, starField.layer3.color.g, starField.layer3.color.b);
		glDrawArrays(GL_TRIANGLES, 12000, 1000);
		pos = cam.pos / starField.layer2.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform3f(shader.uCol, starField.layer2.color.r, starField.layer2.color.g, starField.layer2.color.b);
		glDrawArrays(GL_TRIANGLES, 6000, 1000);
		pos = cam.pos / starField.layer1.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform3f(shader.uCol, starField.layer1.color.r, starField.layer1.color.g, starField.layer1.color.b);
		glDrawArrays(GL_TRIANGLES, 0, 1000);
		glDisableVertexAttribArray(0);
	}
	void Draw(Camera& cam, std::list<Particles>& particles) {
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const auto& shader = colorShader;
		glUseProgram(shader.program.id);

		for (const auto& p : particles) {
			for (size_t i = 0; i < p.arr.size(); ++i) {
				glUniform3f(shader.uCol, p.arr[i].col.r, p.arr[i].col.g, p.arr[i].col.b);
				glm::mat4 mvp = glm::translate(cam.vp, p.pos + p.arr[i].pos);
				glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
				glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);
			}
		}
		glDisableVertexAttribArray(0);
	}
	void Draw(Camera& cam, const AABB& aabb) {
		std::array<float, 15> vertices{ aabb.l, aabb.t, 0.f,
			aabb.r, aabb.t,  0.f,
			aabb.r, aabb.b, 0.f,
			aabb.l, aabb.b, 0.f,
			aabb.l, aabb.t, 0.f };
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_AABB]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 15, &vertices.front(), GL_STATIC_DRAW);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const auto& shader = colorShader;
		glUseProgram(shader.program.id);
		const glm::mat4& mvp = cam.vp;
		glUniform3f(shader.uCol, 1.f, 1.f, 1.f);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINE_STRIP, 0, 5);
		glDisableVertexAttribArray(0);
	}
	void Draw(Camera& cam, const ProtoX& proto) {
#ifdef VAO_SUPPORT
		glBindVertexArray(vao);
#else
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROTOX]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
#endif
		glUseProgram(colorShader.program.id);
		glm::mat4 mvp = cam.vp, m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) ;
		mvp *= m;
		glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform3f(colorShader.uCol, proto.col.r, proto.col.g, proto.col.b);

		const size_t thruster_frame_count = 2;
		const auto& model = assets.models[proto.model_idx];
		glDrawArrays(GL_LINE_STRIP, model.parts[(size_t)Asset::Parts::LowerBody].first, model.parts[(size_t)Asset::Parts::LowerBody].count);
		mvp = cam.vp;
		m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * glm::translate({}, model.parts[(size_t)Asset::Parts::UpperBody].offset) * glm::rotate(glm::mat4{}, proto.upperPart.rot, { 0.f, 0.f, 1.f });
		mvp *= m;
		glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINE_STRIP, model.parts[(size_t)Asset::Parts::UpperBody].first, model.parts[(size_t)Asset::Parts::UpperBody].count);
		if (proto.state.lthruster) {
			mvp = cam.vp;
			m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.lthruster_model;
			mvp *= m;
			glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			auto frame = (proto.state.lt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
				: model.parts[(size_t)Asset::Parts::Thruster2];
			glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		}
		if (proto.state.rthruster) {
			mvp = cam.vp;
			m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.rthruster_model;
			mvp *= m;
			glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			auto frame = (proto.state.rt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
				: model.parts[(size_t)Asset::Parts::Thruster2];
			glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		}
		if (proto.state.bthruster) {
			mvp = cam.vp;
			m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.bthruster1_model;
			mvp *= m;
			glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			auto frame = (proto.state.bt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
				: model.parts[(size_t)Asset::Parts::Thruster2];
			glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
			mvp = cam.vp;
			m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.bthruster2_model;
			mvp *= m;
			glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		}
#ifdef VAO_SUPPORT
		glBindVertexArray(0);
#else
		glDisableVertexAttribArray(0);
#endif
	}

	void Draw(Camera& cam, const ::Landscape&) {
		glUseProgram(colorShader.program.id);
		glUniform3f(colorShader.uCol, landscape.col.r, landscape.col.g, landscape.col.b);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LANDSCAPE]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glm::mat4 mvp = cam.proj * cam.view;
		glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINE_STRIP, 0, landscape.verex_count);
		glDisableVertexAttribArray(0);

		glUseProgram(0);
	}
	void Draw(Camera& cam, const std::vector<::Missile>& missiles) {
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		auto& shader = textureShader;
		glUseProgram(shader.program.id);
		// vertex data
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_VERTEX]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);

		// uv data
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_UV]);
		glVertexAttribPointer(1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);

		// TODO:: update
		//glUniform1f(textureShader.uTotal, (GLfloat)timer.Total());
		//glUniform1f(textureShader.uElapsed, (GLfloat)timer.Elapsed());
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->missile.texID);
		glUniform1i(textureShader.uSmp, 0);

		for (const auto& missile : missiles) {
			glm::mat4 mvp = cam.proj * cam.view,
				m = glm::translate({}, missile.pos) *
				glm::scale({}, missile.scale) *
				glm::rotate(glm::mat4{}, missile.rot, { 0.f, 0.f, 1.f });
			mvp *= m;
			const auto& model = assets.models[assets.MISSILE];
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glDrawArrays(GL_LINE_STRIP, model.parts[0].first, model.parts[0].count);
		}

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	}
	~Renderer() {
		glDeleteBuffers(sizeof(vbo)/sizeof(vbo[0]), vbo);
		glDeleteTextures(1, &tex);
#ifdef VAO_SUPPORT
		glDeleteVertexArrays(1, &vao);
#endif
	}
};
GLuint Renderer::Particles::vbo;
GLsizei Renderer::Particles::vertex_count;

struct InputHandler {
	enum class Keys{Left, Right, W, A, D, Count};
	enum class ButtonClick{LB, RB, MB};
	static bool keys[(size_t)Keys::Count];
	static double x, y, px, py;
	static bool lb, rb, mb;
	static bool update;
	static std::queue<ButtonClick> event_queue;
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		switch (key) {
		case GLFW_KEY_LEFT:
			keys[(size_t)Keys::Left] = action == GLFW_PRESS || action == GLFW_REPEAT;
			break;
		case GLFW_KEY_RIGHT:
			keys[(size_t)Keys::Right] = action == GLFW_PRESS || action == GLFW_REPEAT;
			break;
		case GLFW_KEY_W:
			keys[(size_t)Keys::W] = action == GLFW_PRESS || action == GLFW_REPEAT;
			break;
		case GLFW_KEY_A:
			keys[(size_t)Keys::A] = action == GLFW_PRESS || action == GLFW_REPEAT;
			break;
		case GLFW_KEY_D:
			keys[(size_t)Keys::D] = action == GLFW_PRESS || action == GLFW_REPEAT;
			break;
		}
	}
	static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
	{
		if (!update)
			px = x; py = y;
		update = true;
		x = xpos; y = ypos;
	}
	static void cursor_enter_callback(GLFWwindow* window, int entered)
	{
		if (entered){
			glfwGetCursorPos(window, &x, &y);
			px = x; py = y;
		}
	}
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (!lb)
				event_queue.push(ButtonClick::LB);
			lb = action == GLFW_PRESS;
		}
		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (!rb)
				event_queue.push(ButtonClick::RB);
			rb = action == GLFW_PRESS;
		}
		if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
			if (!mb)
				event_queue.push(ButtonClick::MB);
			mb = action == GLFW_PRESS;
		}
	}
	InputHandler() {
		glfwSetKeyCallback(window, key_callback);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, cursor_pos_callback);
		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetCursorEnterCallback(window, cursor_enter_callback);
	}
	static void Reset() {
		update = false;
		while (!event_queue.empty()) event_queue.pop();
	}
};
bool InputHandler::keys[(size_t)Keys::Count]{};
bool InputHandler::lb = false, InputHandler::rb = false,InputHandler::mb = false;
double InputHandler::x, InputHandler::y, InputHandler::px, InputHandler::py;
bool InputHandler::update = false;
std::queue<InputHandler::ButtonClick> InputHandler::event_queue;
// TODO:: hack
class Scene;
static Scene* scene;
class Scene {
public:
	AABB bounds;
	Shader::Simple simple;
	Asset::Assets assets;
	Renderer renderer;
	ProtoX player;
	std::vector<Missile> missiles;
	std::vector<ProtoX> players;
	std::list<Renderer::Particles> particles;
	Landscape landscape;
	Mesh mesh{ { 5.f, 0.f, 0.f } };
	Camera camera{ width, height };
	InputHandler inputHandler;
	Timer timer;
	GLuint VertexArrayID;
	GLuint vertexbuffer;
	GLuint uvbuffer;
	GLuint texID;
	GLuint uTexSize;
	glm::mat4x4 mvp;
	GLuint GenTexture(size_t w, size_t h) {
		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		GLint fmt, internalFmt;
		fmt = internalFmt = GL_RGB;
		std::vector<unsigned char> pv(w*h*3);
		unsigned char r, g, b;
		r = g = b = 0;
		static std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<uint32_t> dist(0, 255);
		for (size_t i = 0; i < w*h * 3;i+=3) {
			pv[i] = dist(mt);
			pv[i + 1] = dist(mt);
			pv[i + 2] = dist(mt);
		}
		const unsigned char* p = &pv.front();
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, p);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glGenerateMipmap(GL_TEXTURE_2D);
		return textureID;
	}
public:
	Scene() : renderer(assets), player(0xdead/* TODO:: generate id*/, assets.PROTOX, assets.models[assets.PROTOX]) {

		bounds.l = renderer.landscape.start_x;
		bounds.r = renderer.landscape.end_x;
		bounds.t = float((height >> 1) + (height >> 2));
		bounds.b = -float((height >> 1) + (height >> 2));
		players.emplace_back(0xbeef/* TODO:: generate id*/, assets.PROTOX, assets.models[assets.PROTOX]);
		auto& p = players.back();
		p.pos.x = 100.f;
		p.col = { .0f, 1.f, 1.f };

		texID = GenTexture(texw, texh);
		mesh.model = glm::translate(mesh.model, mesh.pos);
		auto mvp = camera.proj * camera.view * mesh.model;

		glm::vec4 v{ -1.0f, -1.0f, 0.0f, 1.f };
		glm::vec4 res1 = v * mvp, res2 = mvp * v;

		static const GLfloat g_vertex_buffer_data[] = { -1.0f, -1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,
			0.0f,  1.0f, 0.0f };

		glGenBuffers(1, &vertexbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

		static const GLfloat g_uv_buffer_data[] = { 0.f, 0.f,
			1.f, .0f,
			.5f,  1.0f };

		glGenBuffers(1, &uvbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);
		//glBindVertexArray(0);

		//glGenVertexArrays(1, &VertexArrayID);
		//glBindVertexArray(VertexArrayID);
		//glEnableVertexAttribArray(0);
		//glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		//glVertexAttribPointer(0,
		//	3,
		//	GL_FLOAT,
		//	GL_FALSE,
		//	0,
		//	(void*)0);
		//glEnableVertexAttribArray(1);
		//glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		//glVertexAttribPointer(1,
		//	2,
		//	GL_FLOAT,
		//	GL_FALSE,
		//	0,
		//	(void*)0);
		//glBindVertexArray(0);
		////glDisableVertexAttribArray(1);
		////glDisableVertexAttribArray(0);
	}
	void Render() {
		renderer.PreRender();
		//glUseProgram(simple.program.id);

		//glUniformMatrix4fv(simple.uMVP, 1, GL_FALSE, &mvp[0][0]);

		//glUniform2f(simple.uRes, (GLfloat)width, (GLfloat)height);

		//glUniform1f(simple.uTotal, (GLfloat)timer.Total());
		//glUniform1f(simple.uElapsed, (GLfloat)timer.Elapsed());
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, texID);
		//glUniform1i(simple.uSmp, 0);
		//glUniform2f(simple.uTexSize, (GLfloat)texw, (GLfloat)texh);

		//glBindVertexArray(VertexArrayID);

		//glEnableVertexAttribArray(0);

		//glDrawArrays(GL_TRIANGLES, 0, 3);
	
		//glBindVertexArray(0);
		renderer.DrawBackground(camera);
		renderer.Draw(camera, landscape);
		for (const auto& p : players) {
			renderer.Draw(camera, p);
			renderer.Draw(camera, p.aabb.Translate(p.pos));
		}
		renderer.Draw(camera, player);
		renderer.Draw(camera, missiles);
		renderer.Draw(camera, particles);
		renderer.PostRender();
	}
	void Update(const Time& t) {
		const double scroll_speed = .5, // px/s
			rot_ratio = .002;
		if (inputHandler.keys[(size_t)InputHandler::Keys::Left])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.keys[(size_t)InputHandler::Keys::Right])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.update)
			player.upperPart.rot += float((inputHandler.px - inputHandler.x) * rot_ratio);

		player.Move(t, inputHandler.keys[(size_t)InputHandler::Keys::A],
			inputHandler.keys[(size_t)InputHandler::Keys::D],
			inputHandler.keys[(size_t)InputHandler::Keys::W]);
		while (!inputHandler.event_queue.empty()) {
			auto e = inputHandler.event_queue.back();
			inputHandler.event_queue.pop();
			switch(e) {
			case InputHandler::ButtonClick::LB:
				player.Shoot(missiles);
				break;
			}
		}
		player.Update(t, bounds);
		for (auto& p : players) {
			p.Update(t, bounds);
		}
		for (auto& m : missiles) {
			m.Update(t);
		}
		auto d = camera.pos.x + player.pos.x;
		auto res = player.WrapAround(renderer.landscape.start_x, renderer.landscape.end_x);
		camera.Update(t);
		camera.Tracking(glm::vec2{ player.pos });
		// wraparound camera hack
		// wrap around from left
		if (res < 0)
			camera.Translate(std::max(d, -float(width >> 2)) - (width >> 2), 0.f, 0.f);
		else if (res > 0)
			camera.Translate(std::min(d, float(width >> 2)) + (width >> 2), 0.f, 0.f);


		auto it = missiles.begin();
		while (it != missiles.end()) {
			auto& m = *it;
			if (m.pos.x < bounds.l ||
				m.pos.x > bounds.r ||
				m.pos.y < bounds.b ||
				m.pos.y > bounds.t) {
				if (&m != &missiles.back()) {
					m = missiles.back();
					missiles.pop_back();
				}
				else {
					missiles.pop_back();
					break;
				}
			}
			else {
				bool last = false;
				for (const auto& p : players) {
					glm::vec3 hit_pos;
					if (m.HitTest(p.aabb.Translate(p.pos), hit_pos)) {
						particles.push_back({ hit_pos });
						if (&m != &missiles.back()) {
							m = missiles.back();
							missiles.pop_back();
						}
						else {
							missiles.pop_back();
							last = true;
							break;
						}
					}
				}
				if (last) break;
				++it;
			}
		}
		for (auto it = particles.begin(); it != particles.end();) {
			it->Update(t);
			if (it->kill) {
				auto temp = it;
				++it;
				particles.erase(temp);
			}
			else ++it;
		}
	}
	~Scene() {
		glDeleteBuffers(1, &vertexbuffer);
		glDeleteBuffers(1, &uvbuffer);
		glDeleteTextures(1, &texID);
		//glDeleteVertexArrays(1, &VertexArrayID);
	}
};

void init() {
	glfwSetErrorCallback(errorcb);
	ThrowIf(glfwInit() != GL_TRUE, "glfw init failed");
	
#ifdef __EMSCRIPTEN__
	assert(!strcmp(glfwGetVersionString(), "3.0.0 JS WebGL Emscripten"));
#else
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif
	window = glfwCreateWindow(width, height, "WebGL Test", NULL, NULL);
	glfwSetWindowPos(window, 900, 100);
	ThrowIf(window == NULL, "Can't create window");
	glfwMakeContextCurrent(window); // stub 
	ThrowIf(glfwGetCurrentContext() != window, "Can't set current context");
	glfwSwapInterval(1);

	glewExperimental = true;
	ThrowIf(glewInit() != GLEW_OK, "glew init failed");
	ThrowIf(glewGetString(0) != NULL, (const char*)glewGetString(0));

	glClearColor(0.f, .0f, .0f, 1.f);
#ifdef REPORT_RESULT  
	int result = 1;
	REPORT_RESULT();
#endif
}
Timer timer;
void main_loop();
int main() {
	//Renderer::DumpCircle();
	init();
	Scene scene;
	// TODO:: hack
	::scene = &scene;
#ifdef __EMSCRIPTEN__
	// void emscripten_set_main_loop(em_callback_func func, int fps, int simulate_infinite_loop);
	emscripten_set_main_loop(main_loop, 0/*60*/, 1);
#else
	do {
		main_loop();
		glfwSwapBuffers(window);
		glfwPollEvents();
	} while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);
	glfwDestroyWindow(window);
	glfwTerminate();
#endif
	return 0;
}

void main_loop() {
	auto a = timer.Elapsed();
	scene->Update({(double)timer.Total(), (double)timer.Elapsed()});
	scene->Render();
	timer.Tick();
	InputHandler::Reset();
}