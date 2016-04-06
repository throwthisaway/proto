#include <GL/glew.h>
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
#include <glm/gtx/rotate_vector.hpp>
#include <chrono>

#include <stdio.h>
#include <vector>
#include <array>
#include <queue>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include "../../MeshLoader/MeshLoader.h"
#include "../emc_socket/Socket.h"
#include <GLFW/glfw3.h>
#include <inttypes.h>
//#define VAO_SUPPORT
#define DEBUG_REL
template<typename T>
constexpr size_t ID5(const T& t, size_t offset) {
	return ((t[offset + 4] - '0') << 20) | ((t[offset + 3] - '0') << 15) | ((t[offset + 2] - '0') << 10) | ((t[offset + 1] - '0') << 5) | (t[offset] - '0');
}

constexpr std::array<unsigned char, 5> ID5(size_t id) {
	return{(unsigned char)((id & 31) + '0'), (unsigned char)(((id>>5)&31) + '0'), (unsigned char)(((id>>10)&31) + '0'), (unsigned char)(((id >> 15) & 31) + '0'), (unsigned char)(((id >> 20) & 31) + '0') };
}
template<typename T>
constexpr size_t Tag(const T& t) {
	return (t[3] << 24) | (t[2] << 16) | (t[1] << 8) | t[0];
}
inline glm::vec3 RotateZ(const glm::vec3& v, const glm::vec3& c, float r) {
	//return{std::cos(r) * (v.x - c.x), std::sin(r) * (v.y - c.y), 0.f };
	return glm::rotateZ(v - c, r);
}
class Scene;
struct {
#ifdef DEBUG_REL
	const float invincibility = 0.f;
#else
	const float invincibility = 5000.f;
#endif
	std::string host = "localhost";
	unsigned short port = 8000;
	int width = 640, height = 480;
	std::string sessionID;
	std::unique_ptr<Scene> scene;
	std::unique_ptr<Client> ws;
}globals;
class exception : std::exception {
public:
	exception(const std::string& message) : message(message) {}
	const std::string message;
};
static const size_t texw = 256, texh = 256;
static GLFWwindow * window;
static std::random_device rd;
static std::mt19937 mt(rd());
static void ReadMeshFile(const char* fname, MeshLoader::Mesh& mesh) {
	FILE *f;
#ifdef __EMSCRIPTEN__
	f = ::fopen(fname, "rb");
#else
	::fopen_s(&f, fname, "rb");
#endif
	if (!f) {
		std::stringstream ss;
		ss << "Can't open file: " << fname;
		throw exception(ss.str());
	}
	::fseek(f, 0, SEEK_END);
	auto fpos = ::ftell(f);
	std::vector<char> data((size_t)fpos);
	::fseek(f, 0, SEEK_SET);
	::fread(&data.front(), 1, (size_t)fpos, f);
	::fclose(f);
	LoadMesh(&data.front(), data.size(), mesh);
}
struct AABB {
	float l, t, r, b;
	//AABB(const AABB&) = default;
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
	const float missile_size = 120.f;
	struct Layer {
		struct Surface {
			GLint first;
			GLsizei count;
			glm::vec3 col;
		};
		glm::vec3 pivot;
		std::vector<Surface> parts;
		AABB aabb;
	};
	struct Model {
		std::vector<glm::vec3> vertices;
		std::vector<Layer> layers;
		std::vector<glm::vec2> uv;
		AABB aabb;
	};

	//enum class Parts { Thruster1, Thruster2, LowerBody, UpperBody, Count };
	std::vector<glm::vec2> GenLineNormals(const std::vector<glm::vec3>& vertices,
		std::vector<MeshLoader::PolyLine>& lines,
		float lineWidth) {
		lineWidth /= 2.f;
		std::vector<glm::vec2> lineNormals;
		lineNormals.reserve(lines.size());
		std::vector<std::vector<size_t>> lineIndices(vertices.size());
		size_t idx = 0;
		for (const auto& l : lines) {
			lineIndices[l.v1].push_back(idx);
			lineIndices[l.v2].push_back(idx);
			++idx;
		}
		// change line orientations
		std::vector<bool> swapped(lines.size());
		bool restart = true;
		while (restart) {
			restart = false;
			for (auto& l : lineIndices) {
				if (l.size() <= 1) continue;
				if (lines[l.front()].v1 == lines[l.back()].v1 || lines[l.front()].v2 == lines[l.back()].v2)
				{
					size_t swap_line_index = (swapped[l.back()]) ? l.front() : l.back();
					assert(!(swapped[l.back()] && swapped[l.front()]));
					std::swap(lines[swap_line_index].v1, lines[swap_line_index].v2);
					swapped[swap_line_index] = true;
					restart = true;
				}
			}
		}
		idx = 0;
		
		for (const auto& l : lines) {
			auto v = glm::normalize(glm::vec2{ vertices[l.v2] } -glm::vec2{ vertices[l.v1] });
			lineNormals.emplace_back(v.y, -v.x);
			++idx;
		}

		idx = 0;
		std::vector<glm::vec2> vertexNormals(lineIndices.size());
		for (const auto& line_index : lineIndices) {
			if (line_index.empty()) {
				++idx;
				continue;
			}
			auto n1 = lineNormals[line_index.front()];
			if (line_index.size() > 1) {
				n1 = glm::normalize(n1 + lineNormals[line_index.back()]);
				// preserve the thickness on joints
				const auto& l = lines[line_index.back()];
				auto v = glm::normalize(glm::vec2{ vertices[l.v2] } -glm::vec2{ vertices[l.v1] });
				auto f = glm::dot(v, n1);	// cos(angle) between normal and the line
				auto d = glm::distance(v * f, n1); // distancve between the normal and the normal projected onto the line
				n1 = n1 / d; // grow normal the preserve the distance between the normal endpoint and the line
				assert(!(lines[line_index.front()].v1 == lines[line_index.back()].v1 || lines[line_index.front()].v2 == lines[line_index.back()].v2));
			}
			vertexNormals[idx++] = n1 * lineWidth;
		}
		return vertexNormals;
	}
	void GenVertices(const MeshLoader::PolyLine& l,
		const std::vector<glm::vec2>& vertexNormals,
		const std::vector<glm::vec3>& mesh_vertices,
		float scale,
		std::vector<glm::vec3>& vertices) {
		auto v1 = mesh_vertices[l.v1], v2 = mesh_vertices[l.v2];
		glm::vec3 n11(vertexNormals[l.v1], v1.z), n12{ -n11.x, -n11.y, v1.z },
			n21(vertexNormals[l.v2], v2.z), n22{ -n21.x, -n21.y, v2.z };
		vertices.push_back(v1 * scale + n11);
		vertices.push_back(v2 * scale + n21);
		vertices.push_back(v2 * scale + n22);
		vertices.push_back(v1 * scale + n11);
		vertices.push_back(v2 * scale + n22);
		vertices.push_back(v1 * scale + n12);
	}

