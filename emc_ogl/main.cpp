#include <GL/glew.h>
#include <assert.h>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/rotate_vector.hpp>

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
#include <string>
#include <memory>
#include "Globals.h"
#include "../../MeshLoader/MeshLoader.h"
#include "../emc_socket/Socket.h"
#include <GLFW/glfw3.h>
#include <inttypes.h>
#include "../../MeshLoader/Tga.h"
#include "../../MeshLoader/File.h"

#include "Exception.h"
#include "Logging.h"
#include "Shader/Simple.h"
#include "Shader/Color.h"
#include "Shader/ColorPosAttrib.h"
#include "Shader/Texture.h"
#include "RT.h"
#include "SAT.h"
#include "Envelope.h"

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
static const size_t texw = 256, texh = 256;
static GLFWwindow * window;
static std::random_device rd;
static std::mt19937 mt(rd());

static const glm::vec4 c64[] = {/* {0.f/255.f,0.f/255.f,0.f/255.f, 1.f},*/{ 255.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1.f },{ 152.f / 255.f, 75.f / 255.f, 67.f / 255.f, 1.f },{ 121.f / 255.f, 193.f / 255.f, 200.f / 255.f, 1.f },{ 155.f / 255.f, 81.f / 255.f, 165.f / 255.f, 1.f },{ 104.f / 255.f, 174.f / 255.f, 92.f / 255.f, 1.f },{ 82.f / 255.f, 66.f / 255.f, 157.f / 255.f, 1.f },{ 201.f / 255.f, 214.f / 255.f, 132.f / 255.f, 1.f },
{ 155.f / 255.f, 103.f / 255.f, 57.f / 255.f, 1.f },{ 106.f / 255.f, 84.f / 255.f, 0.f / 255.f, 1.f },{ 195.f / 255.f, 123.f / 255.f, 117.f / 255.f, 1.f },{ 99.f / 255.f, 99.f / 255.f, 99.f / 255.f, 1.f },{ 138.f / 255.f, 138.f / 255.f, 138.f / 255.f, 1.f },{ 163.f / 255.f, 229.f / 255.f, 153.f / 255.f, 1.f },{ 138.f / 255.f, 123.f / 255.f, 206.f / 255.f, 1.f },{ 173.f / 255.f, 173.f / 255.f, 173.f / 255.f, 1.f } },
cpc[] = {/* { 0.f/255.f,0.f/255.f,0.f/255.f, 1.f},*/{ 0.f / 255.f, 0.f / 255.f, 128.f / 255.f, 1.f },{ 0.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f },{ 128.f / 255.f, 0.f / 255.f, 0.f / 255.f, 1.f },{ 128.f / 255.f, 0.f / 255.f, 128.f / 255.f, 1.f },{ 128.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f },{ 255.f / 255.f, 0.f / 255.f, 0.f / 255.f, 1.f },{ 255.f / 255.f, 0.f / 255.f, 128.f / 255.f, 1.f },{ 255.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f },
{ 0.f / 255.f, 128.f / 255.f, 0.f / 255.f, 1.f },{ 0.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f },{ 0.f / 255.f, 128.f / 255.f, 255.f / 255.f, 1.f },{ 128.f / 255.f, 128.f / 255.f, 0.f / 255.f, 1.f },{ 128.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f },{ 128.f / 255.f, 128.f / 255.f, 255.f / 255.f, 1.f },{ 255.f / 255.f, 128.f / 255.f, 0.f / 255.f, 1.f },{ 255.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f },{ 255.f / 255.f, 128.f / 255.f, 255.f / 255.f, 1.f },
{ 0.f / 255.f, 255.f / 255.f, 0.f / 255.f, 1.f },{ 0.f / 255.f, 255.f / 255.f, 128.f / 255.f, 1.f },{ 0.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1.f },{ 128.f / 255.f, 255.f / 255.f, 0.f / 255.f, 1.f },{ 128.f / 255.f, 255.f / 255.f, 128.f / 255.f, 1.f },{ 128.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1.f },{ 255.f / 255.f, 255.f / 255.f, 0.f / 255.f, 1.f },{ 255.f / 255.f, 255.f / 255.f, 128.f / 255.f, 1.f },{ 255.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1. } },
speccy[] = {/* { 0.f/255.f,0.f/255.f,0.f/255.f, 1.f},*/{ 0.f / 255.f, 0.f / 255.f, 202.f / 255.f, 1.f },{ 202.f / 255.f, 0.f / 255.f, 0.f / 255.f, 1.f },{ 202.f / 255.f, 0.f / 255.f, 202.f / 255.f, 1.f },{ 0.f / 255.f, 202.f / 255.f, 0.f / 255.f, 1.f },{ 0.f / 255.f, 202.f / 255.f, 202.f / 255.f, 1.f },{ 202.f / 255.f, 202.f / 255.f, 0.f / 255.f, 1.f },{ 202.f / 255.f, 202.f / 255.f, 202.f / 255.f, 1.f },
/*{ 0.f/255.f,0.f/255.f,0.f/255.f, 1.f},*/{ 0.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f },{ 255.f / 255.f, 0.f / 255.f, 0.f / 255.f, 1.f },{ 255.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f },{ 0.f / 255.f, 255.f / 255.f, 0.f / 255.f, 1.f },{ 0.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1.f },{ 255.f / 255.f, 255.f / 255.f, 0.f / 255.f, 1.f },{ 255.f / 255.f, 255.f / 255.f, 255.f / 255.f, 1.f } };
//const glm::vec4 colors[] = { { 1.0f,0.5f,0.5f , 1.f },{ 1.0f,0.75f,0.5f , 1.f },{ 1.0f,1.0f,0.5f , 1.f },{ 0.75f,1.0f,0.5f , 1.f },
//{ 0.5f,1.0f,0.5f , 1.f },{ 0.5f,1.0f,0.75f , 1.f },{ 0.5f,1.0f,1.0f , 1.f },{ 0.5f,0.75f,1.0f , 1.f },
//{ 0.5f,0.5f,1.0f , 1.f },{ 0.75f,0.5f,1.0f , 1.f },{ 1.0f,0.5f,1.0f , 1.f },{ 1.0f,0.5f,0.75f , 1.f } };

