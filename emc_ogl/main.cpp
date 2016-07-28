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
#include <mutex>
//#include <thread>
#include "Globals.h"
#include "Helpers.h"
#include "../../MeshLoader/MeshLoader.h"
#include "Logging.h"
#include "Socket.h"
#include <GLFW/glfw3.h>
#include <inttypes.h>
#include "../../MeshLoader/Tga.h"
#include "../../MeshLoader/File.h"
#include "Exception.h"
#include "Shader/Simple.h"
#include "Shader/Color.h"
#include "Shader/ColorPosAttrib.h"
//#include "Shader/Texture.h"
#include "Shader/TextureColorMod.h"
#include "RT.h"
#include "SAT.h"
#include "Envelope.h"
#include "Palette.h"
// TODO::
// - investigate random crashes on network play (message delete for example)
// - add sound
// - finish network game mechanics
// - finalize/cleanup html
// - create preview/thumbnail image
// - player collision
// - refactor pallette
// - redesign proto
// - colorize player only
// - display session lost on socket error
// - test connection loss on broadcast messages (remove connection on exception)
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

class Scene;
class Envelope;
static const glm::vec3 g(0.f, -.1f, 0.f);/* m/ms2 */
struct {
#ifdef DEBUG_REL
	const float invincibility = 5000.f;
#else
	const float invincibility = 5000.f;
#endif
	const float	scale = 40.f,
		radar_scale = 75.f,
		propulsion_scale = 80.f,
		text_spacing = 10.f,
		text_scale = 120.f,
		missile_size = 120.f,
		missile_blink_rate = 33.f, //ms
		blink_rate = 500., // ms
		blink_ratio = .5f,
		tracking_height_ratio = .75f,
		tracking_width_ratio = .75f,
		blink_duration = 5000.f,
		invincibility_blink_rate = 100.f, // ms
		starfield_layer1_blink_rate = 133.f, // ms
		starfield_layer2_blink_rate = 99.f, // ms
		starfield_layer3_blink_rate = 243.f, // ms
		missile_life = 500., // ms
		dot_size = 6.f, // px
		clear_color_blink_rate = 16.f; //ms
	int ab = a;
	const glm::vec4 radar_player_color{ 1.f, 1.f, 1.f, 1.f }, radar_enemy_color{ 1.f, .5f, .5f, 1.f };
	const gsl::span<const glm::vec4, gsl::dynamic_range>& palette = pal, &grey_palette = grey_pal;
	Timer timer;
	std::string host = "localhost";
	unsigned short port = 8000;
	int width = 1024, height = 768;
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