	Model Reconstruct(MeshLoader::Mesh& mesh, float scale, float lineWidth = 3.f) {
		std::vector<glm::vec3> vertices;
		size_t count, start;
		std::vector<Layer> layers;
		std::vector<glm::vec2> vertexNormals = GenLineNormals(mesh.vertices, mesh.lines, lineWidth);
		layers.reserve(mesh.layers.size());
		for (const auto& layer : mesh.layers) {
			layers.push_back({ glm::vec3{ layer.pivot.x, layer.pivot.y, layer.pivot.z } * scale });
			auto& layerInfo = layers.back();
			for (size_t section = 0; section < layer.poly.n; ++section) {
				auto end = layer.poly.sections[section].start + layer.poly.sections[section].count;
				start = vertices.size(); count = 0;
				for (size_t poly = layer.poly.sections[section].start; poly < end; ++poly) {
					const auto& p = mesh.polygons[poly];
					vertices.push_back(mesh.vertices[p.v[0]] * scale);
					vertices.push_back(mesh.vertices[p.v[2]] * scale);
					vertices.push_back(mesh.vertices[p.v[1]] * scale);
					count += 3;
				}
				if (count)
					layerInfo.parts.push_back({ GLint(start), GLsizei(count),
						glm::vec3{ mesh.surfaces[layer.poly.sections[section].index].color[0],
						mesh.surfaces[layer.poly.sections[section].index].color[1],
						mesh.surfaces[layer.poly.sections[section].index].color[2] } });
			}

			for (size_t section = 0; section < layer.line.n; ++section) {
				auto end = layer.line.sections[section].start + layer.line.sections[section].count;
				start = vertices.size(); count = 0;
				for (size_t line = layer.line.sections[section].start; line < end; ++line) {
					const auto& l = mesh.lines[line];
					GenVertices(l, vertexNormals, mesh.vertices, scale, vertices);
					count += 3 * 2;
				}
				if (count)
					layerInfo.parts.push_back({ GLint(start), GLsizei(count),
					   glm::vec3{ mesh.surfaces[layer.line.sections[section].index].color[0],
						mesh.surfaces[layer.line.sections[section].index].color[1],
						mesh.surfaces[layer.line.sections[section].index].color[2] } });
			}
		}

		return { vertices, layers };
	}

	Model Reconstruct(const std::vector<glm::vec3>& mesh_vertices,
		std::vector<MeshLoader::PolyLine>& lines,
		const std::vector<glm::vec2>& mesh_texcoord, float scale = 1.f, float lineWidth = 3.f) {
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec2> texcoord;
		std::vector<glm::vec2> vertexNormals = GenLineNormals(mesh_vertices, lines, lineWidth);
		for (const auto& l : lines) {
			GenVertices(l, vertexNormals, mesh_vertices, scale, vertices);
			// order should correspond to the vertex order of triangle generation
			texcoord.push_back(mesh_texcoord[l.v1]);
			texcoord.push_back(mesh_texcoord[l.v2]);
			texcoord.push_back(mesh_texcoord[l.v2]);

			texcoord.push_back(mesh_texcoord[l.v1]);
			texcoord.push_back(mesh_texcoord[l.v2]);
			texcoord.push_back(mesh_texcoord[l.v1]);
		}
		
		return{ vertices,{ { {},{ Asset::Layer::Surface{ GLint(0), GLsizei(vertices.size()) , { 1.f, 1.f, 1.f } } } } }, texcoord };
	}

	Model ExtractLines(MeshLoader::Mesh& mesh, float scale = 1.f, float lineWidth = 3.f) {
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec2> vertexNormals = GenLineNormals(mesh.vertices, mesh.lines, lineWidth);
		for (const auto& l : mesh.lines) {
			GenVertices(l, vertexNormals, mesh.vertices, scale, vertices);
		}
		return{ vertices,{ { {},{ Asset::Layer::Surface{ GLint(0), GLsizei(vertices.size()), {1.f, 1.f, 1.f} } } } } };
	}