class Scene;
class Envelope;
struct {
#ifdef DEBUG_REL
	const float invincibility = 5000.f;
#else
	const float invincibility = 5000.f;
#endif
	const float	scale = 40.f,
		propulsion_scale = 80.f,
		text_spacing = 30.f,
		text_scale = 120.f,
		missile_size = 120.f,
		blink_rate = 500., // ms
		blink_ratio = .5f,
		tracking_height_ratio = .75f,
		tracking_width_ratio = .75f,
		blink_duration = 5000.f,
		invincibility_blink_rate = 100.f; // ms
	const glm::vec4 radar_dot_color{ 1.f, 1.f, 1.f, 1.f };
	const gsl::span<const glm::vec4, gsl::dynamic_range> palette = gsl::as_span(cpc, sizeof(cpc) / sizeof(cpc[0]));
	Timer timer;
	std::string host = "localhost";
	unsigned short port = 8000;
	int width = 640, height = 480;
	std::string sessionID;
	std::unique_ptr<Scene> scene;
	std::unique_ptr<Client> ws;
	std::vector<std::unique_ptr<Envelope>> envelopes;
}globals;

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
		throw custom_exception(ss.str());
	}
	::fseek(f, 0, SEEK_END);
	auto fpos = ::ftell(f);
	std::vector<char> data((size_t)fpos);
	::fseek(f, 0, SEEK_SET);
	::fread(&data.front(), 1, (size_t)fpos, f);
	::fclose(f);
	LoadMesh(&data.front(), data.size(), mesh);
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
	struct Layer {
		struct Surface {
			GLint first;
			GLsizei count;
			glm::vec3 col;
		};
		glm::vec3 pivot;
		std::vector<Surface> parts, line_parts;
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
				// <---+--->, --->+<---
				if (lines[l.front()].v1 == lines[l.back()].v1 || lines[l.front()].v2 == lines[l.back()].v2)
				{
					size_t swap_line_index = (swapped[l.back()]) ? l.front() : l.back();
					// TODO:: cyclical graphs and more than 2 lines sharing a vertex are not supported
					//assert(!(swapped[l.back()] && swapped[l.front()]));
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
				// vertexes sharing more than 2 lines are not supported
				n1 = glm::normalize(n1 + lineNormals[line_index.back()]);
				// preserve the thickness on joints
				const auto& l = lines[line_index.back()];
				auto v = glm::normalize(glm::vec2{ vertices[l.v2] } -glm::vec2{ vertices[l.v1] });
				auto f = glm::dot(v, n1);	// cos(angle) between normal and the line
				auto d = glm::distance(v * f, n1); // distancve between the normal and the normal projected onto the line
				n1 = n1 / d; // grow normal the preserve the distance between the normal endpoint and the line
				// TODO:: cyclical graphs and more than 2 lines sharing a vertex are not supported
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
		std::vector<Layer> layers;
		layers.reserve(mesh.layers.size());
		for (const auto& layer : mesh.layers) {
			layers.push_back({ glm::vec3{ layer.pivot.x, layer.pivot.y, layer.pivot.z } * scale });
			auto& layerInfo = layers.back();
			for (size_t section = 0, count = 0, start = vertices.size(); section < layer.poly.n; ++section) {
				auto end = layer.poly.sections[section].start + layer.poly.sections[section].count;
				;
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
#ifdef LINE_RENDER
			for (size_t section = 0, count = 0, start = vertices.size(); section < layer.line.n; ++section) {
				auto end = layer.line.sections[section].start + layer.line.sections[section].count;
				for (size_t line = layer.line.sections[section].start; line < end; ++line) {
					const auto& l = mesh.lines[line];
					vertices.push_back(mesh.vertices[l.v1] * scale);
					vertices.push_back(mesh.vertices[l.v2] * scale);
					count += 2;
				}
				if (count)
					layerInfo.line_parts.push_back({ GLint(start), GLsizei(count),
						glm::vec3{ mesh.surfaces[layer.line.sections[section].index].color[0],
						mesh.surfaces[layer.line.sections[section].index].color[1],
						mesh.surfaces[layer.line.sections[section].index].color[2] } });
			}
#else
			std::vector<glm::vec2> vertexNormals = GenLineNormals(mesh.vertices, mesh.lines, lineWidth);
			for (size_t section = 0, count = 0, start = vertices.size(); section < layer.line.n; ++section) {
				auto end = layer.line.sections[section].start + layer.line.sections[section].count;
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
#endif
		}

		return { vertices, layers };
	}

	Model Reconstruct(const std::vector<glm::vec3>& mesh_vertices,
		std::vector<MeshLoader::PolyLine>& lines,
		const std::vector<glm::vec2>& mesh_texcoord, float scale = 1.f, float lineWidth = 3.f) {
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec2> texcoord;
#ifdef LINE_RENDER
		for (const auto& l : lines) {
			vertices.push_back(mesh_vertices[l.v1] * scale);
			vertices.push_back(mesh_vertices[l.v2] * scale);
			texcoord.push_back(mesh_texcoord[l.v1]);
			texcoord.push_back(mesh_texcoord[l.v2]);
		}
		return{ vertices,{ { {},{/*parts*/}, { Asset::Layer::Surface{ GLint(0), GLsizei(vertices.size()) ,{ 1.f, 1.f, 1.f } } } } }, texcoord };
#else
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
		return{ vertices,{ { {},{ Asset::Layer::Surface{ GLint(0), GLsizei(vertices.size()) ,{ 1.f, 1.f, 1.f } } } } }, texcoord };
#endif
		
	}

	Model ExtractLines(MeshLoader::Mesh& mesh, float scale = 1.f, float lineWidth = 3.f) {
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec2> vertexNormals = GenLineNormals(mesh.vertices, mesh.lines, lineWidth);
		for (const auto& l : mesh.lines) {
			GenVertices(l, vertexNormals, mesh.vertices, scale, vertices);
		}
		return{ vertices,{ { {},{ Asset::Layer::Surface{ GLint(0), GLsizei(vertices.size()), {1.f, 1.f, 1.f} } } } } };
	}

	Img::ImgData LoadImage(const char* path) {
		Img::CTga img;
		auto res = img.Load(path);
		ThrowIf(res != ID_IMG_OK, "Image load error");
		if (img.GetImage().pf == Img::PF_BGR || img.GetImage().pf == Img::PF_BGRA)
			img.GetImage().ChangeComponentOrder();
		return img.GetImage();
	}

	struct Assets {
		std::vector<Img::ImgData> images;
		size_t masks_image_index;
		std::vector<Model*> models;
		Model probe, propulsion, land, missile, debris, text, debug, radar, mouse, wsad;
		Assets() {
#ifdef __EMSCRIPTEN__
#define PATH_PREFIX ""
#else
#define PATH_PREFIX "..//..//emc_ogl//"
#endif
			{
//				size_t i = 0;
//				masks_start = images.size();
//				while (1) {
//					std::string path = PATH_PREFIX"asset//masks//mask" + std::to_string(++i) + ".tga";
//					if (!IO::CFile::Exists(path.c_str())) break;
//#ifdef __EMSCRIPTEN__
//					emscripten_log(EM_LOG_CONSOLE, "mask image founad @ %s\n", path.c_str());
//#endif
//					auto img = std::make_unique<Img::CTga>();
//					auto res = img->Load(path.c_str());
//					assert(res == ID_IMG_OK);
//					if (res != ID_IMG_OK) continue;
//					if (img->GetImage().pf == Img::PF_BGR || img->GetImage().pf == Img::PF_BGRA)
//						img->GetImage().ChangeComponentOrder();
//					images.push_back(std::move(img));
//				}
//				masks_end = images.size();
				images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4.tga"));
				masks_image_index = images.size() - 1;
				images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_staggered_6x8.tga"));
				images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x3_scanline.tga"));
				images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4_scanline.tga"));
				images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4_scanline2.tga"));
				
#ifdef __EMSCRIPTEN__
				emscripten_log(EM_LOG_CONSOLE, "image count %d\n", images.size());
#endif
			}
			//{
			//	MeshLoader::Mesh mesh;
			//	ReadMeshFile(PATH_PREFIX"asset//debug2.mesh", mesh);
			//	debug = Reconstruct(mesh,  12000.);
			//	models.push_back(&debug);
			//}

			{
				const std::vector<glm::vec3> vertices{ { 0.f, 0.f, 0.f },
				{ 1.f,0.f, 0.f },
				{ 1.f, -1.f, 0.f } };
				std::vector<MeshLoader::PolyLine> lines{ { 0, 1 },{ 2, 1 } };
				const std::vector<glm::vec2> texcoord{ { 0.f, 0.f },{.5f, 0.f },{ 1.f, 0.f } };
				missile = Reconstruct(vertices, lines, texcoord, globals.scale);
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
				probe = Reconstruct(mesh, globals.scale);
				models.push_back(&probe);
				
				debris = ExtractLines(mesh, globals.scale);
				models.push_back(&debris);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//propulsion.mesh", mesh);
				propulsion = Reconstruct(mesh, globals.propulsion_scale);
				models.push_back(&propulsion);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//land.mesh", mesh);
				land = Reconstruct(mesh, globals.scale);
				models.push_back(&land);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//text.mesh", mesh);
				text = Reconstruct(mesh, globals.text_scale);
				models.push_back(&text);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//radar.mesh", mesh);
				radar = Reconstruct(mesh, globals.scale);
				models.push_back(&radar);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//mouse.mesh", mesh);
				mouse = Reconstruct(mesh, globals.scale);
				models.push_back(&mouse);
			}
			{
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//wsad.mesh", mesh);
				wsad = Reconstruct(mesh, globals.scale);
				models.push_back(&wsad);
			}
			{
				const std::vector<glm::vec3> vertices{ { 0.f, 0.f, 0.f },
				{  0.5f, 0.f, 0.f },
				{ 1.f, 0.f, 0.f } };
				std::vector<MeshLoader::PolyLine> lines{ {0, 1}, {1, 2}};
				const std::vector<glm::vec2> texcoord { { 0.f, 0.f },{.5f, 0.f},{ 1.f, 0.f} };
				missile = Reconstruct(vertices, lines, texcoord, globals.missile_size, 10.f);
				models.push_back(&missile);
			}
			for (auto& m : models) {
				for (auto& l : m->layers) {
					// have lines or triangles?
					const auto& parts = (l.parts.empty()) ? l.line_parts : l.parts;
					if (parts.empty()) continue;
					l.aabb = CalcAABB(m->vertices, parts.front().first, parts.front().count);
					for (size_t i = 1; i < parts.size(); ++i) {
						auto aabb = CalcAABB(m->vertices, parts[i].first, parts[i].count);
						l.aabb = Union(l.aabb, aabb);
					}
					// ...or both
					if (&parts != &l.line_parts) {
						const auto& parts = l.line_parts;
						for (size_t i = 0; i < parts.size(); ++i) {
							auto aabb = CalcAABB(m->vertices, parts[i].first, parts[i].count);
							l.aabb = Union(l.aabb, aabb);
						}
					}
				}


				m->aabb = m->layers.front().aabb;
				for (size_t i = 1; i < m->layers.size(); ++i)
					m->aabb = Union(m->aabb, m->layers[i].aabb);
			}
		}
	};
}

static void errorcb(int error, const char *msg) {
	LOG_ERR(error, msg);
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

struct Object {
	glm::vec3 pos;
	glm::mat4 model;
};
struct Camera {
	glm::vec3 pos{ 0.f, 0.f, -10.f };
	//struct {
	//	int x, y, w, h;
	//}viewport;
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
	void SetProj(int w, int h) {
		proj = glm::ortho((float)(-(w >> 1)), (float)(w >> 1), (float)(-(h >> 1)), (float)(h >> 1), 0.1f, 100.f);
		vp = proj * view;
	}
	Camera(int w, int h) : view(glm::translate(glm::mat4{}, pos)) {
		SetProj(w, h);
	}
	void Update(const Time&) {}
	void Tracking(const glm::vec3& tracking_pos, const AABB& scene_aabb, const AABB& player_aabb) {
		const glm::vec4 player_tl = vp * (glm::vec4(player_aabb.l, player_aabb.t, 0.f, 0.f)),
			player_br = vp * (glm::vec4(player_aabb.r, player_aabb.b, 0.f, 0.f));

		const auto tracking_screen_pos = vp * glm::vec4(tracking_pos, 1.f);
		glm::vec4 d;
		if (tracking_screen_pos.x + player_tl.x < -globals.tracking_width_ratio)
			d.x = tracking_screen_pos.x + globals.tracking_width_ratio + player_tl.x;
		else if (tracking_screen_pos.x + player_br.x > globals.tracking_width_ratio)
			d.x = tracking_screen_pos.x - globals.tracking_width_ratio + player_br.x;
		if (tracking_screen_pos.y + player_br.y < -globals.tracking_height_ratio)
			d.y = tracking_screen_pos.y + globals.tracking_height_ratio + player_br.y;
		else if (tracking_screen_pos.y + player_tl.y > globals.tracking_height_ratio)
			d.y = tracking_screen_pos.y - globals.tracking_height_ratio + player_tl.y;

		pos -= glm::vec3(glm::inverse(vp) * d);
		view = glm::translate({}, pos);
		vp = proj * view;

		const glm::vec4 scene_tl = vp * (glm::vec4(scene_aabb.l, scene_aabb.t, 0.f, 1.f)),
			scene_br = vp * (glm::vec4(scene_aabb.r, scene_aabb.b, 0.f, 1.f));
		d = {};
		if (scene_br.y > -1.f)
			d.y = scene_br.y + 1.f;
		else if (scene_tl.y < 1.f)
			d.y = scene_tl.y - 1.f;
		if (scene_tl.x > -1.f)
			d.x = scene_tl.x + 1.f;
		else if (scene_br.x < 1.f)
			d.x = scene_br.x - 1.f;
		pos -= glm::vec3(glm::inverse(vp) * d);
		view = glm::translate({}, pos);
		vp = proj * view;
	}
};

struct ProtoX;
struct Missile {
	glm::vec3 pos, prev;
	float rot;
	float vel;
	ProtoX* owner;
	size_t id;
	// TODO:: life
	Missile& operator=(const Missile&) = default;
	Missile(const glm::vec3 pos, float rot, float vel, ProtoX* owner, size_t id) : pos(pos), prev(pos),
		rot(rot), vel(vel), owner(owner), id(id) {}
	void Update(const Time& t) {
		prev = pos;
		pos += glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * vel * (float)t.frame;
	}
	bool HitTest(const AABB& bounds, glm::vec3& hit_pos) {
		if (pos == prev) return false;
		/* AABB intersection
		const glm::vec3 end = glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * size + pos;
		const glm::vec2 min{ std::min(end.x, pos.x), std::min(end.y, pos.y) }, max{ std::max(end.x, pos.x), std::max(end.y, pos.y) };
		const AABB aabb_union{ std::min(min.x, bounds.l), std::max(max.y, bounds.t), std::max(max.x, bounds.r), std::min(min.y, bounds.b)};
		const float xd1 = max.x - min.x, yd1 = max.y - min.y, xd2 = bounds.r - bounds.l, yd2 = bounds.t - bounds.b;
		return xd1 + xd2 >= aabb_union.r - aabb_union.l && yd1 + yd2 >= aabb_union.t - aabb_union.b;*/
		const auto rot_v = glm::vec3{ std::cos(rot), std::sin(rot), 0.f };
		// TODO:: hit pos might be off by one frame
		auto end = rot_v * globals.missile_size + pos;
		auto start = prev;
		
		// approximate hit position
		auto c = Center(bounds);
		hit_pos = (glm::distance(c, start) < glm::distance(c, end)) ? start : end;
		// broad phase

		if (end.x < start.x) std::swap(end, start);
		bool hit = std::max(end.x, bounds.r) - std::min(start.x, bounds.l) < end.x - start.x + bounds.r - bounds.l;
		if (!hit) return false;
		if (end.y < start.y) std::swap(end, start);
		auto res = std::max(end.y, bounds.t) - std::min(start.y, bounds.b) < end.y - start.y + bounds.t - bounds.b;
		if (!res) return false;
		// narrow phase
		return ::HitTest(bounds, { start, end });
		//return end.x >= bounds.l && end.x <= bounds.r && end.y >= bounds.b && end.y <= bounds.t;
	}
	bool IsOutOfBounds(const AABB& bounds) {
		return pos.x < bounds.l ||
			pos.x > bounds.r ||
			pos.y < bounds.b ||
			pos.y > bounds.t;
	}
};
struct Ctrl {
	const size_t tag;
	const char ctrl;
};
struct Sess{
	size_t tag;
	char sessionID[5];
};
struct Plyr {
	const size_t tag, id;
	const float x, y, rot, invincible;
};
struct Misl {
	const size_t tag, player_id, missile_id;
	const float x, y, rot, vel;
};
struct Scor {
	const size_t tag, owner_id, target_id, missile_id;
	const float x, y;
};
struct Kill {
	const size_t tag;
	const char client_id[CLIENTID_LEN];
};
struct Conn {
	const size_t tag;
	const char client_id[CLIENTID_LEN];
	const unsigned char ctrl;
};
struct ProtoX {
	const size_t id;
	AABB aabb;
	Client *ws;
	const Asset::Model& debris_ref;
	Asset::Model debris;
	std::vector<float> debris_centrifugal_speed, debris_speed;
	const float max_vel =.3f,
		m = 500.f,
		force =.1f,
		slowdown =.0003f,
		g = -.1f, /* m/ms2 */
		ground_level = 20.f,
		fade_out_time = 1500.f, // ms
		max_debris_speed =.3f, //px/ ms
		min_debris_speed = .05f, //px/ ms
		debris_max_centrifugal_speed =.02f; // rad/ms
	// state...
	float invincible, fade_out, visible = 1.f, blink = 1.f;
	double hit_time;
	bool hit, killed;
	glm::vec3 pos, vel, f, hit_pos;
	const glm::vec3 missile_start_offset;
	const Asset::Layer& layer;
	size_t score = 0, missile_id = 0;
	enum class Ctrl{Full, Prop, Turret};
	Ctrl ctrl = Ctrl::Full;
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
		void SetRot(const float rot) {
			this->rot = std::min(max_rot, std::max(min_rot, rot));
		}
	}turret;
	ProtoX(const size_t id, const Asset::Model& model, const Asset::Model& debris, size_t frame_count, Client* ws = nullptr) : id(id),
		aabb(model.aabb),
		ws(ws),
		debris_ref(debris),
		missile_start_offset(model.layers[model.layers.size() - 1].pivot),
		layer(model.layers.front()),
		left({ 25.f, 15.f, 0.f }, glm::half_pi<float>(), frame_count),
		right({ -25.f, 15.f, 0.f }, -glm::half_pi<float>(), frame_count),
		bottom({}, 0.f, frame_count),
		turret(model.layers.back()) {
		debris_centrifugal_speed.resize(debris.vertices.size() / 6);
		debris_speed.resize(debris.vertices.size() / 6);
		Init();
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "Player connected");
#endif
		
	}
	void Shoot(std::vector<Missile>& missiles) {
		if (killed) return;
		const float missile_vel = 1.f;
		auto rot = turret.rest_pos + turret.rot;
		const glm::vec3 missile_vec{ std::cos(rot) * missile_vel, std::sin(rot) * missile_vel,.0f};
		missiles.emplace_back(pos + missile_start_offset, rot, glm::length(missile_vec), this, ++missile_id );
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
		invincible = globals.invincibility; fade_out = fade_out_time;
		globals.envelopes.push_back(std::unique_ptr<Envelope>(
			new Blink(visible, invincible, globals.timer.TotalMs(), globals.invincibility_blink_rate, 0.f))); 
		globals.envelopes.push_back(std::unique_ptr<Envelope>(
			new Blink(blink, 0., globals.timer.TotalMs(), globals.blink_rate, globals.blink_ratio)));
		hit = killed = false;
	}
	void Kill(const glm::vec3& hit_pos, double hit_time) {
		if (invincible > 0.f || hit || killed) return;
		hit = true;
		this->hit_pos = hit_pos - pos;
		this->hit_time = hit_time;
		debris = debris_ref;
		static std::uniform_real_distribution<float> dist_cfs(-debris_max_centrifugal_speed, debris_max_centrifugal_speed),
			dist_sp(min_debris_speed, max_debris_speed);
		for (auto& cfs : debris_centrifugal_speed) cfs = dist_cfs(mt);
		for (auto& sp : debris_speed) sp = dist_sp(mt);
	}
	void Update(const Time& t, const AABB& bounds) {
		if (invincible > 0.f) {
			invincible -= (float)t.frame;
			if (invincible <= 0.f) {
				invincible = 0.f;
			}
		}
		else if (hit) {
			auto fade_time = (float)(t.total - hit_time) / fade_out_time;
			fade_out = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
			if (killed = (fade_out <= 0.0001f)) return;
			// TODO:: refactor to continuous fx instead of incremental
			for (size_t i = 0, cfs = 0; i < debris.vertices.size(); i += 6, ++cfs) {
				// TODO:: only enough to have the average of the furthest vertices
				auto center = (debris.vertices[i] + debris.vertices[i + 1] + debris.vertices[i + 2] +
					debris.vertices[i + 3] + debris.vertices[i + 4] + debris.vertices[i + 5]) / 6.f;
				auto v = center - hit_pos;
				float len = glm::length(v);
				v /= len;
				v *= debris_speed[cfs] * (float)t.frame;
				//v.y += g * (float)t.frame;
				auto incr = debris_centrifugal_speed[cfs] * (float)t.frame;
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
	s *=.5f;
	data.emplace_back(x - s, y - s, 0.f);
	data.emplace_back(x + s, y - s, 0.f);
	data.emplace_back(x - s, y + s, 0.f);

	data.emplace_back(x + s, y - s, 0.f);
	data.emplace_back(x + s, y + s, 0.f);
	data.emplace_back(x - s, y + s, 0.f);
}

struct Text {
	enum class Align{Left, Center, Right};
	glm::vec3 pos;
	float scale;
	Align align;
	std::string str;
};
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
		VBO_TEXT = 9,
		VBO_RADAR = 10,
		VBO_MOUSE = 11,
		VBO_WSAD = 12,
		VBO_COUNT = 13;
	GLuint vbo[VBO_COUNT];
	std::vector<GLuint> tex;
#ifdef VAO_SUPPORT
	GLuint vao;
#endif
	struct Missile{
		GLuint texID;
		~Missile() {
			glDeleteTextures(1, &texID);
		}
	}missile;
	struct {
		GLuint id;
		float factor = 1.f;
		const Asset::Model* model;
	}wsad_decal, mouse_decal;
	struct Particles {
		static GLuint vbo;
		static GLsizei vertex_count;
		static const size_t count = 200;
		static constexpr float slowdown =.01f, g = -.00005f, init_mul = 1.f, min_fade = 750.f, max_fade = 1500.,
			v_min =.05f, v_max =.35f, blink_rate = 16.;
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
//		struct A {
//			int a, b;
//			A(int a, int b) : a(a), b(b) {
//#ifdef __EMSCRIPTEN__
//				emscripten_log(EM_LOG_CONSOLE, "A ctor. %d %d", a, b);
//#endif
//			}
//		};
		Particles(const glm::vec3& pos, double time) : pos(pos), time((float)time) {
			//static A a(1, 2), b(3, 4), c(5, 6), d(7, 8), e(9, 10), f(11, 12), g(13, 14), h(15, 16);
			//static A a1(17, 18) , b1(31, 41), /*missing*/c1(51, 61), d1(71, 81), e1(91, 101), f1(111, 121), g1(131, 141), h1(151, 161);
			static std::uniform_real_distribution<> col_dist(0., 1.);
			static std::uniform_real_distribution<> rad_dist(.0, glm::two_pi<float>());
			static std::uniform_real_distribution<float> fade_dist(min_fade, max_fade);
			static std::uniform_real_distribution<> v_dist(v_min, v_max);
			static std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);

#ifdef __EMSCRIPTEN__
			emscripten_log(EM_LOG_CONSOLE, "%d %x %x", sizeof(col_idx_dist), *reinterpret_cast<int*>(&col_idx_dist), *(reinterpret_cast<int*>(&col_idx_dist) + 1));
#endif
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
				arr[i].col = globals.palette[(arr[i].start_col_idx + size_t((t.total - time) / blink_rate)) % globals.palette.size()];
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
	static void DumpCircle(const size_t steps = 16) {
		const float ratio = 0.5f;
		for (size_t i = 0; i <= steps; ++i) {
			std::cout << std::cos(glm::two_pi<float>() / steps * i) * ratio << "f, " <<
				std::sin(glm::two_pi<float>() / steps * i) * ratio << "f,.0f,\n";
		}
	}
	Renderer(Asset::Assets& assets, const AABB& scene_bounds) : assets(assets),
		rt(globals.width, globals.height) {
		Init(scene_bounds);
	}
	void Init(const AABB& scene_bounds) {
		glDisable(GL_DEPTH_TEST);
		tex.resize(assets.images.size());
		glGenTextures(tex.size(), &tex.front());
		for (size_t i = 0; i < tex.size(); ++i) {
		
			GLint fmt, internalFmt;
			auto& img = assets.images[i];
			if (img.pf == Img::PF_RGBA)
				fmt = internalFmt = GL_RGBA;
			else if (img.pf == Img::PF_RGB)
				fmt = internalFmt = GL_RGB;
			else if (img.pf == Img::PF_BGR)
				fmt = internalFmt = GL_BGR;
			else if (img.pf == Img::PF_BGRA)
				fmt = internalFmt = GL_BGRA;
			else
				throw custom_exception("Image format is neiter RGBA nor RGB");

			glBindTexture(GL_TEXTURE_2D, tex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, img.width, img.height, 0, fmt, GL_UNSIGNED_BYTE, &img.data.front());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		rt.GenRenderTargets(tex[assets.masks_image_index], assets.images[assets.masks_image_index].width,
			assets.images[assets.masks_image_index].height);
		glBindTexture(GL_TEXTURE_2D, 0);

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
					dist_y(0.f/*scene_bounds.b * mul*/, scene_bounds.t * mul);
				mul -=.5f;
				for (size_t i = 0; i < count_per_layer; ++i) {
					GenerateSquare((float)dist_x(mt), (float)dist_y(mt), star_size, data);
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_STARFIELD]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			starField = { count_per_layer, scene_bounds, vbo[VBO_STARFIELD], count_per_layer * layer_count, {1.f, {1.f, 1.f, 1.f}}, {.5f,{.5f,.0f,.0f }}, {.25f,{.0f,.0f,.5f } } };
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
		{
			// text
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_TEXT]);
			glBufferData(GL_ARRAY_BUFFER, assets.text.vertices.size() * sizeof(assets.text.vertices[0]), &assets.text.vertices.front(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		{
			// raadar
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_RADAR]);
			glBufferData(GL_ARRAY_BUFFER, assets.radar.vertices.size() * sizeof(assets.radar.vertices[0]), &assets.radar.vertices.front(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		{
			// mouse
			glBindBuffer(GL_ARRAY_BUFFER, mouse_decal.id = vbo[VBO_MOUSE]);
			mouse_decal.model = &assets.mouse;
			glBufferData(GL_ARRAY_BUFFER, assets.mouse.vertices.size() * sizeof(mouse_decal.model->vertices[0]), &mouse_decal.model->vertices.front(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		{
			// wsad
			glBindBuffer(GL_ARRAY_BUFFER, wsad_decal.id = vbo[VBO_WSAD]);
			wsad_decal.model = &assets.wsad;
			glBufferData(GL_ARRAY_BUFFER, assets.wsad.vertices.size() * sizeof(wsad_decal.model->vertices[0]), &wsad_decal.model->vertices.front(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}
	void PreRender() {
		rt.Set();
		glClear(GL_COLOR_BUFFER_BIT);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostRender() {
		glViewport(0, 0, globals.width, globals.height);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		rt.Render();
	}
	void DrawBackground(const Camera& cam) {
		const auto& shader = colorShader;
		glUseProgram(shader.id);
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

	void Draw(const Camera& cam, const std::list<Particles>& particles) {
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		const auto& shader = colorPosAttribShader;
		glUseProgram(shader.id);
		glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo);
		glEnableVertexAttribArray(shader.aPos);
		glVertexAttribPointer(shader.aPos,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &cam.vp[0][0]);
		for (const auto& p : particles) {
			for (size_t i = 0; i < p.arr.size(); ++i) {
				glUniform4f(shader.uCol, p.arr[i].col.r, p.arr[i].col.g, p.arr[i].col.b, p.arr[i].col.a);
				glVertexAttrib3fv(shader.aVertex, &(p.pos + p.arr[i].pos)[0]);
				glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);
			}
		}
		glDisableVertexAttribArray(shader.aPos);
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
		glUseProgram(shader.id);
		const glm::mat4& mvp = cam.vp;
		glUniform4f(shader.uCol, 1.f, 1.f, 1.f, 1.f);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINE_STRIP, 0, 5);
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const ProtoX& proto, const ProtoX::Propulsion& prop) {
		auto& shader = colorShader;
		glUseProgram(shader.id);
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
			Draw<GL_TRIANGLES>(shader.uCol, layer.parts);
			Draw<GL_LINES>(shader.uCol, layer.line_parts);
		}
	}
	template<GLenum mode>
	void Draw(GLuint uCol, const std::vector<Asset::Layer::Surface> parts, float brightness = 1.f) {
		for (const auto& p : parts) {
			const auto col = p.col * brightness;
			glUniform4f(uCol, col.r, col.g, col.b, 1.f);
			glDrawArrays(mode, p.first, p.count);
		}
	}
	//void Draw(const Camera& cam, const Asset::Model& model, size_t vbo_index) {
	//	const auto& shader = colorShader;
	//	glUseProgram(shader.id);
	//	glEnableVertexAttribArray(0);
	//	glBindBuffer(GL_ARRAY_BUFFER, vbo[vbo_index]);
	//	glVertexAttribPointer(0,
	//		3,
	//		GL_FLOAT,
	//		GL_FALSE,
	//		0,
	//		(void*)0);
	//	const auto& mvp = cam.vp;
	//	glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
	//	for (const auto& l : model.layers) {
	//		Draw<GL_TRIANGLES>(shader.uCol, l.parts);
	//		Draw<GL_LINES>(shader.uCol, l.line_parts);
	//	}
	//	glDisableVertexAttribArray(0);
	//}
	// TODO:: <==> duplicate???
	void Draw(const Camera& cam, GLuint vbo, const glm::vec3& pos, const Asset::Model& model, float brightness = 1.f) {
		auto& shader = colorShader;
		glUseProgram(shader.id);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const glm::mat4 mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		for (const auto& l : model.layers) {
			Draw<GL_TRIANGLES>(shader.uCol, l.parts, brightness);
			Draw<GL_LINES>(shader.uCol, l.line_parts, brightness);
		}
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const ProtoX& proto, bool player = false) {
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
			glUseProgram(shader.id);
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
		glUseProgram(shader.id);
		const glm::mat4 mvp = glm::translate(cam.vp, proto.pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		float prop_blink = 1.f, turret_blink = 1.f;
		if (player) {
			if (proto.ctrl == ProtoX::Ctrl::Full || proto.ctrl == ProtoX::Ctrl::Prop)
				prop_blink = proto.blink;
			if (proto.ctrl == ProtoX::Ctrl::Full || proto.ctrl == ProtoX::Ctrl::Turret)
				turret_blink = proto.blink;
		}
		Draw<GL_TRIANGLES>(shader.uCol, proto.layer.parts, prop_blink);
		Draw<GL_LINES>(shader.uCol, proto.layer.line_parts, prop_blink);
		{ 
			const glm::mat4 m = glm::translate(
				glm::rotate(
					glm::translate({}, proto.pos + proto.turret.layer.pivot), proto.turret.rot, { 0.f, 0.f, 1.f }), -proto.turret.layer.pivot);
			const glm::mat4 mvp = cam.vp * m;
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			Draw<GL_TRIANGLES>(shader.uCol, proto.turret.layer.parts, turret_blink);
			Draw<GL_LINES>(shader.uCol, proto.turret.layer.line_parts, turret_blink);
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

	void RenderHUD(const Camera& cam, const AABB& bounds, const ProtoX* player, const std::map<size_t, std::unique_ptr<ProtoX>>& players) {
		const float score_scale = 2.f, border = 8.f, text_y = -assets.text.aabb.t - assets.text.aabb.b, text_width = assets.text.aabb.r - assets.text.aabb.l + border;
		const glm::vec3 pos1{ -globals.width/2.f + text_width, text_y, 0.f }, pos2 = { globals.width / 2.f + text_width, text_y, 0.f };
		Draw(cam, vbo[VBO_RADAR], {/*radar pos*/}, assets.radar);
		std::vector<Text> texts;
		texts.push_back({pos1, score_scale, Text::Align::Left, std::to_string(player->score) });
		const auto& shader = colorPosAttribShader;
		glUseProgram(shader.id);
		glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo);
		glEnableVertexAttribArray(shader.aPos);
		glVertexAttribPointer(shader.aPos,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &cam.vp[0][0]);
		const auto col = globals.radar_dot_color * player->blink;
		glUniform4f(shader.uCol, col.r, col.g, col.b, col.a);
		const AABB& aabb = assets.radar.aabb;
		const auto rw = aabb.r - aabb.l, rh = aabb.t - aabb.b, sw = bounds.r - bounds.l, sh = bounds.t - bounds.b,
			rx = rw / sw, ry = rh / sh;

		const glm::vec3 pos(player->pos.x * rx, (player->pos.y + bounds.b) * ry, 0.f);
		glVertexAttrib3fv(shader.aVertex, &(pos)[0]);
		glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);

		glUniform4f(shader.uCol, globals.radar_dot_color.r, globals.radar_dot_color.g, globals.radar_dot_color.b, globals.radar_dot_color.a);
		size_t max = 0;
		for (const auto& p : players) {
			max = std::max(max, p.second->score);
			const glm::vec3 pos(p.second->pos.x * rx, (p.second->pos.y + bounds.b)* ry, 0.f);
			const auto col = globals.radar_dot_color * player->blink;
			glVertexAttrib3fv(shader.aVertex, &(pos)[0]);
			glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);
		}
		texts.push_back({pos2, score_scale, Text::Align::Right, std::to_string(max) });
		glDisableVertexAttribArray(shader.aPos);
		Draw(cam, texts);
	}

	void DrawLandscape(const Camera& cam) {
		Draw(cam, vbo[VBO_LANDSCAPE], {}, assets.land );
	}
	void Draw(const Camera& cam, const std::vector<::Missile>& missiles) {
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		auto& shader = textureShader;
		glUseProgram(shader.id);
		// vertex data
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_VERTEX]);
		glVertexAttribPointer(shader.aPos,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);

		// uv data
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_MISSILE_UV]);
		glVertexAttribPointer(shader.aUV1,
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
		const auto vp = cam.proj * cam.view;
		for (const auto& missile : missiles) {
			const glm::mat4 mvp = glm::rotate(glm::translate(vp, missile.pos), missile.rot, { 0.f, 0.f, 1.f });
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			for (const auto& l : assets.missile.layers) {
				for (const auto& p : l.parts) {
					glDrawArrays(GL_TRIANGLES, p.first, p.count);
				}
				for (const auto& p : l.line_parts) {
					glDrawArrays(GL_LINES, p.first, p.count);
				}
			}
		}

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	}
	void Draw(const Camera& cam, const std::vector<Text>& texts) {
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_TEXT]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		auto& shader = colorShader;
		glUseProgram(shader.id);
		for (const auto& str : texts) {
			auto pos = str.pos;
			if (str.align == Text::Align::Right || str.align == Text::Align::Center) {
				auto ofs = str.str.size() * globals.text_spacing * str.scale;
				if (str.align == Text::Align::Center) ofs /= 2.f;
				pos.x -= ofs;
			}
			for (const auto& c : str.str) {
				if (c - 0x20 >= assets.text.layers.size()) continue;
				const auto& layer = assets.text.layers[c - 0x20];
				const glm::mat4 mvp = glm::scale(glm::translate(cam.vp, pos), { str.scale, str.scale, 1.f });
				glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
				Draw<GL_TRIANGLES>(shader.uCol, layer.parts);
				Draw<GL_LINES>(shader.uCol, layer.line_parts);
				pos.x += layer.aabb.r - layer.aabb.l + globals.text_spacing;
			}
		}
	}
	~Renderer() {
		glDeleteBuffers(sizeof(vbo)/sizeof(vbo[0]), vbo);
		glDeleteTextures(tex.size(), &tex.front());
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
	static std::function<void(int key, int scancode, int action, int mods)> keyCb;
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
		if (keyCb) keyCb(key, scancode, action, mods);
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
std::function<void(int key, int scancode, int action, int mods)> InputHandler::keyCb;

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
	std::vector<Text> texts;
	Object mesh{ { 5.f, 0.f, 0.f } };
	Camera camera{ (int)globals.width, (int)globals.height }, hud{ (int)globals.width, (int)globals.height },
		overlay;
	InputHandler inputHandler;
	//GLuint VertexArrayID;
	//GLuint vertexbuffer;
	//GLuint uvbuffer;
	//GLuint texID;
	//GLuint uTexSize;
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
	//GLuint GenTexture(size_t w, size_t h) {
	//	GLuint textureID;
	//	glGenTextures(1, &textureID);
	//	glBindTexture(GL_TEXTURE_2D, textureID);
	//	GLint fmt, internalFmt;
	//	fmt = internalFmt = GL_RGB;
	//	std::vector<unsigned char> pv(w*h*3);
	//	unsigned char r, g, b;
	//	r = g = b = 0;
	//	std::uniform_int_distribution<uint32_t> dist(0, 255);
	//	for (size_t i = 0; i < w*h * 3;i+=3) {
	//		pv[i] = dist(mt);
	//		pv[i + 1] = dist(mt);
	//		pv[i + 2] = dist(mt);
	//	}
	//	const unsigned char* p = &pv.front();
	//	glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, p);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//	//glGenerateMipmap(GL_TEXTURE_2D);
	//	return textureID;
	//}
public:
	void SetHudVp(int w, int h) {
		camera.SetProj(globals.width, VP_RATIO(globals.height));
		hud.SetProj(globals.width, HUD_RATIO(globals.height));
	}
	void RandomizePos(ProtoX& p) {
		const AABB aabb{ bounds.l - p.aabb.l, bounds.t - p.aabb.t, bounds.r - p.aabb.r, bounds.b - p.aabb.b };
		std::uniform_real_distribution<> xdist(aabb.l, aabb.r), ydist(aabb.b, aabb.t);
		p.pos.x = xdist(mt); p.pos.y = ydist(mt);
		p.pos.x = 0; p.pos.y = 0.;
	}
	void GenerateNPC() {
		static size_t i = 0;
		auto p = std::make_unique<ProtoX>((size_t)0xbeef, assets.probe, assets.debris, assets.propulsion.layers.size());
		RandomizePos(*p.get());
		players[++i] = std::move(p);
	}
	void GenerateNPCs() {
		for (size_t i = 0; i<MAX_NPC; ++i) {
			GenerateNPC();
		}
	}
	void SetCtrl(ProtoX::Ctrl ctrl) {
		if (!player) return;
		player->ctrl = ctrl;
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Prop)
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new Blink(renderer.wsad_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio)));
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Turret)
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new Blink(renderer.mouse_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio)));
	}
	Scene() : bounds(assets.land.aabb),
#ifdef DEBUG_REL
		player(std::make_unique<ProtoX>(0xdeadbeef, assets.probe, assets.debris, assets.propulsion.layers.size())),
#endif
		renderer(assets, bounds),
		overlay(camera) {
		const auto size = renderer.rt.GetCurrentRes();
		SetHudVp(size.x, size.y);
		texts.push_back(Text{ {}, 1.f, Text::Align::Center, "A !\"'()&%$#0123456789:;<=>?AMKXR" });
		/*bounds.t = float((height >> 1) + (height >> 2));
		bounds.b = -float((height >> 1) + (height >> 2));*/
#ifdef DEBUG_REL
		SetCtrl(ProtoX::Ctrl::Full);
		RandomizePos(*player.get());
		GenerateNPCs();
#endif
		inputHandler.keyCb = std::bind(&Scene::KeyCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

		mesh.model = glm::translate(mesh.model, mesh.pos);

		/*texID = GenTexture(texw, texh);
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
			1.f,.0f,
			.5f,  1.0f };

		glGenBuffers(1, &uvbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);*/

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
		auto res = renderer.rt.GetCurrentRes();
		glViewport(0, HUD_RATIO(res.y), res.x, VP_RATIO(res.y));
		renderer.DrawBackground(camera);
		renderer.DrawLandscape(camera);
		renderer.Draw(camera, missiles);
		for (const auto& p : players) {
			renderer.Draw(camera, *p.second.get());
			renderer.Draw(camera, p.second->aabb.Translate(p.second->pos));
		}
		if (player)
			renderer.Draw(camera, *player.get(), true);
		renderer.Draw(camera, particles);

		if (player) {
			if (player->ctrl == ProtoX::Ctrl::Full || player->ctrl == ProtoX::Ctrl::Prop)
				renderer.Draw(overlay, renderer.wsad_decal.id, { -globals.width / 2.f, -globals.height / 2.f, 0.f }, *renderer.wsad_decal.model, renderer.wsad_decal.factor);
			if (player->ctrl == ProtoX::Ctrl::Full || player->ctrl == ProtoX::Ctrl::Turret)
				renderer.Draw(overlay, renderer.mouse_decal.id, { globals.width / 2.f, -globals.height / 2.f, 0.f }, *renderer.mouse_decal.model, renderer.mouse_decal.factor);
		}
		renderer.Draw(overlay, texts);
		glViewport(0, 0, res.x, HUD_RATIO(res.y));
		renderer.RenderHUD(hud, bounds, player.get(), players);
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
	void KeyCallback(int key, int scancode, int action, int mods) {
		static float w = (float)assets.images[assets.masks_image_index].width, h =
			(float)assets.images[assets.masks_image_index].height;
		if (key == GLFW_KEY_PAGE_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.GenMaskUVBufferData( (float)globals.width, (float)globals.height, w+=.1f, h+=.1f);
		} else if (key == GLFW_KEY_PAGE_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.GenMaskUVBufferData((float)globals.width, (float)globals.height, w-=.1f, h-=.1f);
		}else if (key == GLFW_KEY_HOME && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.maskOpacity+=.05f;
		}else if (key == GLFW_KEY_END && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.maskOpacity-=.05f;
		} else if (key == GLFW_KEY_LEFT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.contrast -= .05f;
		} else if (key == GLFW_KEY_RIGHT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.contrast += .05f;
		} else if (key == GLFW_KEY_COMMA && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.brightness -= .05f;
		} else if (key == GLFW_KEY_PERIOD && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.brightness += .05f;
		}
	}
	void Update(const Time& t) {
		const double scroll_speed =.5, // px/s
			rot_ratio =.002;
		if (inputHandler.keys[(size_t)InputHandler::Keys::Left])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.keys[(size_t)InputHandler::Keys::Right])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (player) {
			if (inputHandler.update)
				player->turret.SetRot(float((globals.width>>1) - inputHandler.x) * rot_ratio);
				//player->turret.rot += float((inputHandler.px - inputHandler.x) * rot_ratio);

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

		for (auto p = std::begin(players); p != std::end(players);) {
			if (p->second->killed) {
				players.erase(p++);
			}
			else ++p;
		}
		if (player->killed) {
			player = std::make_unique<ProtoX>(player->id, assets.probe, assets.debris, assets.propulsion.layers.size(), globals.ws.get());
			SetCtrl(player->ctrl);
			RandomizePos(*player.get());
		}
		for (auto& m : missiles) {
			m.Update(t);
		}
		for (const auto& e : globals.envelopes) {
			e->Update(t);
		}
		// TODO:: is resize more efficient
		globals.envelopes.erase(std::remove_if(
			std::begin(globals.envelopes), std::end(globals.envelopes), [](const auto& e) { return e->Finished(); }), globals.envelopes.end());

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
		if (players.size()<MAX_NPC) {
			// TODO:: add new when killed
			GenerateNPC();
			//players[(size_t)0xbeef] = std::make_unique<ProtoX>((size_t)0xbeef, assets.probe, assets.debris, assets.propulsion.layers.size());
			//auto& p = players[0xbeef];
			//p->pos.x = 50.f;
		}
#endif
	}
	~Scene() {
		//glDeleteBuffers(1, &vertexbuffer);
		//glDeleteBuffers(1, &uvbuffer);
		//glDeleteTextures(1, &texID);
		////glDeleteVertexArrays(1, &VertexArrayID);
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
		SendSessionID();
	}
	void OnMessage(const char* msg, int len) {
//#ifdef __EMSCRIPTEN__
//		std::string str{ msg, (unsigned int)len };
//		emscripten_log(EM_LOG_CONSOLE, "Socket message: %s", str.c_str());
//#endif
		messages.push(std::vector<unsigned char>{ msg, msg + (size_t)len });
	}
	void OnConn(const std::vector<unsigned char>& msg) {
		const Conn* conn = reinterpret_cast<const Conn*>(msg.data());
		size_t id = ID5(conn->client_id, 0);
		player = std::make_unique<ProtoX>(id, assets.probe, assets.debris, assets.propulsion.layers.size(), globals.ws.get());
		player->ctrl = static_cast<ProtoX::Ctrl>(conn->ctrl - 48/*TODO:: eliminate conversion*/);
		#ifdef __EMSCRIPTEN__
				emscripten_log(EM_LOG_CONSOLE, "OnConn id: %5s ctrl: %d", conn->client_id, player->ctrl);
		#endif
#ifndef DEBUG_REL
		float dx = (assets.probe.aabb.r - assets.probe.aabb.l) / 2.f,
			dy = (assets.probe.aabb.t - assets.probe.aabb.b) / 2.f;
		std::uniform_real_distribution<> x_dist(bounds.l + dx, bounds.r - dx), y_dist(bounds.b + dy, bounds.t - dy);
		player->pos = { x_dist(mt), y_dist(mt), 0.f };
#endif
	}
	void SendSessionID() {
		Sess msg{ Tag("SESS") };
		std::copy(std::begin(globals.sessionID), std::end(globals.sessionID), msg.sessionID);
		globals.ws->Send((char*)&msg, sizeof(msg));
	}
	void OnPlyr(const std::vector<unsigned char>& msg) {
		const Plyr* player = reinterpret_cast<const Plyr*>(&msg.front());
//#ifdef __EMSCRIPTEN__
//		emscripten_log(EM_LOG_CONSOLE, "OnPlyr %d ", player.id, player.x, player.y, player.rot, player.invincible);
//#endif
		auto it = players.find(player->id);
		ProtoX * proto;
		if (it == players.end()) {
			auto ptr = std::make_unique<ProtoX>( player->id, assets.probe, assets.debris, assets.propulsion.layers.size() );
			proto = ptr.get();
			players[player->id] = std::move(ptr);
		}
		else
			proto = it->second.get();
		proto->pos.x = player->x; proto->pos.y = player->y; proto->turret.rot = player->rot; proto->invincible = player->invincible;
	}
	void OnMisl(const std::vector<unsigned char>& msg) {
		const Misl* misl = reinterpret_cast<const Misl*>(&msg.front());
		auto it = players.find(misl->player_id);
		if (it != players.end())
			missiles.push_back({ { misl->x, misl->y, 0.f }, misl->rot, misl->vel, it->second.get(), misl->missile_id });
	}
	void OnScor(const std::vector<unsigned char>& msg, const Time& t) {
		if (!player) return;
		const Scor* scor = reinterpret_cast<const Scor*>(&msg.front());
		particles.push_back({ glm::vec3{scor->x, scor->y, 0.f}, t.total });
		auto it = std::find_if(missiles.begin(), missiles.end(), [=](const Missile& m) {
			return m.owner->id == scor->owner_id && m.id == scor->missile_id;});
		if (it != missiles.end()) {
			RemoveMissile(*it);
		}
		if (scor->target_id == player->id)
			player->Kill({ scor->x, scor->y, 0.f }, t.total);
		else {
			auto it = players.find(scor->target_id);
			if (it != players.end())
				it->second->Kill({ scor->x, scor->y, 0.f }, t.total);
		}
	}
	void OnKill(const std::vector<unsigned char>& msg, const Time& t) {
		const Kill* kill = reinterpret_cast<const Kill*>(&msg.front());
		auto clientID = ID5(kill->client_id, 0);
		auto it = std::find_if(std::begin(players), std::end(players), [&](const auto& p) {
			return p.second->id == clientID;
		});
		if (it == std::end(players)) return;
		auto& p = it->second;
		auto hit_pos = p->pos + glm::vec3{ p->aabb.r - p->aabb.l, p->aabb.t - p->aabb.b, 0.f };
		particles.push_back({hit_pos, t.total });
		p->Kill(hit_pos, t.total);
	}
	void OnCtrl(const std::vector<unsigned char>& msg) {
		const Ctrl* ctrl = reinterpret_cast<const Ctrl*>(&msg.front());
		SetCtrl(static_cast<ProtoX::Ctrl>(ctrl->ctrl - 48/*TODO:: eliminate conversion*/));
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_CONSOLE, "ctrl %x ", player->ctrl);
#endif
	}
	void Dispatch(const std::vector<unsigned char>& msg, const Time& t) {
		size_t tag = Tag(msg);
		constexpr size_t conn = Tag("CONN"), // str clientID
			kill = Tag("KILL"),	// str clientID
			plyr = Tag("PLYR"), // clientID, pos.x, pos.y, invincible
			misl = Tag("MISL"), // clientID, pos.x, pos.y, v.x, v.y
			scor = Tag("SCOR"), // clientID, clientID
			ctrl = Tag("CTRL"); // other str clientID , upper = '1' / lower = '0'
//#ifdef __EMSCRIPTEN__
//		emscripten_log(EM_LOG_CONSOLE, "Dispatch %x ", tag);
//#endif
		switch (tag) {
		case conn:
			OnConn(msg);
			break;
		case plyr:
			OnPlyr(msg);
			break;
		case misl:
			OnMisl(msg);
			break;
		case scor:
			OnScor(msg, t);
			break;
		case kill:
			OnKill(msg, t);
			break;
		case ctrl:
			OnCtrl(msg);
			break;
		}
	}
	void ProcessMessages(const Time& t) {
		while (messages.size()) {
			auto& msg = messages.front();
			Dispatch(msg, t);
			messages.pop();
		}
	}
};

static void init(int width, int height) {
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

	glClearColor(0.f,.0f,.0f, 1.f);
#ifdef REPORT_RESULT  
	int result = 1;
	REPORT_RESULT();
#endif
}
void main_loop();
int main(int argc, char** argv) {
	//TestHitTest();
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
		globals.timer.Tick();
		globals.scene = std::make_unique<Scene>();
	}
	catch (const custom_exception& ex) {
#ifdef __EMSCRIPTEN__
		emscripten_log(EM_LOG_ERROR, ex.what());
#else
		std::cout << ex.what();
#endif
		throw;
	}
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
		globals.timer.Tick();
		globals.scene->ProcessMessages({ globals.timer.TotalMs(), globals.timer.ElapsedMs() });
		globals.scene->Update({ globals.timer.TotalMs(), globals.timer.ElapsedMs() });
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