	Model Reconstruct(MeshLoader::Mesh& mesh, float scale, float lineWidth = LINE_WIDTH) {
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
		const std::vector<glm::vec2>& mesh_texcoord, float scale = 1.f, float lineWidth = LINE_WIDTH) {
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

	Model ExtractLines(MeshLoader::Mesh& mesh, float scale = 1.f, float lineWidth = LINE_WIDTH) {
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


				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4.tga"));
				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_staggered_6x8.tga"));
				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x3_scanline.tga"));
				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4_scanline.tga"));
				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4_scanline2.tga"));
				//images.push_back(LoadImage(PATH_PREFIX"asset//masks//mask_straight_3x4_scanline_bright.tga"));
				//masks_image_index = images.size() - 1;
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
				radar = Reconstruct(mesh, globals.radar_scale);
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
				missile = Reconstruct(vertices, lines, texcoord, globals.missile_size, globals.dot_size);
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
	void Tracking(const glm::vec3& tracking_pos, const AABB& scene_aabb, AABB player_aabb) {
		// TODO:: either make proto::aabb local, or refactor this
		player_aabb.l -= tracking_pos.x;
		player_aabb.t -= tracking_pos.y;
		player_aabb.r -= tracking_pos.x;
		player_aabb.b -= tracking_pos.y;
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
	size_t id, col_idx;
	float life, blink;
	bool first = true;
	Missile& operator=(const Missile&) = default;
	Missile(const glm::vec3 pos, float rot, float vel, ProtoX* owner, size_t id) : pos(pos), prev(pos),
		rot(rot), vel(vel), owner(owner), id(id), life(globals.missile_life), blink(globals.missile_blink_rate) {
		GenColIdx();
	}
	void GenColIdx() {
		std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);
		col_idx = col_idx_dist(mt);
	}
	void Update(const Time& t) {
		if (first) { first = false; return; }	// draw at least once, and exist at initial position
		life -= (float)t.frame;
		prev = pos;
		pos += glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * vel * (float)t.frame;
		blink -= (float)t.frame;
		if (blink < 0.f) {
			blink += globals.missile_blink_rate;
			GenColIdx();
		}
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
		hit_pos = /*end;*/ (glm::distance(c, start) < glm::distance(c, end)) ? start : end;
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
	const float x, y, turret_rot, invincible, vx, vy, rot;
	const bool prop_left, prop_right, prop_bottom;
};
struct Misl {
	const size_t tag, player_id, missile_id;
	const float x, y, rot, vel, life;
};
struct Scor {
	const size_t tag, owner_id, target_id, missile_id;
	const int score;
	const float x, y, vec_x, vec_y;
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
struct Wait {
	const size_t tag;
	const unsigned char n;
};
struct Text {
	enum class Align { Left, Center, Right };
	glm::vec3 pos;
	float scale;
	Align align;
	std::string str;
};
struct Particles {
	static GLuint vbo;
	static GLsizei vertex_count;
	static const size_t count = 200;
	static constexpr float slowdown = .02f, g = -.001f, init_mul = 1.f, min_fade = 750.f, max_fade = 1500.f,
		v_min = .05f, v_max = .55f, blink_rate = 16.f;
	const float vec_ratio = .7f; // static constexpr makes emscripten complain about unresolved symbol...
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
	Particles(const glm::vec3& pos, const glm::vec3& vec, double time) : pos(pos), time((float)time) {
		//static A a(1, 2), b(3, 4), c(5, 6), d(7, 8), e(9, 10), f(11, 12), g(13, 14), h(15, 16);
		//static A a1(17, 18) , b1(31, 41), /*missing*/c1(51, 61), d1(71, 81), e1(91, 101), f1(111, 121), g1(131, 141), h1(151, 161);
		static std::uniform_real_distribution<> col_dist(0., 1.);
		static std::uniform_real_distribution<> rad_dist(.0, glm::two_pi<float>());
		static std::uniform_real_distribution<float> fade_dist(min_fade, max_fade);
		static std::uniform_real_distribution<> v_dist(v_min, v_max);
		static std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);

		//#ifdef __EMSCRIPTEN__
		//			emscripten_log(EM_LOG_CONSOLE, "%d %x %x", sizeof(col_idx_dist), *reinterpret_cast<int*>(&col_idx_dist), *(reinterpret_cast<int*>(&col_idx_dist) + 1));
		//#endif
		for (size_t i = 0; i < count; ++i) {
			arr[i].col = { col_dist(mt), col_dist(mt), col_dist(mt), 1.f };
			arr[i].pos = {};
			auto r = rad_dist(mt);

			arr[i].v = { std::cos(r) * v_dist(mt) * init_mul, std::sin(r) * v_dist(mt) * init_mul, 0.f };
			arr[i].v += vec * vec_ratio;
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
GLuint Particles::vbo;
GLsizei Particles::vertex_count;

struct ProtoX {
	const size_t id;
	AABB aabb;
	Client *ws;
	const Asset::Model& debris_ref;
	Asset::Model debris;
	std::vector<float> debris_centrifugal_speed, debris_speed;
	const float max_vel = .3f,
		m = 500.f,
		force = .2f,
		slowdown = .0003f,
		ground_level = 20.f,
		fade_out_time = 1500.f, // ms
		max_debris_speed = .3f, //px/ ms
		min_debris_speed = .05f, //px/ ms
		debris_max_centrifugal_speed = .02f, // rad/ms
		rot_max_speed = .003f, //rad/ms
		rot_inc = .000005f, // rad/ms*ms
		safe_rot = glm::radians(35.f);
	// state...
	float invincible, fade_out, visible = 1.f, blink = 1.f;
	double hit_time;
	bool hit, killed;
	glm::vec3 vel, f, hit_pos;
	int score = 0;
	size_t missile_id = 0;
	enum class Ctrl { Full, Prop, Turret };
	Ctrl ctrl = Ctrl::Full;
	bool pos_invalidated = false; // from web-message
	float clear_color_blink;
	float rot_speed = 0.f;
	struct Body {
		glm::vec3 pos;
		float rot;
		const Asset::Layer& layer;
		Val<AABB> aabb;
		Val<BBox> bbox;
		Body(const glm::vec3& pos, float rot, const Asset::Layer& layer) : pos(pos), rot(rot), layer(layer), aabb(TransformAABB(layer.aabb, GetModel())),
			bbox(TransformBBox(layer.aabb, GetModel())) {}
		void Update(const Time& t) {
			aabb = TransformAABB(layer.aabb, GetModel());
			bbox = TransformBBox(layer.aabb, GetModel());
		}
		glm::mat4 GetModel() const {
			return ::GetModel({}, pos, rot, layer.pivot);
		}
	}body;
	std::vector<Text> msg;
	struct Propulsion {
		const glm::vec3 pos;
		const float rot, scale;
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
		Propulsion(const glm::vec3& pos, float rot, float scale, size_t frame_count) :
			pos(pos), rot(rot), scale(scale), frame_count(frame_count) {}
		auto GetModel(const glm::mat4& m) const {
			return ::GetModel(m, pos, rot, /*layer.pivot*/{}, scale);
		}
	}left, bottom, right;
	struct Turret {
		const float rot = 0.f;
		const float rest_pos = glm::half_pi<float>(),
			min_rot = -glm::radians(45.f) - rest_pos,
			max_rot = glm::radians(225.f) - rest_pos;
		const Asset::Layer& layer;
		const glm::vec3 missile_start_offset;
		Val<AABB> aabb;
		Val<BBox> bbox;
		Turret(const Asset::Layer& layer, const glm::mat4& m) : layer(layer),
			missile_start_offset(layer.pivot), aabb(TransformAABB(layer.aabb, GetModel(m))),
			bbox(TransformBBox(layer.aabb, GetModel(m))) {}
		void SetRot(const float rot) {
			prev_rot = this->rot;
			const_cast<float&>(this->rot) = std::min(max_rot, std::max(min_rot, rot));
		}
		void Update(const Time& t, const glm::mat4& m) {
			aabb = TransformAABB(layer.aabb, GetModel(m));
			bbox = TransformBBox(layer.aabb, GetModel(m));
		}
		glm::mat4 GetModel(const glm::mat4& m) const {
			return ::GetModel(m, {}, rot, layer.pivot);
		}
	private:
		float prev_rot = 0.f;
		/*auto GetPrevModel(const glm::mat4& m) const {
			return ::GetModel(m, {}, prev_rot, layer.pivot);
		}*/
	}turret;
	ProtoX(const size_t id, const Asset::Model& model, const Asset::Model& debris, size_t frame_count, const glm::vec3& pos, Client* ws = nullptr) : id(id),
		aabb(model.aabb),
		ws(ws),
		debris_ref(debris),
		left({ 88.f, 3.f, 0.f }, glm::radians(55.f), 1.f, frame_count),
		right({ -88.f, 3.f, 0.f }, -glm::radians(55.f), 1.f, frame_count),
		bottom({ 0.f, -20.f, 0.f }, 0.f, 2.f, frame_count),
		body(pos, 0.f, model.layers.front()),
		turret(model.layers.back(), body.GetModel()),
		clear_color_blink(globals.clear_color_blink_rate) {
		debris_centrifugal_speed.resize(debris.vertices.size() / 6);
		debris_speed.resize(debris.vertices.size() / 6);
		Init();
	}
	/*auto GetPrevModel() const {
		return ::GetModel({}, prev_pos, prev_rot, layer.pivot);
	}*/
	void Kill(size_t id, size_t missile_id, int score, const glm::vec3& hit_pos, const glm::vec3& missile_vec) {
		if (!ws) return;
		Scor msg{ Tag("SCOR"), this->id, id, missile_id, score, hit_pos.x, hit_pos.y, missile_vec.x, missile_vec.y };
		ws->Send((const char*)&msg, sizeof(msg));
	}
	void Shoot(std::vector<Missile>& missiles) {
		if (ctrl != Ctrl::Turret && ctrl != Ctrl::Full) return;
		if (killed) return;
		const float missile_vel = 1.f;
		auto rot = turret.rest_pos + turret.rot + body.rot;
		glm::vec3 missile_vec{ std::cos(rot) * missile_vel, std::sin(rot) * missile_vel,.0f};

		auto start_pos = RotateZ(turret.missile_start_offset, body.layer.pivot, body.rot) + body.layer.pivot;
		missiles.emplace_back(body.pos + start_pos, rot, glm::length(missile_vec), this, ++missile_id );
		if (ws) {
			auto& m = missiles.back();
			Misl misl{ Tag("MISL"), id, m.id, m.pos.x, m.pos.y, m.rot, m.vel, m.life};
			globals.ws->Send((char*)&misl, sizeof(misl));
		}
	}
	void TurretControl(double x, double px) {
		if (ctrl != Ctrl::Turret && ctrl != Ctrl::Full) return;
		const double rot_ratio = (turret.max_rot - turret.min_rot) / globals.width;
		turret.SetRot(float((globals.width >> 1) - x) * rot_ratio);
		//turret.rot += float((px - x) * rot_ratio);
	}
	void Move(const Time& t, bool lt, bool rt, bool bt) {
		if (ctrl != Ctrl::Prop && ctrl != Ctrl::Full) return;
		left.Set(lt);
		right.Set(rt);
		bottom.Set(bt);
		const glm::vec3 vf = glm::vec3{ .0f, -1.f, 0.f } * force;
		const glm::vec3 force_l = glm::rotateZ(vf, glm::radians(-45.f)),
			force_r = glm::rotateZ(vf, glm::radians(45.f)),
			force_b = -vf;
		
		float sign = 0.f, limit;
		if (lt) {
			sign = 1.f;
			limit = rot_max_speed;
		}
		if (rt) {
			sign = -1.f;
			limit = -rot_max_speed;
		}
		if (!lt && !rt && std::abs(rot_speed) > 0.f) {
			sign = (rot_speed < 0.f) ? 1.f : -1.f;
			limit = 0.f;
		}
		if (sign>0.f)
			rot_speed = std::min(rot_speed + sign * rot_inc * (float)t.frame, limit);
		else if (sign<0.f)
			rot_speed = std::max(rot_speed + sign * rot_inc * (float)t.frame, limit);

		glm::vec3 val;
		if (lt) val += force_l;
		if (rt) val += force_r;
		if (bt) val += force_b;
		f = glm::rotateZ(val, body.rot);
	}
	void SetInvincibility() {
		invincible = globals.invincibility;
		globals.envelopes.push_back(std::unique_ptr<Envelope>(
			new Blink(visible, invincible, globals.timer.TotalMs(), globals.invincibility_blink_rate, 0.f)));
	}
	void SetCtrl(Ctrl ctrl) {
		SetInvincibility();
		this->ctrl = ctrl;
		pos_invalidated = false;
	}
	void Init() {
		SetInvincibility();
		fade_out = fade_out_time;
		globals.envelopes.push_back(std::unique_ptr<Envelope>(
			new Blink(blink, 0., globals.timer.TotalMs(), globals.blink_rate, globals.blink_ratio)));
		hit = killed = false;
	}
	void Die(const glm::vec3& hit_pos, std::list<Particles>& particles, const glm::vec3& missile_vec, double hit_time) {
		if (invincible > 0.f || hit || killed) return;
		hit = true;
		this->hit_pos = RotateZ(hit_pos, body.pos, -body.rot);
		this->hit_time = hit_time;
		debris = debris_ref;
		static std::uniform_real_distribution<float> dist_cfs(-debris_max_centrifugal_speed, debris_max_centrifugal_speed),
			dist_sp(min_debris_speed, max_debris_speed);
		for (auto& cfs : debris_centrifugal_speed) cfs = dist_cfs(mt);
		for (auto& sp : debris_speed) sp = dist_sp(mt);

		particles.push_back({ hit_pos,  missile_vec, hit_time });
	}
	bool IsInRestingPos(const AABB& bounds) const {
		return body.pos.y + aabb.b <= bounds.b + ground_level;
	}

	auto GenBBoxEdgesCCW() {
		//// TODO:: bbox not aabb
		//std::vector<glm::vec3> res;
		//AABBToBBoxEdgesCCW(body.aabb, res);
		//AABBToBBoxEdgesCCW(turret.aabb, res);
		//AABBToBBoxEdgesCCW(body.aabb.prev, res);
		//AABBToBBoxEdgesCCW(turret.aabb.prev, res);
		std::vector<glm::vec3> res;
		
		res.push_back(body.bbox.operator const BBox &()[0]);
		res.push_back(body.bbox.operator const BBox &()[1]);
		res.push_back(body.bbox.operator const BBox &()[1]);
		res.push_back(body.bbox.operator const BBox &()[2]);
		res.push_back(body.bbox.operator const BBox &()[2]);
		res.push_back(body.bbox.operator const BBox &()[3]);
		res.push_back(body.bbox.operator const BBox &()[3]);
		res.push_back(body.bbox.operator const BBox &()[0]);
		
		res.push_back(body.bbox.prev[0]);
		res.push_back(body.bbox.prev[1]);
		res.push_back(body.bbox.prev[1]);
		res.push_back(body.bbox.prev[2]);
		res.push_back(body.bbox.prev[2]);
		res.push_back(body.bbox.prev[3]);
		res.push_back(body.bbox.prev[3]);
		res.push_back(body.bbox.prev[0]);

		res.push_back(turret.bbox.operator const BBox &()[0]);
		res.push_back(turret.bbox.operator const BBox &()[1]);
		res.push_back(turret.bbox.operator const BBox &()[1]);
		res.push_back(turret.bbox.operator const BBox &()[2]);
		res.push_back(turret.bbox.operator const BBox &()[2]);
		res.push_back(turret.bbox.operator const BBox &()[3]);
		res.push_back(turret.bbox.operator const BBox &()[3]);
		res.push_back(turret.bbox.operator const BBox &()[0]);

		res.push_back(turret.bbox.prev[0]);
		res.push_back(turret.bbox.prev[1]);
		res.push_back(turret.bbox.prev[1]);
		res.push_back(turret.bbox.prev[2]);
		res.push_back(turret.bbox.prev[2]);
		res.push_back(turret.bbox.prev[3]);
		res.push_back(turret.bbox.prev[3]);
		res.push_back(turret.bbox.prev[0]);

		return res;
	}
	void Update(const Time& t, const AABB& bounds, std::list<Particles>& particles, bool player_self) {
		
		msg.clear();
		if (invincible > 0.f) {
			invincible -= (float)t.frame;
			if (invincible <= 0.f) {
				invincible = 0.f;
			}
		}
		else if (hit) {
			auto fade_time = (float)(t.total - hit_time) / fade_out_time;
			fade_out = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
			if ((killed = (fade_out <= 0.0001f))) return;
			left.on = right.on = bottom.on = false;
			if (clear_color_blink < 0.f)
				clear_color_blink += globals.clear_color_blink_rate;
			else
				clear_color_blink -= (float)t.frame;
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
		if (player_self && (ctrl == Ctrl::Full || ctrl == Ctrl::Prop)) {
			vel += ((f + g) / m) * (float)t.frame;
			vel.x = std::max(-max_vel, std::min(max_vel, vel.x));
			vel.y = std::max(-max_vel, std::min(max_vel, vel.y));
			body.pos += vel * (float)t.frame;
			//std::string str;
			//str += std::to_string(vel.x);
			//str += " ";
			//str += std::to_string(vel.y);
			//str += "\n";
			//LOG_INFO(str.c_str());
		}
		else if (!pos_invalidated) {
			////std::string str("!pos_invalidated ");
			////str += std::to_string(vel.x);
			////str += " ";
			////str += std::to_string(vel.y);
			////str += "\n";
			////LOG_INFO(str.c_str());
			//SetPos(pos + vel * (float)t.frame);
		}

		// ground constraint
		if (IsInRestingPos(bounds)) {
			body.pos.y = bounds.b + ground_level - aabb.b;
			if (std::abs(body.rot) > safe_rot && invincible <= 0.f) {
				glm::vec3 hit_pos = body.pos + glm::vec3{ 0.f, aabb.b, 0.f },
					vec{ 0.f, 1.f, 0.f };
				Die(hit_pos, particles, vec, t.total);
				--score;
				Kill(id, id, score, hit_pos, vec);
			} else {
				body.rot = 0.f;
				if (std::abs(vel.y) < 0.001f)
					vel.y = 0.f;
				else
					vel = { 0.f, -vel.y / 2.f, 0.f };
			}
		}
		else if (body.pos.y + aabb.t >= bounds.t) {
			body.pos.y = bounds.t - aabb.t;
			vel.y = 0.f;
		}
		if (!IsInRestingPos(bounds))
			body.rot += rot_speed * (float)t.frame;

		std::stringstream ss;
		ss << IsInRestingPos(aabb) << " " << rot_speed ;
		msg.push_back({ body.pos + glm::vec3{ 100.f, 0.f, 0.f }, 1.f, Text::Align::Left, ss.str() });

		{
			body.Update(t);
			turret.Update(t, body.GetModel());
			aabb = Union(Union(body.aabb, turret.aabb),
				Union(body.aabb.prev, turret.aabb.prev));
		}
		if (ws) {
			Plyr player{ Tag("PLYR"), id, body.pos.x, body.pos.y, turret.rot, invincible, vel.x, vel.y, body.rot, left.on, right.on, bottom.on };
			globals.ws->Send((char*)&player, sizeof(Plyr));
		}
	}
	float WrapAround(float min, float max) {
		float dif = body.pos.x - max;
		if (dif >= 0) {
			body.pos.x = min + dif;
			return dif;
		}
		dif = body.pos.x - min;
		if (dif < 0) {
			body.pos.x = max + dif;
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
struct Renderer {
	const Asset::Assets& assets;
	RT rt;
	Shader::Color colorShader;
	Shader::ColorPosAttrib colorPosAttribShader;
	//	Shader::Texture textureShader;
	Shader::TextureColorMod textureColorModShader;
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
		VBO_EDGES = 13,
		VBO_COUNT = 14;
	GLuint vbo[VBO_COUNT];
	std::vector<GLuint> tex;
	glm::vec4 clearColor;
#ifdef VAO_SUPPORT
	GLuint vao;
#endif
	struct Missile {
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
	struct StarField {
		size_t count_per_layer;
		AABB bounds;
		GLuint vbo;
		size_t count;
		struct Layer {
			float z;
			size_t color_idx;
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
		rt(globals.width, globals.height),
		clearColor(0.f, .0f, .0f, 1.f) {
		Init(scene_bounds);
	}
	void Init(const AABB& scene_bounds) {
		glDisable(GL_DEPTH_TEST);
		if (!assets.images.empty()) {
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
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		rt.GenRenderTargets();


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
				texture_data[idx++] = 0xff;// u_dist(mt);
				texture_data[idx++] = 0xff;// u_dist(mt);
				texture_data[idx++] = 0xff;// u_dist(mt);
				//texture_data[idx++] = u_dist(mt);
				//texture_data[idx++] = u_dist(mt);
				//texture_data[idx++] = u_dist(mt);
				texture_data[idx] = 255;
			}
			//texture_data[0] = 0;// u_dist(mt);
			//texture_data[1] = 0;// u_dist(mt);
			//texture_data[2] = 255;// u_dist(mt);
			//texture_data[3] = 255;
			//texture_data[(size - 1)  * 4] = 255;// u_dist(mt);
			//texture_data[(size - 1) * 4 + 1] = 0;// u_dist(mt);
			//texture_data[(size - 1) * 4 + 2] = 0;// u_dist(mt);
			//texture_data[(size - 1) * 4 + 3] = 255;

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
			const float star_size = globals.dot_size;
			const size_t count_per_layer = 3000, layer_count = 3;
			std::vector<glm::vec3> data;
			data.reserve(count_per_layer *layer_count * 6);
			float mul = 2.f;
			for (size_t j = 0; j < layer_count; ++j) {
				std::uniform_real_distribution<> dist_x(scene_bounds.l * mul,
					scene_bounds.r * mul),
					dist_y(0.f/*scene_bounds.b * mul*/, scene_bounds.t * mul);
				mul -= .5f;
				for (size_t i = 0; i < count_per_layer; ++i) {
					GenerateSquare((float)dist_x(mt), (float)dist_y(mt), star_size, data);
				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_STARFIELD]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), &data.front(), GL_STATIC_DRAW);
			starField = { count_per_layer, scene_bounds, vbo[VBO_STARFIELD], count_per_layer * layer_count, {1.f, 0}, {.5f, 0}, {.25f, 0 } };
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new SequenceAsc(starField.layer1.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer1_blink_rate, starField.layer1.color_idx, globals.grey_palette.size(), 1, 1)));
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new SequenceAsc(starField.layer2.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer2_blink_rate, starField.layer2.color_idx, globals.grey_palette.size(), 1, 1)));
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new SequenceAsc(starField.layer3.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer3_blink_rate, starField.layer3.color_idx, globals.grey_palette.size(), 1, 1)));
		}

		{
			// particle
			glBindBuffer(GL_ARRAY_BUFFER, Particles::vbo = vbo[VBO_PARTICLE]);
			std::vector<glm::vec3> data;
			GenerateSquare(0.f, 0.f, globals.dot_size, data);
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
			// radar
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
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
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
		auto col = globals.grey_palette[starField.layer3.color_idx];
		glUniform4f(shader.uCol, col.r, col.g, col.b, 1.f);
		glDrawArrays(GL_TRIANGLES, starField.count_per_layer * 6 * 2, starField.count_per_layer);
		pos = cam.pos * starField.layer2.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		col = globals.grey_palette[starField.layer2.color_idx];
		glUniform4f(shader.uCol, col.r, col.g, col.b, 1.f);
		glDrawArrays(GL_TRIANGLES, starField.count_per_layer * 6 * 1, starField.count_per_layer);
		pos = cam.pos * starField.layer1.z;
		mvp = glm::translate(cam.vp, pos);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		col = globals.grey_palette[starField.layer1.color_idx];
		glUniform4f(shader.uCol, col.r, col.g, col.b, 1.f);
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

	void Draw(const Camera& cam, const std::vector<glm::vec3>& edges, const glm::vec4& col = {1.f, 1.f, 1.f, 1.f}) {
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_EDGES]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * edges.size(), edges.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		const auto& shader = colorShader;
		glUseProgram(shader.id);
		const glm::mat4& mvp = cam.vp;
		glUniform4f(shader.uCol, col.r, col.g, col.b, col.a);
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		glDrawArrays(GL_LINES, 0, edges.size());
		glDisableVertexAttribArray(0);
	}
	void Draw(const Camera& cam, const ProtoX& proto, const ProtoX::Propulsion& prop, const glm::mat4& parent_model) {
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
			glm::mat4 mvp = cam.vp * prop.GetModel(parent_model);
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
		const auto m = proto.body.GetModel();
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
			glm::mat4 mvp = cam.vp * m;
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
		const glm::mat4 mvp = cam.vp * m;
		glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		float prop_blink = 1.f, turret_blink = 1.f;
		if (player) {
			if (proto.ctrl == ProtoX::Ctrl::Full || proto.ctrl == ProtoX::Ctrl::Prop)
				prop_blink = proto.blink;
			if (proto.ctrl == ProtoX::Ctrl::Full || proto.ctrl == ProtoX::Ctrl::Turret)
				turret_blink = proto.blink;
		}
		Draw<GL_TRIANGLES>(shader.uCol, proto.body.layer.parts, prop_blink);
		Draw<GL_LINES>(shader.uCol, proto.body.layer.line_parts, prop_blink);
		{ 
			const glm::mat4 mvp = cam.vp * proto.turret.GetModel(m);
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			Draw<GL_TRIANGLES>(shader.uCol, proto.turret.layer.parts, turret_blink);
			Draw<GL_LINES>(shader.uCol, proto.turret.layer.line_parts, turret_blink);
		}
		Draw(cam, proto, proto.left, m);
		Draw(cam, proto, proto.right, m);
		Draw(cam, proto, proto.bottom, m);
		Draw(cam, proto.msg);
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
		const float score_scale = 2.f, 
			border = 12.f, 
			text_y = -assets.text.aabb.t - assets.text.aabb.b;
		Draw(cam, vbo[VBO_RADAR], {/*radar pos*/}, assets.radar);
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
		const auto col = globals.radar_player_color * player->blink;
		glUniform4f(shader.uCol, col.r, col.g, col.b, col.a);
		const AABB& aabb = assets.radar.aabb;
		const auto rw = aabb.r - aabb.l, rh = aabb.t - aabb.b, sw = bounds.r - bounds.l, sh = bounds.t - bounds.b,
			rx = rw / sw, ry = rh / sh;

		const glm::vec3 pos(player->body.pos.x * rx, (player->body.pos.y - bounds.b) * ry + aabb.b, 0.f);
		glVertexAttrib3fv(shader.aVertex, &(pos)[0]);
		glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);

		glUniform4f(shader.uCol, globals.radar_enemy_color.r, globals.radar_enemy_color.g, globals.radar_enemy_color.b, globals.radar_enemy_color.a);
		int max = 0;
		for (const auto& p : players) {
			max = std::max(max, p.second->score);
			const glm::vec3 pos(p.second->body.pos.x * rx, (p.second->body.pos.y - bounds.b) * ry + aabb.b, 0.f);
			glVertexAttrib3fv(shader.aVertex, &(pos)[0]);
			glDrawArrays(GL_TRIANGLES, 0, Particles::vertex_count);
		}

		std::vector<Text> texts;
		const glm::vec3 pos1{ aabb.l + border, text_y, 0.f }, pos2 = { aabb.r + border, text_y, 0.f };
		texts.push_back({ pos1, score_scale, Text::Align::Right, std::to_string(player->score) });
		texts.push_back({pos2, score_scale, Text::Align::Left, std::to_string(max) });
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
		auto& shader = textureColorModShader;
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
		glUniform1i(shader.uSmp, 0);
		
		const auto vp = cam.proj * cam.view;
		for (const auto& missile : missiles) {
			if (!missile.owner->visible) continue;
			const glm::mat4 mvp = glm::rotate(glm::translate(vp, missile.pos), missile.rot, { 0.f, 0.f, 1.f });
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			const auto& col = globals.palette[missile.col_idx];
			glUniform4f(shader.uCol, col.r, col.g, col.b, 1.f);
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
		auto w = assets.text.aabb.r - assets.text.aabb.l;
		for (const auto& str : texts) {
			auto pos = str.pos;
			const auto incr = (w + globals.text_spacing) * str.scale;
			if (str.align == Text::Align::Right || str.align == Text::Align::Center) {
				auto ofs = incr * str.str.size() ;
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
				pos.x += incr;
			}
		}
	}
	~Renderer() {
		glDeleteBuffers(sizeof(vbo)/sizeof(vbo[0]), vbo);
		if (!tex.empty())
			glDeleteTextures(tex.size(), &tex.front());
#ifdef VAO_SUPPORT
		glDeleteVertexArrays(1, &vao);
#endif
	}
};

struct InputHandler {
	enum class Keys{Left, Right, W, A, D, Count};
	enum class ButtonClick{LB, RB, MB};
	static std::function<void(int key, int scancode, int action, int mods)> keyCb;
	static bool keys[(size_t)Keys::Count];
	static double x, y, px, py;
	static bool lb, rb, mb;
	static bool update;
	static std::queue<ButtonClick> event_queue;
#ifdef __EMSCRIPTEN__
	// TODO:: static constexpr
	#define NUMTOUCHES 4u
	struct TouchEvent{
		int eventType;
		size_t n;
		EmscriptenTouchPoint touchPoints[NUMTOUCHES];
	};
	static std::queue<TouchEvent> touch_event_queue;
	static EM_BOOL touchstart_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
	{
		touch_event_queue.push({ eventType, std::min((size_t)e->numTouches, NUMTOUCHES)});
		auto& front = touch_event_queue.front();
		memcpy(front.touchPoints, e->touches, sizeof(EmscriptenTouchPoint) * front.n);
		return 0;
	}
	static EM_BOOL touchend_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
	{
		return touchstart_callback(eventType, e, userData);
	}
	static EM_BOOL touchmove_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
	{
		return touchstart_callback(eventType, e, userData);
	}
	static EM_BOOL touchcancel_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
	{
		return touchstart_callback(eventType, e, userData);
	}
#endif
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
		//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
#ifdef __EMSCRIPTEN__
std::queue<InputHandler::TouchEvent> InputHandler::touch_event_queue;
#endif

class Scene {
public:
	Asset::Assets assets;
	AABB bounds;
	Shader::Simple simple;
	Renderer renderer;
	std::unique_ptr<ProtoX> player;
	std::vector<Missile> missiles;
	std::map<size_t, std::unique_ptr<ProtoX>> players;
	std::list<Particles> particles;
	std::vector<Text> texts;
	Object mesh{ { 5.f, 0.f, 0.f } };
	Camera camera{ (int)globals.width, (int)globals.height }, hud{ (int)globals.width, (int)globals.height },
		overlay;
	InputHandler inputHandler;
	std::mutex msgMutex;
	//GLuint VertexArrayID;
	//GLuint vertexbuffer;
	//GLuint uvbuffer;
	//GLuint texID;
	//GLuint uTexSize;
	std::queue<std::vector<unsigned char>> messages;
	int wait = 0;
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
	glm::vec3 RandomizePos(const AABB& asset_aabb) {
		const AABB aabb{ bounds.l - asset_aabb.l, bounds.t - asset_aabb.t, bounds.r - asset_aabb.r, bounds.b - asset_aabb.b };
		std::uniform_real_distribution<> xdist(aabb.l, aabb.r), ydist(aabb.b, aabb.t);
		return{ xdist(mt), ydist(mt), 0.f };
	}
	void GenerateNPC() {
		static size_t i = 0;
		auto p = std::make_unique<ProtoX>((size_t)0xbeef, assets.probe, assets.debris, assets.propulsion.layers.size(), RandomizePos(assets.probe.aabb));
		std::uniform_real_distribution<> rot_dist(0., glm::degrees(glm::two_pi<float>()));
		p->body.rot = rot_dist(mt);
		p->turret.SetRot(rot_dist(mt));
		players[++i] = std::move(p);
	}
	void GenerateNPCs() {
		for (size_t i = 0; i<MAX_NPC; ++i) {
			GenerateNPC();
		}
	}
	void SetCtrl(ProtoX::Ctrl ctrl) {
		if (!player) return;
		player->SetCtrl(ctrl);
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Prop)
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new Blink(renderer.wsad_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio)));
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Turret)
			globals.envelopes.push_back(std::unique_ptr<Envelope>(new Blink(renderer.mouse_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio)));
	}
	Scene() : bounds(assets.land.aabb),
//#ifndef __EMSCRIPTEN__
#ifdef DEBUG_REL
	player(std::make_unique<ProtoX>(0xdeadbeef, assets.probe, assets.debris, assets.propulsion.layers.size(), glm::vec3{})),
#endif
//#endif
		renderer(assets, bounds),
		overlay(camera) {
		const auto size = renderer.rt.GetCurrentRes();
		SetHudVp(size.x, size.y);
		//texts.push_back(Text{ {}, 1.f, Text::Align::Center, "A !\"'()&%$#0123456789:;<=>?AMKXR" });
		/*bounds.t = float((height >> 1) + (height >> 2));
		bounds.b = -float((height >> 1) + (height >> 2));*/
#ifdef DEBUG_REL
		SetCtrl(ProtoX::Ctrl::Full);
		GenerateNPCs();
#endif
		inputHandler.keyCb = std::bind(&Scene::KeyCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

		mesh.model = glm::translate(mesh.model, mesh.pos);
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
			renderer.Draw(camera, p.second->GenBBoxEdgesCCW());
			renderer.Draw(camera, p.second->aabb);
		//	renderer.Draw(camera, p.second->aabb.Translate(p.second->pos));
		}
		if (player) {
			renderer.Draw(camera, *player.get(), true);
			renderer.Draw(camera, player->GenBBoxEdgesCCW(), { .3f, 1.f, .3f, 1.f });
			renderer.Draw(camera, player->aabb);
		}
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
		//}else if (key == GLFW_KEY_HOME && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
		//	renderer.rt.maskOpacity+=.05f;
		//}else if (key == GLFW_KEY_END && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
		//	renderer.rt.maskOpacity-=.05f;
		if (key == GLFW_KEY_LEFT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.contrast -= .05f;
		} else if (key == GLFW_KEY_RIGHT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.contrast += .05f;
		} else if (key == GLFW_KEY_COMMA && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.brightness -= .05f;
		} else if (key == GLFW_KEY_PERIOD && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.brightness += .05f;
		}
		else if (key == GLFW_KEY_F9 && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			renderer.rt.shadowMask.Reload();
		}

	}
	void Update(const Time& t) {
		const double scroll_speed = .5; // px/s
		if (inputHandler.keys[(size_t)InputHandler::Keys::Left])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.keys[(size_t)InputHandler::Keys::Right])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		texts.clear();
		if (wait > 0) {
			std::stringstream ss;
			ss << "WAITING FOR " << wait << " MORE PLAYER" << ((wait == 1) ? "":"S");
			texts.push_back({ {}, .8f, Text::Align::Center, ss.str() });
			ss.str("");
			ss << "SHARE THE URL IN THE ADDRESS BAR";
			texts.push_back({ {0.f,  -(assets.text.aabb.t - assets.text.aabb.b), 0.f}, .8f, Text::Align::Center, ss.str() });
		}
		if (player) {
			bool touch = false;
#ifdef __EMSCRIPTEN__
			while (!inputHandler.touch_event_queue.empty()) {
				auto e = inputHandler.touch_event_queue.back();
				inputHandler.touch_event_queue.pop();
				bool d = e.touchPoints[0].clientX < (globals.width / 3),
					w = e.touchPoints[0].clientX >= (globals.width / 4) && e.touchPoints[0].clientX <= (globals.width * 4 / 5),
					a = e.touchPoints[0].canvasX > (globals.width * 2 / 3),
					shoot = e.touchPoints[0].clientY < (globals.height / 2);
				//LOG_INFO("%d %d %d %d %d %d\n", e.touchPoints[0].clientX, e.touchPoints[0].clientY, globals.width, globals.width / 4, globals.width * 4 / 5, w);
				player->Move(t, a, d, w);
				if (shoot)
					player->Shoot(missiles);
				touch = true;
			}
#endif
			if (!touch) {
				if (inputHandler.update) player->TurretControl(inputHandler.x, inputHandler.px);
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
			}
			player->Update(t, bounds, particles, true);
		}
		for (auto& p : players) {
			p.second->Update(t, bounds, particles, false);
		}

		for (auto p = std::begin(players); p != std::end(players);) {
			if (p->second->killed) {
				players.erase(p++);
			}
			else ++p;
		}
		if (player->hit && player->clear_color_blink < 0.f) {
			std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);
			renderer.clearColor = globals.palette[col_idx_dist(mt)];
		}
		if (player && player->killed) {
			auto ctrl = player->ctrl;
			auto score = player->score;
			player = std::make_unique<ProtoX>(player->id, assets.probe, assets.debris, assets.propulsion.layers.size(), RandomizePos(player->aabb), globals.ws.get());
			renderer.clearColor = { 0.f, 0.f, 0.f, 1.f };
			SetCtrl(ctrl);
			player->score = score;
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
			auto d = camera.pos.x + player->body.pos.x;
			auto res = player->WrapAround(assets.land.layers[0].aabb.l, assets.land.layers[0].aabb.r);
			camera.Update(t);
			camera.Tracking(player->body.pos, bounds, player->aabb);
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
				if (m.life<=0.f || m.IsOutOfBounds(bounds)) {
					if (RemoveMissile(m)) break;
					continue;
				}
				else {
					//invincible players can't kill
					if (m.owner->invincible > 0.f) {
						++it;
						continue;	
					}
					// TODO:: test all hits or just ours?
					if (m.owner != player.get()) {
						++it;
						continue;
					}
					bool last = false;
					for (const auto& p : players) {
						// if (m.owner != p.second.get()) continue;
						glm::vec3 hit_pos;
						if (!p.second->hit && !p.second->killed && p.second->invincible == 0.f && m.HitTest(p.second->aabb/*.Translate(p.second->body.pos)*/, hit_pos)) {
							glm::vec3 vec{ std::cos(m.rot), std::sin(m.rot), 0.f };
							p.second->Die(hit_pos, particles, vec, (double)t.total);
							++player->score;
							player->Kill(p.second->id, m.id, player->score, hit_pos, vec);
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
		// reset pos_invalidated
		if (player)
			player->pos_invalidated = false;
		for (auto& p : players) {
			p.second->pos_invalidated = false;
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
		LOG_INFO("Socket error: %d  %s\n", code, msg);
	}
	void OnClose() {
		LOG_INFO("Socket closed\n");
	}
	void OnOpen() {
		SendSessionID();
	}
	void OnMessage(const char* msg, int len) {
		//LOG_INFO("OnMessage thread id: %d", std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::lock_guard<std::mutex> lock(msgMutex);
		messages.push(std::vector<unsigned char>{ msg, msg + (size_t)len });
	}
	void OnConn(const std::vector<unsigned char>& msg) {
		const Conn* conn = reinterpret_cast<const Conn*>(msg.data());
		size_t id = ID5(conn->client_id, 0);
		player = std::make_unique<ProtoX>(id, assets.probe, assets.debris, assets.propulsion.layers.size(), RandomizePos(assets.probe.aabb), globals.ws.get());
		player->ctrl = static_cast<ProtoX::Ctrl>(conn->ctrl - 48/*TODO:: eliminate conversion*/);
	}
	void SendSessionID() {
		Sess msg{ Tag("SESS") };
		std::copy(std::begin(globals.sessionID), std::end(globals.sessionID), msg.sessionID);
		globals.ws->Send((char*)&msg, sizeof(msg));
	}
	void OnPlyr(const std::vector<unsigned char>& msg) {
		const Plyr* player = reinterpret_cast<const Plyr*>(&msg.front());
		if (this->player->id == player->id) {
			if (this->player->ctrl == ProtoX::Ctrl::Prop) this->player->turret.SetRot(player->turret_rot);
			else if (this->player->ctrl == ProtoX::Ctrl::Turret) {
				this->player->pos_invalidated = true;
				this->player->body.pos.x = player->x; this->player->body.pos.y = player->y;
				this->player->body.rot = player->rot;
				this->player->vel.x = player->vx; this->player->vel.y = player->vy;
				this->player->left.on = player->prop_left; this->player->right.on = player->prop_right;  this->player->bottom.on = player->prop_bottom;
			}
			return;
		}
		auto it = players.find(player->id);
		ProtoX * proto;
		if (it == players.end()) {
			auto ptr = std::make_unique<ProtoX>(player->id, assets.probe, assets.debris, assets.propulsion.layers.size(), glm::vec3{ player->x, player->y, 0.f });
			proto = ptr.get();
			players[player->id] = std::move(ptr);
		}
		else {
			proto = it->second.get();
			proto->body.pos.x = player->x; proto->body.pos.y = player->y;
		}
		proto->body.rot = player->rot;
		proto->turret.SetRot(player->turret_rot); proto->invincible = player->invincible;
		proto->vel.x = player->vx; proto->vel.y = player->vy;
		proto->left.on = player->prop_left; proto->right.on = player->prop_right;  proto->bottom.on = player->prop_bottom;
		proto->pos_invalidated = true;
	}
	void OnMisl(const std::vector<unsigned char>& msg) {
		const Misl* misl = reinterpret_cast<const Misl*>(&msg.front());
		ProtoX* proto = nullptr;
		if (player->id == misl->player_id) proto = player.get();
		else {
			auto it = players.find(misl->player_id);
			if (it != players.end()) proto = it->second.get();
		}
		if (!proto) return;
		missiles.push_back({ { misl->x, misl->y, 0.f }, misl->rot, misl->vel, proto, misl->missile_id });
		missiles.back().life = misl->life;
	}
	void OnScor(const std::vector<unsigned char>& msg, const Time& t) {
		if (!player) return;
		const Scor* scor = reinterpret_cast<const Scor*>(&msg.front());
		auto it = std::find_if(missiles.begin(), missiles.end(), [=](const Missile& m) {
			return m.owner->id == scor->owner_id && m.id == scor->missile_id;});
		if (it != missiles.end()) {
			it->owner->score = scor->score;
			RemoveMissile(*it);
		}
		if (scor->owner_id == player->id && player->ctrl == ProtoX::Ctrl::Prop) player->score = scor->score;
		const::glm::vec3 hit_pos(scor->x, scor->y, 0.f),
			missile_vec{ scor->vec_x, scor->vec_y, 0.f };
		if (scor->target_id == player->id)
			player->Die(hit_pos, particles, missile_vec, t.total);
		else {
			auto it = players.find(scor->target_id);
			if (it != players.end())
				it->second->Die(hit_pos, particles, missile_vec, t.total);
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
		auto hit_pos = p->body.pos + glm::vec3{ p->aabb.r - p->aabb.l, p->aabb.t - p->aabb.b, 0.f };
		p->Die(hit_pos, particles, {}, t.total);
	}
	void OnCtrl(const std::vector<unsigned char>& msg) {
		const Ctrl* ctrl = reinterpret_cast<const Ctrl*>(&msg.front());
		SetCtrl(static_cast<ProtoX::Ctrl>(ctrl->ctrl - 48/*TODO:: eliminate conversion*/));
	}
	void OnWait(const std::vector<unsigned char>& msg) {
		auto wait = reinterpret_cast<const Wait*>(&msg.front());
		this->wait = wait->n;
		LOG_INFO("OnWait: %d", wait->n);
	}
	void Dispatch(const std::vector<unsigned char>& msg, const Time& t) {
		size_t tag = Tag(msg);
		constexpr size_t conn = Tag("CONN"), // str clientID
			kill = Tag("KILL"),	// str clientID
			plyr = Tag("PLYR"), // clientID, pos.x, pos.y, invincible
			misl = Tag("MISL"), // clientID, pos.x, pos.y, v.x, v.y
			scor = Tag("SCOR"), // clientID, clientID
			ctrl = Tag("CTRL"), // other str clientID , upper = '1' / lower = '0'
			wait = Tag("WAIT");	// n - number to wait for
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
		case wait:
			OnWait(msg);
			break;
		}
	}
	void ProcessMessages(const Time& t) {
		//LOG_INFO("ProcessMessage thread id: %d", std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::lock_guard<std::mutex> lock(msgMutex);
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
		LOG_INFO("SessionID is: %s\n", argv[4]);
	}
	init(globals.width, globals.height);

	try {
		globals.timer.Tick();
		globals.scene = std::make_unique<Scene>();
	}
	catch (const custom_exception& ex) {
		LOG_INFO("main loop exception: %s\n", ex.what());
		throw;
	}
#ifdef __EMSCRIPTEN__
	auto ret = emscripten_set_touchstart_callback(0, 0, 1, InputHandler::touchstart_callback);
	if (ret < 0)
		LOG_ERR(ret, "emscripten_set_touchstart_callback failed");
	ret = emscripten_set_touchend_callback(0, 0, 1, InputHandler::touchend_callback);
	if (ret < 0)
		LOG_ERR(ret, "emscripten_set_touchend_callback failed");
	ret = emscripten_set_touchmove_callback(0, 0, 1, InputHandler::touchmove_callback);
	if (ret < 0)
		LOG_ERR(ret, "emscripten_set_touchmove_callback failed");
	ret = emscripten_set_touchcancel_callback(0, 0, 1, InputHandler::touchcancel_callback);
	if (ret < 0)
		LOG_ERR(ret, "emscripten_set_touchcancel_callback failed");

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
//		::Sleep(100);
//	}
//	catch (...) {
//	LOG_INFO("exception has been thrown\n");
//		throw;
//	}
}