	struct Assets {
		std::vector<Model*> models;
		Model probe, propulsion, land, missile, debris;
		Assets() {
#ifdef __EMSCRIPTEN__
#define PATH_PREFIX ""
#else
#define PATH_PREFIX "..//..//emc_ogl//"
#endif
			const float scale = 40.f, propulsion_scale = 80.f;
			{
				const std::vector<glm::vec3> vertices{ { 0.f, 0.f, 0.f },
				{ 1.f,0.f, 0.f },
				{ 1.f, -1.f, 0.f } };
				std::vector<MeshLoader::PolyLine> lines{ { 0, 1 },{ 2, 1 } };
				const std::vector<glm::vec2> texcoord{ { 0.f, 0.f },{ .5f, 0.f },{ 1.f, 0.f } };
				missile = Reconstruct(vertices, lines, texcoord, scale);
				models.push_back(&missile);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//proto.mesh", mesh);
				
				
#ifdef __EMSCRIPTEN__
				emscripten_log(EM_LOG_CONSOLE, " %f %f %f, %f %f %f", mesh.surfaces[0].color[0],
					mesh.surfaces[0].color[1],
					mesh.surfaces[0].color[2],
					mesh.surfaces[1].color[0],
					mesh.surfaces[1].color[1],
					mesh.surfaces[1].color[2]);
#endif
				probe = Reconstruct(mesh, scale);
				models.push_back(&probe);
				
				debris = ExtractLines(mesh, scale);
				models.push_back(&debris);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//propulsion.mesh", mesh);
				propulsion = Reconstruct(mesh, propulsion_scale);
				models.push_back(&propulsion);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//land.mesh", mesh);
				land = Reconstruct(mesh, scale);
				models.push_back(&land);
			}
			{
				const std::vector<glm::vec3> vertices{ { 0.f, 0.f, 0.f },
				{  0.5f, 0.f, 0.f },
				{ 1.f, 0.f, 0.f } };
				std::vector<MeshLoader::PolyLine> lines{ {0, 1}, {1, 2}};
				const std::vector<glm::vec2> texcoord { { 0.f, 0.f },{ .5f, 0.f},{ 1.f, 0.f} };
				missile = Reconstruct(vertices, lines, texcoord, missile_size, 10.f);
				models.push_back(&missile);
			}
			for (auto& m : models) {
				for (auto& l : m->layers) {
					l.aabb = CalcAABB(m->vertices, l.parts.front().first, l.parts.front().count);
					for (size_t i = 1; i < l.parts.size(); ++i) {
						auto aabb = CalcAABB(m->vertices, l.parts[i].first, l.parts[i].count);
						l.aabb = Union(l.aabb, aabb);
					}
				}

				m->aabb = m->layers.front().aabb;
				for (size_t i = 1; i < m->layers.size(); ++i)
					m->aabb = Union(m->aabb, m->layers[i].aabb);
			}
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
uniform vec4 uCol;
void main() {
	gl_FragColor = uCol;
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

	struct ColorPosAttrib {
		Program program{
#ifndef __EMSCRIPTEN__
			"#version 130\n"
#endif
			R"(
precision highp float;
attribute vec3 v, pos;
uniform mat4 uMVP;
void main() {
	gl_Position = uMVP * vec4(v + pos, 1.0);
}
)",
#ifndef __EMSCRIPTEN__
"#version 130\n"
#endif
R"(
precision highp float;
uniform vec4 uCol;
void main() {
	gl_FragColor = uCol;
}
)", 0 };
		GLuint uMVP, uCol;
		ColorPosAttrib() {
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

struct Object {
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
	void Tracking(const glm::vec3& tracking_pos, const AABB& scene_aabb, const AABB& player_aabb) {
		// TODO:: camera tracking bounds
		const float max_x = float(globals.width>>2), top =  float(globals.height >> 2), bottom = -float(globals.height >> 2);
		auto d = tracking_pos + pos;

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
		glViewport(0, 0, globals.width, globals.height);
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
		glUniform2f(shader.uScreenSize, (GLfloat)globals.width, (GLfloat)globals.height);

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

struct ProtoX;
struct Missile {
	glm::vec3 pos;
	float rot;
	float vel;
	ProtoX* owner;
	size_t id;
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
		end = glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * Asset::missile_size + pos;
		return end.x >= bounds.l && end.x <= bounds.r && end.y >= bounds.b && end.y <= bounds.t;
	}
	bool IsOutOfBounds(const AABB& bounds) {
		return pos.x < bounds.l ||
			pos.x > bounds.r ||
			pos.y < bounds.b ||
			pos.y > bounds.t;
	}
};
struct Plyr {
	size_t tag, id;
	float x, y, rot, invincible;
};
struct Misl {
	size_t tag, player_id, missile_id;
	float x, y, rot, vel;
};
struct Scor {
	size_t tag, owner_id, target_id, missile_id;
	float x, y;
};
struct Kill {
	size_t tag, client_id;
};
struct ProtoX {
	const size_t id;
	AABB aabb;
	Client *ws;
	const Asset::Model& debris_ref;
	Asset::Model debris;
	const float max_vel = .3f,
		m = 500.f,
		force = .1f,
		slowdown = .0003f,
		g = -.1f, /* m/ms2 */
		ground_level = 20.f,
		blink_rate = 50.f, // ms
		blink_duration = globals.invincibility,  // ms
		fade_out_time = 1500.f, // ms
		debris_speed = .1f, //px/ ms
		debris_centrifugal_speed = .01f; // rad/ms
	// state...
	float invincible, blink_time, fade_out, hit_time;
	bool visible, hit, killed;
	glm::vec3 pos, vel, f, hit_pos;
	const glm::vec3 missile_start_offset;
	const Asset::Layer& layer;
	size_t score = 0, missile_id = 0;
	struct Propulsion {
		const glm::vec3 pos;
		const float rot;
		const size_t frame_count;
		bool on = false;
		const double frame_time = 100.; //ms
		double elapsed = 0.;
		size_t frame;
		void Update(const Time& t) {
			elapsed += t.frame;
			if (elapsed >= frame_time) {
				++frame;
				if (frame >= frame_count) frame = 0;
				elapsed -= frame_time;
			}
		}
		void Set(bool on) {
			if (this->on == on)
				return;
			this->on = on;
			if (on) {
				frame = 0;
				elapsed = 0.;
			}
		}
		Propulsion(const glm::vec3& pos, float rot, size_t frame_count) :
			pos(pos), rot(rot), frame_count(frame_count) {}
	}left, bottom, right;
	struct Turret {
		float rot = 0.f;
		const float rest_pos = glm::half_pi<float>(),
			min_rot = -glm::radians(45.f) - rest_pos,
			max_rot = glm::radians(225.f) - rest_pos;
		const Asset::Layer& layer;
		Turret(const Asset::Layer& layer) :layer(layer) {}
		void Update(const Time& t) {
			rot = std::max(min_rot, rot);
			rot = std::min(max_rot, rot);
		}
	}turret;
	ProtoX(const size_t id, const Asset::Model& model, const Asset::Model& debris, size_t frame_count, Client* ws = nullptr) : id(id),
		aabb{model.aabb},
		ws(ws),
		debris_ref(debris),
		missile_start_offset(model.layers[model.layers.size() - 1].pivot),
		layer(model.layers.front()),
		left({ 25.f, 15.f, 0.f }, glm::half_pi<float>(), frame_count),
		right({ -25.f, 15.f, 0.f }, -glm::half_pi<float>(), frame_count),
		bottom({}, 0.f, frame_count),
		turret(model.layers.back()) {
		Init();
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "Player connected");
#endif
		
	}
	void Shoot(std::vector<Missile>& missiles) {
		const float missile_vel = 1.f;
		auto rot = turret.rest_pos + turret.rot;
		const glm::vec3 missile_vec{ std::cos(rot) * missile_vel, std::sin(rot) * missile_vel, .0f};
		missiles.push_back({ pos + missile_start_offset, rot, glm::length(missile_vec), this, ++missile_id });
		if (ws) {
			auto& m = missiles.back();
			Misl misl{ Tag("MISL"), id, m.id, m.pos.x, m.pos.y, m.rot, m.vel};
			globals.ws->Send((char*)&misl, sizeof(misl));
		}
	}
	void Move(const Time& t, bool lt, bool rt, bool bt) {
		left.Set(lt);
		right.Set(rt);
		bottom.Set(bt);
		f.x = (lt) ? -force : (rt) ? force : 0.f;
		f.y = (bt) ? force : g;
	}
	void Init() {
		invincible = blink_duration; blink_time = blink_rate; fade_out = fade_out_time;
		visible = true; hit = killed = false;
	}
	void Kill(const glm::vec3& hit_pos, double hit_time) {
		if (invincible > 0.f || hit || killed) return;
		hit = true;
		this->hit_pos = hit_pos - pos;
		this->hit_time = hit_time;
		debris = debris_ref;
	}
	void Update(const Time& t, const AABB& bounds) {
		if (invincible > 0.f) {
			blink_time -= (float)t.frame;
			if (blink_time < 0.f) {
				visible = !visible;
				blink_time += blink_rate;
			}
			invincible -= (float)t.frame;
			if (invincible <= 0.f) {
				blink_time = blink_rate;
				invincible = 0.f;
				visible = true;
			}
		}
		else if (hit) {
			auto fade_time = (float)(t.total - hit_time) / fade_out_time;
			fade_out = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
			if (killed = fade_out <= 0.0001f) return;
			// TODO:: refactor to continuous fx instead of incremental
			for (size_t i = 0; i < debris.vertices.size(); i += 6) {
				// TODO:: only enough to have the average of the furthest vertices
				auto center = (debris.vertices[i] + debris.vertices[i + 1] + debris.vertices[i + 2] +
					debris.vertices[i + 3] + debris.vertices[i + 4] + debris.vertices[i + 5]) / 6.f;
				auto v = center - hit_pos;
				float len = glm::length(v);
				v /= len;
				v *= debris_speed * (float)t.frame;
				//v.y += g * (float)t.frame;
				auto incr = debris_centrifugal_speed * (float)t.frame;
//				auto r = RotateZ(debris.vertices[i], center, incr);
				debris.vertices[i] += v;
				debris.vertices[i] = center + RotateZ(debris.vertices[i], center, incr);
				debris.vertices[i + 1] += v;
				debris.vertices[i + 1] = center + RotateZ(debris.vertices[i + 1], center, incr);
				debris.vertices[i + 2] += v;
				debris.vertices[i + 2] = center + RotateZ(debris.vertices[i + 2], center, incr);
				debris.vertices[i + 3] += v;
				debris.vertices[i + 3] = center + RotateZ(debris.vertices[i + 3], center, incr);
				debris.vertices[i + 4] += v;
				debris.vertices[i + 4] = center + RotateZ(debris.vertices[i + 4], center, incr);
				debris.vertices[i + 5] += v;
				debris.vertices[i + 5] = center + RotateZ(debris.vertices[i + 5], center, incr);
			}
			return;
		}
		left.Update(t);
		right.Update(t);
		bottom.Update(t);
		turret.Update(t);
		vel += (f / m) * (float)t.frame;
		pos += vel * (float)t.frame;
		vel.x = std::max(-max_vel, std::min(max_vel, vel.x));
		vel.y = std::max(-max_vel, std::min(max_vel, vel.y));
		// ground constraint
		if (pos.y + aabb.b <= bounds.b + ground_level) {
			pos.y = bounds.b + ground_level - aabb.b;
			if (std::abs(vel.y) < 0.001f)
				vel.y = 0.f;
			else
				vel = { 0.f, -vel.y / 2.f, 0.f };
		}
		else if (pos.y + aabb.t >= bounds.t) {
			pos.y = bounds.t - aabb.t;
			vel.y = 0.f;
		}
		if (ws) {
			Plyr player{ Tag("PLYR"), id, pos.x, pos.y, turret.rot, invincible };
			globals.ws->Send((char*)&player, sizeof(Plyr));
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
const glm::vec4 colors[] = { { 1.0f,0.5f,0.5f , 1.f },{ 1.0f,0.75f,0.5f , 1.f },{ 1.0f,1.0f,0.5f , 1.f },{ 0.75f,1.0f,0.5f , 1.f },
{ 0.5f,1.0f,0.5f , 1.f },{ 0.5f,1.0f,0.75f , 1.f },{ 0.5f,1.0f,1.0f , 1.f },{ 0.5f,0.75f,1.0f , 1.f },
{ 0.5f,0.5f,1.0f , 1.f },{ 0.75f,0.5f,1.0f , 1.f },{ 1.0f,0.5f,1.0f , 1.f },{ 1.0f,0.5f,0.75f , 1.f } };

struct Renderer {
	const Asset::Assets& assets;
	RT rt;
	Shader::Color colorShader;
	Shader::ColorPosAttrib colorPosAttribShader;
	Shader::Texture textureShader;
	static const size_t VBO_PROTOX = 0,
		VBO_MISSILE_UV = 1,
		VBO_LANDSCAPE = 2,
		VBO_MISSILE_VERTEX = 3,
		VBO_AABB = 4,
		VBO_STARFIELD = 5,
		VBO_PARTICLE = 6,
		VBO_PROPULSION = 7,
		VBO_DEBRIS = 8,
		VBO_COUNT = 9;
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
		static constexpr float slowdown = .01f, g = -.00005f, init_mul = 1.f, min_fade = 750.f, max_fade = 1500.,
			v_min = .05f, v_max = .2f, blink_rate = 33.;
		glm::vec3 pos;
		bool kill = false;
		float time;
		struct Particle {
			glm::vec3 pos, v;
			glm::vec4 col;
			float life, fade_duration;
			size_t start_col_idx;
		};
		std::array<Particle, count> arr;
		Particles(const glm::vec3& pos, double time) : pos(pos), time((float)time) {
			static std::uniform_real_distribution<> col_dist(0., 1.);
			static std::uniform_real_distribution<> rad_dist(.0, glm::two_pi<float>());
			static std::uniform_real_distribution<float> fade_dist(min_fade, max_fade);
			static std::uniform_real_distribution<> v_dist(v_min, v_max);
			static std::uniform_int_distribution<> col_idx_dist(0, sizeof(colors) / sizeof(colors[0]) - 1);
			for (size_t i = 0; i < count; ++i) {
				arr[i].col = { col_dist(mt), col_dist(mt), col_dist(mt), 1.f };
				arr[i].pos = {};
				auto r = rad_dist(mt);

				arr[i].v = { std::cos(r) * v_dist(mt) * init_mul, std::sin(r) * v_dist(mt) * init_mul, 0.f };
				arr[i].life = 1.f;
				arr[i].fade_duration = fade_dist(mt);
				arr[i].start_col_idx = col_idx_dist(mt);
			}
		}
		void Update(const Time& t) {
			kill = true;
			for (size_t i = 0; i < count; ++i) {
				if (arr[i].life <= 0.f) continue;
				kill = false;
				arr[i].pos += arr[i].v * (float)t.frame;
				//arr[i].v *= 1.f - arr[i].decay * (float)t.frame;
				arr[i].v.y += g * (float)t.frame;
				arr[i].col = colors[(arr[i].start_col_idx + size_t((t.total - time) / blink_rate)) % (sizeof(colors) / sizeof(colors[0]))];
				arr[i].col.a = arr[i].life;
				auto fade_time = (float)(t.total - time) / arr[i].fade_duration;
				arr[i].life = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
				if (arr[i].life <= 0.01f) arr[i].life = 0.f;
			}
		}
	};
	struct StarField {
		size_t count_per_layer;
		AABB bounds;
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
	Renderer(Asset::Assets& assets, const AABB& scene_bounds) : assets(assets) {
		Init(scene_bounds);
	}
	void Init(const AABB& scene_bounds) {
		glGenBuffers(sizeof(vbo) / sizeof(vbo[0]), vbo);
		// ProtoX
		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROTOX]);
			glBufferData(GL_ARRAY_BUFFER, assets.probe.vertices.size() * sizeof(assets.probe.vertices[0]), &assets.probe.vertices.front(), GL_STATIC_DRAW);

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
		// Debris
		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_DEBRIS]);
			glBufferData(GL_ARRAY_BUFFER, assets.debris.vertices.size() * sizeof(assets.debris.vertices[0]), nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		// Landscape
		{
			//std::vector<glm::vec3> landscape_data;
			//const size_t count = 30;
			//const int w = 2048, h = height >> 1, mid = -(h >> 1);
			//int lb = -h, rb = 0, start = -(w >> 1) - w;
			//landscape.start_x = -(w >> 1);
			//int prev = mid;
			//landscape_data.emplace_back((float)start, (float)mid, 0.f);
			//for (;;) {
			//	if (start >= -(w >> 1))
			//		break;
			//	std::uniform_int_distribution<> dist(lb, rb);
			//	int rnd = dist(mt);
			//	start += std::abs(prev - rnd);
			//	prev = rnd;
			//	landscape_data.emplace_back((float)start, (float)rnd, 0.f);
			//}
			//landscape_data.emplace_back((float)(landscape_data.back().x + std::abs(landscape_data.back().y - mid)), (float)mid, 0.f);
			//landscape.end_x = -(w >> 1) + landscape_data.back().x - landscape_data.front().x;
			//size_t transform_start = landscape_data.size();
			//landscape_data.insert(landscape_data.end(), landscape_data.begin(), landscape_data.end());
			//float offset = landscape_data.back().x - landscape_data.front().x;
			//std::transform(landscape_data.begin() + transform_start, landscape_data.end(), landscape_data.begin() + transform_start, [=](glm::vec3 d) {
			//	d.x += offset;
			//	return d;
			//});
			//transform_start = landscape_data.size();
			//landscape_data.insert(landscape_data.end(), landscape_data.begin(), landscape_data.begin() + transform_start);
			//std::transform(landscape_data.begin() + transform_start, landscape_data.end(), landscape_data.begin() + transform_start, [&](glm::vec3 d) {
			//	d.x += offset + offset;
			//	return d;
			//});
			//landscape.verex_count = landscape_data.size();
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LANDSCAPE]);
			glBufferData(GL_ARRAY_BUFFER, assets.land.vertices.size() * sizeof(assets.land.vertices[0]), &assets.land.vertices.front(), GL_STATIC_DRAW);
		}

		// Missile
		{
			{
				glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_UV]);
				glBufferData(GL_ARRAY_BUFFER, assets.missile.uv.size() * sizeof(assets.missile.uv[0]), &assets.missile.uv.front(), GL_STATIC_DRAW);
				glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_VERTEX]);
				glBufferData(GL_ARRAY_BUFFER, assets.missile.vertices.size() * sizeof(assets.missile.vertices[0]), &assets.missile.vertices.front(), GL_STATIC_DRAW);
			}
			std::normal_distribution<> dist(32., 12.);
			const size_t count = 32, size = 64;
			std::vector<GLubyte> texture_data(size * 4);
			std::uniform_int_distribution<> u_dist(0, 255);
			for (size_t i = 0; i < count; ++i) {
				auto rnd = dist(mt);
				size_t idx = (size_t)std::max(0., std::min(double(size - 1), rnd));
				idx *= 4;
				texture_data[idx++] = 0;// u_dist(mt);
				texture_data[idx++] = 255;// u_dist(mt);
				texture_data[idx++] = 0;// u_dist(mt);
				texture_data[idx] = 255;
			}
			texture_data[0] = 0;// u_dist(mt);
			texture_data[1] = 0;// u_dist(mt);
			texture_data[2] = 255;// u_dist(mt);
			texture_data[3] = 255;
			texture_data[(size - 1)  * 4] = 255;// u_dist(mt);
			texture_data[(size - 1) * 4 + 1] = 0;// u_dist(mt);
			texture_data[(size - 1) * 4 + 2] = 0;// u_dist(mt);
			texture_data[(size - 1) * 4 + 3] = 255;

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
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 5, nullptr, GL_DYNAMIC_DRAW);
		}
		{
			// Starfield
			const float star_size = 3.f;
			const size_t count_per_layer = 3000, layer_count = 3;
			std::vector<glm::vec3> data;
			data.reserve(count_per_layer *layer_count * 6);
			float mul = 2.f;
			for (size_t j = 0; j < layer_count; ++j) {
				std::uniform_real_distribution<> dist_x(scene_bounds.l * mul,
					scene_bounds.r * mul),
					dist_y(scene_bounds.b * mul, scene_bounds.t * mul);
				mul -=.5f;
				for (size_t i = 0; i < count_per_layer; ++i) {
					GenerateSquare((float)dist_x(mt), (float)dist_y(mt), star_size, data);
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_STARFIELD]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			starField = { count_per_layer, scene_bounds, vbo[VBO_STARFIELD], count_per_layer * layer_count, {1.f, {1.f, 1.f, 1.f}}, {.5f,{ .5f, .0f, .0f }}, {.25f,{ .0f, .0f, .5f } } };
		}
		
		{
			// particle
			const float particle_size = 3.f;
			glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo = vbo[VBO_PARTICLE]);
			std::vector<glm::vec3> data;
			GenerateSquare(0.f, 0.f, particle_size, data);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			Particles::vertex_count = data.size();
		}
		{
			// propulsion
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROPULSION]);
			glBufferData(GL_ARRAY_BUFFER, assets.propulsion.vertices.size() * sizeof(assets.propulsion.vertices[0]), &assets.propulsion.vertices.front(), GL_STATIC_DRAW);
		}
		
	}
	void PreRender() {
		//rt.Set();
		glClear(GL_COLOR_BUFFER_BIT);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	void PostRender() {
		//rt.Render();
		//rt.Reset();
	}
	void DrawBackground(const Camera& cam) {
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
		glm::vec3 pos = cam.pos * starField.layer3.z;
		glm::mat4 mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform4f(shader.uCol, starField.layer3.color.r, starField.layer3.color.g, starField.layer3.color.b, 1.f);
		glDrawArrays(GL_TRIANGLES, starField.count_per_layer * 6 * 2, starField.count_per_layer);
		pos = cam.pos * starField.layer2.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform4f(shader.uCol, starField.layer2.color.r, starField.layer2.color.g, starField.layer2.color.b, 1.f);
		glDrawArrays(GL_TRIANGLES, starField.count_per_layer * 6 * 1, starField.count_per_layer);
		pos = cam.pos * starField.layer1.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glUniform4f(shader.uCol, starField.layer1.color.r, starField.layer1.color.g, starField.layer1.color.b, 1.f);
		glDrawArrays(GL_TRIANGLES, 0, starField.count_per_layer);
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const Asset::Model& model, size_t vbo_index) {
		const auto& shader = colorShader;
		glUseProgram(shader.program.id);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[vbo_index]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glm::mat4 mvp = cam.vp;
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		for (const auto& l : model.layers)
			for (const auto& p : l.parts) {
				glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, 1.f);
				glDrawArrays(GL_TRIANGLES, p.first, p.count);
			}
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const std::list<Particles>& particles) {
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const auto& shader = colorPosAttribShader;
		glUseProgram(shader.program.id);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &cam.vp[0][0]);
		for (const auto& p : particles) {
			for (size_t i = 0; i < p.arr.size(); ++i) {
				glUniform4f(shader.uCol, p.arr[i].col.r, p.arr[i].col.g, p.arr[i].col.b, p.arr[i].col.a);
				glVertexAttrib3fv(1, &(p.pos + p.arr[i].pos)[0]);
				glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);
			}
		}
		glDisableVertexAttribArray(0);
		glDisable(GL_BLEND);
	}
	void Draw(const Camera& cam, const AABB& aabb) {
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
		glUniform4f(shader.uCol, 1.f, 1.f, 1.f, 1.f);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINE_STRIP, 0, 5);
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const ProtoX& proto, const ProtoX::Propulsion& prop) {
		auto& shader = colorShader;
		glUseProgram(shader.program.id);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_PROPULSION]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const size_t frame_count = assets.propulsion.layers.size();
		const auto& model = assets.propulsion;
		if (prop.on) {
			glm::mat4 m = glm::translate({}, proto.pos + prop.pos);
			m = glm::rotate(m, prop.rot, { 0.f, 0.f, 1.f });
			glm::mat4 mvp = cam.vp * m;
			glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);

			auto& layer = assets.propulsion.layers[proto.left.frame];
			for (const auto& p : layer.parts) {
				glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, 1.f);
				glDrawArrays(GL_TRIANGLES, p.first, p.count);
			}
		}
	}
	void Draw(const Camera& cam, const ProtoX& proto) {
		if (!proto.visible)
			return;
		if (proto.hit) {
			glEnable(GL_BLEND);
			glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_DEBRIS]);
			glBufferData(GL_ARRAY_BUFFER, proto.debris.vertices.size() * sizeof(proto.debris.vertices[0]), &proto.debris.vertices.front(), GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0,
				3,
				GL_FLOAT,
				GL_FALSE,
				0,
				(void*)0);
			const auto& shader = colorShader;
			glUseProgram(shader.program.id);
			auto& p = assets.debris.layers.front().parts.front();
			glm::mat4 mvp = glm::translate(cam.vp, proto.pos);
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, proto.fade_out);
			glDrawArrays(GL_TRIANGLES, p.first, p.count);
			glDisableVertexAttribArray(0);
			glDisable(GL_BLEND);
			return;
		}
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
		auto& shader = colorShader;
		glUseProgram(shader.program.id);
		for (const auto& p : proto.layer.parts) {
			glm::mat4 mvp = glm::translate(cam.vp, proto.pos);
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, 1.f);
			glDrawArrays(GL_TRIANGLES, p.first, p.count);
		}
		for (const auto& p : proto.turret.layer.parts) {
			glm::mat4 m = glm::translate({}, proto.pos + proto.turret.layer.pivot);
			m = glm::rotate(m, proto.turret.rot, { 0.f, 0.f, 1.f });
			m = glm::translate(m, -proto.turret.layer.pivot);
			
			glm::mat4 mvp = cam.vp * m;
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, 1.f);
			glDrawArrays(GL_TRIANGLES, p.first, p.count);
		}

		Draw(cam, proto, proto.left);
		Draw(cam, proto, proto.right);
		Draw(cam, proto, proto.bottom);
		//if (proto.state.rthruster) {
		//	mvp = cam.vp;
		//	m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.rthruster_model;
		//	mvp *= m;
		//	glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		//	auto frame = (proto.state.rt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
		//		: model.parts[(size_t)Asset::Parts::Thruster2];
		//	glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		//}
		//if (proto.state.bthruster) {
		//	mvp = cam.vp;
		//	m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.bthruster1_model;
		//	mvp *= m;
		//	glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		//	auto frame = (proto.state.bt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
		//		: model.parts[(size_t)Asset::Parts::Thruster2];
		//	glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		//	mvp = cam.vp;
		//	m = glm::translate({}, proto.pos) * glm::scale({}, proto.scale) * proto.bthruster2_model;
		//	mvp *= m;
		//	glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		//	glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		//}
#ifdef VAO_SUPPORT
		glBindVertexArray(0);
#else
		glDisableVertexAttribArray(0);
#endif
	}

	void DrawLandscape(const Camera& cam) {
		Draw(cam, assets.land, VBO_LANDSCAPE);
	}
	void Draw(const Camera& cam, const std::vector<::Missile>& missiles) {
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
				glm::rotate(glm::mat4{}, missile.rot, { 0.f, 0.f, 1.f });
			mvp *= m;
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			for (const auto& l : assets.missile.layers)
				for (const auto& p : l.parts) {
					glDrawArrays(GL_TRIANGLES, p.first, p.count);
				}
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

class Scene {
public:
	Asset::Assets assets;
	AABB bounds;
	Shader::Simple simple;
	Renderer renderer;
	std::unique_ptr<ProtoX> player;
	std::vector<Missile> missiles;
	std::map<size_t, std::unique_ptr<ProtoX>> players;
	std::list<Renderer::Particles> particles;
	Object mesh{ { 5.f, 0.f, 0.f } };
	Camera camera{ globals.width, globals.height };
	InputHandler inputHandler;
	Timer timer;
	GLuint VertexArrayID;
	GLuint vertexbuffer;
	GLuint uvbuffer;
	GLuint texID;
	GLuint uTexSize;
	glm::mat4x4 mvp;
	std::queue<std::vector<unsigned char>> messages;
#ifndef __EMSCRIPTEN__
	void* operator new(size_t i)
	{
		return _mm_malloc(i,16);
	}
	void operator delete(void* p)
	{
		_mm_free(p);
	}
#endif
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
	Scene() : bounds(assets.land.aabb),
#ifdef DEBUG_REL
		player(std::make_unique<ProtoX>(0xdeadbeef, assets.probe, assets.debris, assets.propulsion.layers.size())),
#endif
		renderer(assets, bounds) {
		/*bounds.t = float((height >> 1) + (height >> 2));
		bounds.b = -float((height >> 1) + (height >> 2));*/
#ifdef DEBUG_REL
		players[(size_t)0xbeef] = std::make_unique<ProtoX>( (size_t)0xbeef, assets.probe, assets.debris, assets.propulsion.layers.size() );

		auto& p = players[0xbeef];
		p->pos.x = 100.f;
#endif
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
		renderer.DrawLandscape(camera);
		renderer.Draw(camera, missiles);
		for (const auto& p : players) {
			renderer.Draw(camera, *p.second.get());
			renderer.Draw(camera, p.second->aabb.Translate(p.second->pos));
		}
		if (player)
			renderer.Draw(camera, *player.get());
		renderer.Draw(camera, particles);
		renderer.PostRender();
	}

	bool RemoveMissile(Missile& m) {
		if (&m != &missiles.back()) {
			m = missiles.back();
			missiles.pop_back();
			return false;
		}
		missiles.pop_back();
		return true;
	}
	void Update(const Time& t) {
		const double scroll_speed = .5, // px/s
			rot_ratio = .002;
		if (inputHandler.keys[(size_t)InputHandler::Keys::Left])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.keys[(size_t)InputHandler::Keys::Right])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (player) {
			if (inputHandler.update)
				player->turret.rot += float((inputHandler.px - inputHandler.x) * rot_ratio);

			player->Move(t, inputHandler.keys[(size_t)InputHandler::Keys::A],
				inputHandler.keys[(size_t)InputHandler::Keys::D],
				inputHandler.keys[(size_t)InputHandler::Keys::W]);
			while (!inputHandler.event_queue.empty()) {
				auto e = inputHandler.event_queue.back();
				inputHandler.event_queue.pop();
				switch (e) {
				case InputHandler::ButtonClick::LB:
					player->Shoot(missiles);
					break;
				}
			}
			player->Update(t, bounds);
		}
		for (auto& p : players) {
			p.second->Update(t, bounds);
		}

		for (auto p = std::begin(players); p != std::end(players);){
			if (p->second->killed)
				players.erase(p++);
			else ++p;
		}
		for (auto& m : missiles) {
			m.Update(t);
		}
		if (player) {
			auto d = camera.pos.x + player->pos.x;
			auto res = player->WrapAround(assets.land.layers[0].aabb.l, assets.land.layers[0].aabb.r);
			camera.Update(t);
			camera.Tracking(player->pos, bounds, player->aabb);
			// wraparound camera hack
			// wrap around from left
			if (res < 0)
				camera.Translate(std::max(d, -float(globals.width >> 2)) - (globals.width >> 2), 0.f, 0.f);
			else if (res > 0)
				camera.Translate(std::min(d, float(globals.width >> 2)) + (globals.width >> 2), 0.f, 0.f);

			auto it = missiles.begin();
			while (it != missiles.end()) {
				bool missile_removed = false;
				auto& m = *it;
				if (m.IsOutOfBounds(bounds)) {
					if (RemoveMissile(m)) break;
					continue;
				}
				else {
					// TODO:: test all hits or just ours?
					if (m.owner != player.get()) {
						++it;
						continue;
					}
					bool last = false;
					for (const auto& p : players) {
						// if (m.owner != p.second.get()) continue;
						glm::vec3 hit_pos;
						if (!p.second->hit && !p.second->killed && p.second->invincible == 0.f && m.HitTest(p.second->aabb.Translate(p.second->pos), hit_pos)) {
							p.second->Kill(hit_pos, (double)t.total);
							particles.push_back({ hit_pos, t.total });
							++player->score;
							if (player->ws) {
								Scor msg{ Tag("SCOR"), player->id, p.second->id, m.id, hit_pos.x, hit_pos.y };
								player->ws->Send((const char*)&msg, sizeof(msg));
							}
							last = RemoveMissile(m);
							missile_removed = true;
							break;
						}
					}
					if (last) break;
				}
				if (!missile_removed)
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
#ifdef DEBUG_REL
		if (players.empty()) {
			players[(size_t)0xbeef] = std::make_unique<ProtoX>((size_t)0xbeef, assets.probe, assets.debris, assets.propulsion.layers.size());
			auto& p = players[0xbeef];
			p->pos.x = 50.f;
		}
#endif
	}
	~Scene() {
		glDeleteBuffers(1, &vertexbuffer);
		glDeleteBuffers(1, &uvbuffer);
		glDeleteTextures(1, &texID);
		//glDeleteVertexArrays(1, &VertexArrayID);
	}
	void OnError(int code, const char* msg) {
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_ERROR, "Socket error: %d  %s", code, msg);
#endif
	}
	void OnClose() {
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "Socket closed");
#endif
	}
	void OnOpen() {
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "Socket open");
#endif
	}
	void OnMessage(const char* msg, int len) {
//#ifdef __EMSCRIPTEN__
//		std::string str{ msg, (unsigned int)len };
//		emscripten_log(EM_LOG_CONSOLE, "Socket message: %s", str.c_str());
//#endif
		messages.push(std::vector<unsigned char>{ msg, msg + (size_t)len });
	}
	void OnConn(size_t id) {
		player = std::make_unique<ProtoX>(id, assets.probe, assets.debris, assets.propulsion.layers.size(), globals.ws.get());
#ifndef DEBUG_REL
		float dx = (assets.probe.aabb.r - assets.probe.aabb.l) / 2.f,
			dy = (assets.probe.aabb.t - assets.probe.aabb.b) / 2.f;
		std::uniform_real_distribution<> x_dist(bounds.l + dx, bounds.r - dx), y_dist(bounds.b + dy, bounds.t - dy);
		player->pos = { x_dist(mt), y_dist(mt), 0.f };
#endif
		SendSessionID();
	}
	void SendSessionID() {
		constexpr size_t sess = Tag("SESS"); // sessionID
		union {
			struct {
				size_t tag;
				char sessionID[5];
			}str;
			char arr[128];
		}msg;
		msg.str.tag = sess;
		std::copy(std::begin(globals.sessionID), std::end(globals.sessionID), msg.str.sessionID);
		globals.ws->Send(msg.arr, sizeof(msg.str));
	}
	void OnPlyr(const std::vector<unsigned char>& msg) {
		Plyr player;
		memcpy(&player, &msg.front(), msg.size());
//#ifdef __EMSCRIPTEN__
//		emscripten_log(EM_LOG_CONSOLE, "OnPlyr %d ", player.id, player.x, player.y, player.rot, player.invincible);
//#endif
		auto it = players.find(player.id);
		ProtoX * proto;
		if (it == players.end()) {
			auto ptr = std::make_unique<ProtoX>( player.id, assets.probe, assets.debris, assets.propulsion.layers.size() );
			proto = ptr.get();
			players[player.id] = std::move(ptr);
		}
		else
			proto = it->second.get();
		proto->pos.x = player.x; proto->pos.y = player.y; proto->turret.rot = player.rot; proto->invincible = player.invincible;
	}
	void OnMisl(const std::vector<unsigned char>& msg) {
		const Misl* misl = reinterpret_cast<const Misl*>(&msg.front());
		auto it = players.find(misl->player_id);
		if (it != players.end())
			missiles.push_back({ { misl->x, misl->y, 0.f }, misl->rot, misl->vel, it->second.get(), misl->missile_id });
	}
	void OnScor(const std::vector<unsigned char>& msg) {
		if (!player) return;
		const Scor* scor = reinterpret_cast<const Scor*>(&msg.front());
		particles.push_back({ glm::vec3{scor->x, scor->y, 0.f}, (double)timer.Total() });
		auto it = std::find_if(missiles.begin(), missiles.end(), [=](const Missile& m) {
			return m.owner->id == scor->owner_id && m.id == scor->missile_id;});
		if (it != missiles.end()) {
			printf("remove %d\n", missiles.size());
			RemoveMissile(*it);
			printf("after %d\n", missiles.size());
		}
		if (scor->target_id == player->id)
			player->Kill({ scor->x, scor->y, 0.f }, (double)timer.Total());
		else {
			auto it = players.find(scor->target_id);
			if (it != players.end())
				it->second->Kill({ scor->x, scor->y, 0.f }, (double)timer.Total());
		}
	}
	void OnKill(const std::vector<unsigned char>& msg) {
		const Kill* kill = reinterpret_cast<const Kill*>(&msg.front());
		auto it = std::find_if(std::begin(players), std::end(players), [&](const auto& p) {
			return p.second->id == kill->client_id; });
		if (it == std::end(players)) return;
		auto& p = it->second;
		auto hit_pos = p->pos + glm::vec3{ p->aabb.r - p->aabb.l, p->aabb.t - p->aabb.b, 0.f };
		p->Kill(hit_pos, (double)timer.Total());
	}
	void Dispatch(const std::vector<unsigned char>& msg) {
		size_t tag = Tag(msg);
		constexpr size_t conn = Tag("CONN"), // clientID
			kill = Tag("KILL"),	// clientID
			plyr = Tag("PLYR"), // clientID, pos.x, pos.y, invincible
			misl = Tag("MISL"), // clientID, pos.x, pos.y, v.x, v.y
			scor = Tag("SCOR"); // clientID, clientID
//#ifdef __EMSCRIPTEN__
//		emscripten_log(EM_LOG_CONSOLE, "Dispatch %x ", tag);
//#endif
		switch (tag) {
		case conn:
			OnConn(ID5(msg, 4));
			break;
		case plyr:
			OnPlyr(msg);
			break;
		case misl:
			OnMisl(msg);
			break;
		case scor:
			OnScor(msg);
			break;
		case kill:
		#ifdef __EMSCRIPTEN__
			emscripten_log(EM_LOG_CONSOLE, "kill %x ", ID5(msg, 4));
		#endif
			OnKill(msg);
			break;
		}
	}
	void ProcessMessages() {
		while (messages.size()) {
			auto& msg = messages.front();
			Dispatch(msg);
			messages.pop();
		}
	}
};

void init(int width, int height) {
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
int main(int argc, char** argv) {
	//Renderer::DumpCircle();
	if (argc > 1)
		globals.host = (!strcmp(argv[1], "0.0.0.1")) ? "localhost" : argv[1];
	if (argc > 3) {
		auto w = atoi(argv[2]), h = atoi(argv[3]);
		if (w>0)
			globals.width = w;
		if (h>0)
			globals.height = h;
	}
	if (argc > 4) {
		globals.sessionID = argv[4];
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "SessionID is: %s", argv[4]);
#endif
	}
	init(globals.width, globals.height);

	try {
		globals.scene = std::make_unique<Scene>();
	}
	catch (exception& ex) {
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_ERROR, ex.message.c_str());
#else
		std::cout << ex.message;
#endif
		throw;
	}
	timer.Tick();
#ifdef __EMSCRIPTEN__
	Session session = { std::bind(&Scene::OnOpen, globals.scene.get()),
		std::bind(&Scene::OnClose, globals.scene.get()),
		std::bind(&Scene::OnError, globals.scene.get(),std::placeholders::_1, std::placeholders::_2),
		std::bind(&Scene::OnMessage, globals.scene.get(),std::placeholders::_1, std::placeholders::_2) };
	globals.ws = std::make_unique<Client>(globals.host.c_str(), 8000, session);
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
//	try {
		timer.Tick();
		globals.scene->ProcessMessages();
		globals.scene->Update({ (double)timer.Total(), (double)timer.Elapsed() });
		globals.scene->Render();
		InputHandler::Reset();
//	}
//	catch (...) {
//#ifdef __EMSCRIPTEN__
//		emscripten_log(EM_LOG_ERROR, "exception has been thrown");
//#else
//		std::cout << "exception has been thrown";
//#endif
//		throw;
//	}
}