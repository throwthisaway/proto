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
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include <string>
#include <memory>
//#include <mutex>
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
#include "audio.h"
#include "Command.h"
//#define OBB_TEST
// TODO::
// - on die send kill
// - decrease score on suicide for other player too
// -------------------------------------
// - move calculations from Move to Update
// - culling after camrea update
// - draw debris on top of everything
// - fix audio updqate before camera update
// - culling particles, debris, players, missiles
// - introduce npc flag, overhaul player_self flag, add npc behaviour
//----------------------------------------
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
template<typename T>
inline bool SanitizeMsg(size_t size) {
	//return size + size % 4 == sizeof(T);
	return true;
}
static float NDCToGain(float x) {
	x = std::abs(x);
	return 1.f - glm::smoothstep(1.f, 3.f, x);
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
	const float invincibility = 1000.f;
#else
	const float invincibility = 5000.f;
#endif
	const float	scale = 40.f,
		radar_scale = 75.f,
		propulsion_scale = 80.f,
		text_spacing = 10.f,
		text_scale = 120.f,
		missile_size = 240.f,
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
		missile_life = 1000., // ms
		dot_size = 6.f, // px
		clear_color_blink_rate = 16.f, //ms
		look_ahead_ratio = .3f,
		camera_z = -10.f,
		missile_start_offset_y = 1.f * scale,
		foreground_pos_x_ratio = 1.f,
		player_fade_out_time = 3500.f, // ms;
		msg_interval = 0.f; //ms
	const size_t max_missile = 3;
	int ab = a;
	const glm::vec4 radar_player_color{ 1.f, 1.f, 1.f, 1.f }, radar_enemy_color{ 1.f, .5f, .5f, 1.f };
	const gsl::span<const glm::vec4, gsl::dynamic_range>& palette = pal, &grey_palette = grey_pal;
	Timer timer;
	std::string host = "localhost";
	unsigned short port = 8000;
	int width = 1024, height = 768;
	std::string sessionID;
	std::unique_ptr<Audio> audio;
	std::unique_ptr<Randomizer<std::array<ALuint, Audio::PEW_COUNT>>> randomizer;
	std::unique_ptr<Scene> scene;
	std::unique_ptr<Client> ws;
	std::vector<std::weak_ptr<Envelope>> envelopes;
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
		Model probe, propulsion, land, missile, debris, text, debug, radar, mouse, wsad, foreground;
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
				MeshLoader::Mesh mesh;
				ReadMeshFile(PATH_PREFIX"asset//foreground.mesh", mesh);
				foreground = Reconstruct(mesh, globals.scale);
				models.push_back(&foreground);
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
	glm::mat4 view, proj, vp, ivp;
	glm::vec4 d;
	void SetPos(float x, float y, float z) {
		d = {};
		pos = { x, y, z };
		view = glm::translate(pos);
		vp = proj * view;
		ivp = glm::inverse(vp);
	}
	void Translate(float x, float y, float z) {
		pos.x += x; pos.y += y; pos.z += z;
		view = glm::translate(pos);
		vp = proj * view;
		ivp = glm::inverse(vp);
	}
	void SetProj(int w, int h) {
		proj = glm::ortho((float)(-(w >> 1)), (float)(w >> 1), (float)(-(h >> 1)), (float)(h >> 1), 0.1f, 100.f);
		vp = proj * view;
		ivp = glm::inverse(vp);
	}
	Camera(int w, int h) : view(glm::translate(pos)) {
		SetProj(w, h);
	}
	glm::vec4 NDC(const glm::vec3& pos) const {
		return vp * glm::vec4(pos, 1.f);
	}
	glm::vec4 EaseOut(const glm::vec4& d, const glm::vec4& current, const glm::vec4& limit) {
		const auto ratio = glm::vec4(1.f, 1.f, 0.f, 0.f) - glm::abs(current / limit);
		const auto r = ratio * ratio * ratio * ratio;
		return d;// *r;
	}
	void Update(const Time& t, const AABB& scene_aabb, const glm::vec3& player_pos, const glm::vec3& vel) {
		const auto ip = glm::inverse(proj);
		const auto len = glm::length(vel);
		if (len > 0.f) {
			// TODO:: make them const
			glm::vec4 bl(-globals.tracking_width_ratio, -globals.tracking_width_ratio, 0.f, 0.f),
				tr(globals.tracking_width_ratio, globals.tracking_width_ratio, 0.f, 0.f);
			bl = ip * bl;
			tr = ip * tr;
			const auto vel_n = vel / len;
			auto dist = glm::vec4(pos + player_pos, 0.f);
			dist.z = 0.f;
			auto ratio = dist / (tr - bl) * 2.f;
			ratio = 1.f - glm::abs((ratio + glm::vec4(vel_n, 0.f)) / 2.f);
			d -= glm::vec4(vel_n * (float)t.frame * globals.look_ahead_ratio, 0.f) * (1.f - ratio * ratio * ratio);
			d = glm::clamp(d, bl, tr);
			d.z = 0.f;
		}
		pos = glm::vec3(-player_pos.x,
			-player_pos.y, globals.camera_z) + glm::vec3(d);

		// scene bound constraints
		// TODO:: make them const
		glm::vec4 bl = { -1.f, -1.f, 0.f, 0.f },
			tr = { 1.f, 1.f, 0.f, 0.f };
		bl = ip * bl;
		tr = ip * tr;
		const auto snap_bl = glm::vec3(scene_aabb.l, scene_aabb.b, 0.f) - glm::vec3(bl),
			snap_tr = glm::vec3(scene_aabb.r, scene_aabb.t, 0.f) - glm::vec3(tr);

		const auto temp = pos;
		pos = glm::clamp(pos, -snap_tr, -snap_bl);
		pos.z = globals.camera_z;
		view = glm::translate(pos);
		vp = proj * view;
		ivp = glm::inverse(vp);
		return;
		//// TODO:: either make proto::aabb local, or refactor this
		//player_aabb.l -= tracking_pos.x;
		//player_aabb.t -= tracking_pos.y;
		//player_aabb.r -= tracking_pos.x;
		//player_aabb.b -= tracking_pos.y;
		//const glm::vec4 player_tl = vp * (glm::vec4(player_aabb.l, player_aabb.t, 0.f, 0.f)),
		//	player_br = vp * (glm::vec4(player_aabb.r, player_aabb.b, 0.f, 0.f));
		//const auto tracking_screen_pos = vp * glm::vec4(tracking_pos, 1.f);
		//glm::vec4 d;
		//if (tracking_screen_pos.x + player_tl.x < -globals.tracking_width_ratio)
		//	d.x = tracking_screen_pos.x + globals.tracking_width_ratio + player_tl.x;
		//else if (tracking_screen_pos.x + player_br.x > globals.tracking_width_ratio)
		//	d.x = tracking_screen_pos.x - globals.tracking_width_ratio + player_br.x;
		//if (tracking_screen_pos.y + player_br.y < -globals.tracking_height_ratio)
		//	d.y = tracking_screen_pos.y + globals.tracking_height_ratio + player_br.y;
		//else if (tracking_screen_pos.y + player_tl.y > globals.tracking_height_ratio)
		//	d.y = tracking_screen_pos.y - globals.tracking_height_ratio + player_tl.y;

		//pos -= glm::vec3(glm::inverse(vp) * d);
		//view = glm::translate(pos);
		//vp = proj * view;

		//const glm::vec4 scene_tl = vp * (glm::vec4(scene_aabb.l, scene_aabb.t, 0.f, 1.f)),
		//	scene_br = vp * (glm::vec4(scene_aabb.r, scene_aabb.b, 0.f, 1.f));
		//d = {};
		//if (scene_br.y > -1.f)
		//	d.y = scene_br.y + 1.f;
		//else if (scene_tl.y < 1.f)
		//	d.y = scene_tl.y - 1.f;
		//if (scene_tl.x > -1.f)
		//	d.x = scene_tl.x + 1.f;
		//else if (scene_br.x < 1.f)
		//	d.x = scene_br.x - 1.f;
		//pos -= glm::vec3(glm::inverse(vp) * d);
		//view = glm::translate(pos);
		//vp = proj * view;
	}
};

struct ProtoX;
struct Missile {
	// TODO:: Val<glm::vec3> pos
	glm::vec3 pos, prev, vec, rot_v;
	float rot, vel;
	ProtoX* owner;
	size_t id, col_idx;
	float life, blink;
	bool remove = false, first = true;
	Audio::Source pew;
	Missile& operator=(const Missile&) = default;
	Missile(const glm::vec3 pos, float rot, float vel, ProtoX* owner, size_t id, const glm::vec3& vec) : pos(pos), prev(pos), vec(vec),
		rot_v(glm::vec3{ std::cos(rot), std::sin(rot), 0.f }), rot(rot), vel(vel), owner(owner), id(id), life(globals.missile_life), blink(globals.missile_blink_rate),
		pew(globals.audio->GenSource(globals.randomizer->Gen())) {
		GenColIdx();
	}
	//~Missile() {
	//	++owner->missile_count;
	//}
	void GenColIdx() {
		std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);
		col_idx = col_idx_dist(mt);
	}
	void Update(const Time& t, const Camera& cam, const AABB& scene_bounds) {
		if (life <= 0.f || IsOutOfBounds(scene_bounds)) {
			remove = true;
			return;
		}
		life -= (float)t.frame;
		prev = pos;
		pos += rot_v * vel * (float)t.frame + vec;
		blink -= (float)t.frame;
		if (blink < 0.f) {
			blink += globals.missile_blink_rate;
			GenColIdx();
		}
		const auto& ndc = cam.NDC(pos);
		if (first) {
			globals.audio->Enqueue(Audio::Command::ID::Start, pew, glm::clamp(ndc.x, -1.f, 1.f), ::NDCToGain(ndc.x));
			first = false;
		}
		else
			globals.audio->Enqueue(Audio::Command::ID::Ctrl, pew, glm::clamp(ndc.x, -1.f, 1.f), ::NDCToGain(ndc.x));
	}
	auto End() const {
		return rot_v * globals.missile_size + pos;
	}
	bool Cull(const glm::mat4& vp) const {
		const auto end = End();
		glm::vec4 tl(std::min(end.x, pos.x), std::max(end.y, pos.y), 0.f, 1.f);
		tl = vp * tl;
		if (tl.x > 1.f || tl.y < -1.f) return true;
		glm::vec4 br(std::max(end.x, pos.x), std::min(end.y, pos.y), 0.f, 1.f);
		br = vp * br;
		if (br.x < -1.f || br.y > 1.f) return true;
		return false;
	}
	bool HitTest(const AABB& bounds, glm::vec3& hit_pos) const {
		if (pos == prev) return false;
		/* AABB intersection
		const glm::vec3 end = glm::vec3{ std::cos(rot), std::sin(rot), 0.f } * size + pos;
		const glm::vec2 min{ std::min(end.x, pos.x), std::min(end.y, pos.y) }, max{ std::max(end.x, pos.x), std::max(end.y, pos.y) };
		const AABB aabb_union{ std::min(min.x, bounds.l), std::max(max.y, bounds.t), std::max(max.x, bounds.r), std::min(min.y, bounds.b)};
		const float xd1 = max.x - min.x, yd1 = max.y - min.y, xd2 = bounds.r - bounds.l, yd2 = bounds.t - bounds.b;
		return xd1 + xd2 >= aabb_union.r - aabb_union.l && yd1 + yd2 >= aabb_union.t - aabb_union.b;*/

		// TODO:: hit pos might be off by one frame
		auto end = End();
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
	const float x, y, rot, vel, life, vx, vy;
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
private:
	static const size_t count = 400;
	static constexpr float slowdown = .02f, g = -.001f, init_mul = 1.f, min_fade = 750.f, max_fade = 2500.f,
		v_min = .05f, v_max = .75f, blink_rate = 16.f;
	const float vec_ratio = .75f; // static constexpr makes emscripten complain about unresolved symbol...
	bool kill = false, first = true;
public:
	const glm::vec3 pos, vec;
	const float time;
	struct Particle {
		glm::vec3 pos, v, decay;
		glm::vec4 col;
		float life, fade_duration;
		size_t start_col_idx;
	};
	std::array<Particle, count> arr;
	Particles(const glm::vec3& pos, const glm::vec3& vec, double time) : pos(pos), vec(vec), time((float)time) {}
	//static constexpr AABB GetMissileMaxBounds() {
	//	
	//	AABB res;
	//AABB max_bounds;
	// t = max_fade
	// a = decay
	// v0 = v
	//x = v0 * t + .5f * a  * t * t;
	//	return res;
	//}
	glm::vec3 GetPos(const Particle& p, float dt) const {
		return p.v * dt + p.decay * .5f * dt * dt;
	}
	bool Kill(double elapsed) const {
		return elapsed - time > max_fade || kill;
	}
	void Update(const Time& t) {
		if (first) {
			first = false;
			static std::uniform_real_distribution<> col_dist(0., 1.);
			static std::uniform_real_distribution<> rad_dist(.0, glm::two_pi<float>());
			static std::uniform_real_distribution<float> fade_dist(min_fade, max_fade);
			static std::uniform_real_distribution<> v_dist(v_min, v_max);
			static std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);

			for (size_t i = 0; i < count; ++i) {
				arr[i].col = { col_dist(mt), col_dist(mt), col_dist(mt), 1.f };
				arr[i].pos = {};
				auto r = rad_dist(mt);

				arr[i].v = { std::cos(r) * v_dist(mt) * init_mul, std::sin(r) * v_dist(mt) * init_mul, 0.f };
				arr[i].life = 1.f;
				arr[i].fade_duration = fade_dist(mt);
				arr[i].decay = -arr[i].v / arr[i].fade_duration;
				arr[i].decay.y += g;
				arr[i].v += vec;
				arr[i].start_col_idx = col_idx_dist(mt);
			}
		}
		kill = true;
		// TODO:: do it in vertex shader?
		float dt = (float)(t.total - time);
		for (size_t i = 0; i < count; ++i) {
			if (arr[i].life <= 0.f) continue;
			kill = false;
			arr[i].pos = GetPos(arr[i], dt);
			arr[i].col = globals.palette[(arr[i].start_col_idx + size_t(dt / blink_rate)) % globals.palette.size()];
			arr[i].col.a = arr[i].life;
			auto fade_time = dt / arr[i].fade_duration;
			arr[i].life = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
			if (arr[i].life <= 0.01f) arr[i].life = 0.f;
		}
	}
};
GLuint Particles::vbo;
GLsizei Particles::vertex_count;

struct Debris {
	const float max_centrifugal_speed = .02f, // rad/ms
		max_speed = .3f, //px/ ms
		min_speed = .05f, //px/ ms
		min_scale = 1.f, // 1/ms
		max_scale = 1.035f, // 1/ms
		missile_vec_ratio = .35f;
	const size_t min_scale_occurence = 3u, max_scale_occurence = 6u;
	const Asset::Model& ref;
	Asset::Model model;
	std::vector<float> centrifugal_speed, speed, scale;
	const glm::mat4 m;
	const glm::vec3 pos, vec;
	const float start;
	bool first = true;
	float fade_out;
	Debris(const Asset::Model& model, const glm::mat4& m, glm::vec3& pos, const glm::vec3& vec, float start) :
		ref(model),
		centrifugal_speed(model.vertices.size() / 6),
		speed(model.vertices.size() / 6),
		scale(model.vertices.size() / 6),
		m(m),
		pos(pos),
		vec(glm::vec3(glm::inverse(m) * glm::vec4(vec, 0.f))),
		//vec(vec),
		start(start) {}
	bool Kill(double elapsed) const {
		// don't change to fade_out, it might be not updated
		return elapsed - start > globals.player_fade_out_time;
	}
	void Setup() {
		if (!first) return;
		first = false;
		model = ref;
		static std::uniform_real_distribution<float> dist_cfs(-max_centrifugal_speed, max_centrifugal_speed),
			dist_sp(min_speed, max_speed),
			dist_scale(min_scale, max_scale);
		static std::uniform_int_distribution<size_t> dist_scale_occurence(std::min(min_scale_occurence, scale.size() - 1),
			std::min(max_scale_occurence, scale.size() - 1)),
			dist_scale_count(0, scale.size() - 1);
		for (auto& cfs : centrifugal_speed) cfs = dist_cfs(mt);
		for (auto& sp : speed) sp = dist_sp(mt);
		std::fill(std::begin(scale), std::end(scale), 1.f);
		for (size_t i = 0, count = dist_scale_occurence(mt); i < count; ++i) scale[dist_scale_count(mt)] = dist_scale(mt);
	}
	void Update(const Time& t) {
		Setup();
		float dt = t.total - start;
		fade_out = 1.f - dt / globals.player_fade_out_time;
		// TODO:: refactor to continuous fx instead of incremental
		for (size_t i = 0, cfs = 0; i < model.vertices.size(); i += 6, ++cfs) {
			// TODO:: only enough to have the average of the furthest vertices
			auto center = (model.vertices[i] + model.vertices[i + 1] + model.vertices[i + 2] +
				model.vertices[i + 3] + model.vertices[i + 4] + model.vertices[i + 5]) / 6.f;
			auto v = center;// -pos;
			float len = glm::length(v);
			v /= len;
			v *= speed[cfs] * (float)t.frame;
			v += vec /** missile_vec_ratio*/ * (float)t.frame;
			//v.y += g * (float)t.frame;
			auto incr = centrifugal_speed[cfs] * (float)t.frame;
			//				auto r = RotateZ(debris.vertices[i], center, incr);
			model.vertices[i] += v;
			model.vertices[i] = center + RotateZ(model.vertices[i], center, incr) * scale[cfs];
			model.vertices[i + 1] += v;
			model.vertices[i + 1] = center + RotateZ(model.vertices[i + 1], center, incr) * scale[cfs];
			model.vertices[i + 2] += v;
			model.vertices[i + 2] = center + RotateZ(model.vertices[i + 2], center, incr) * scale[cfs];
			model.vertices[i + 3] += v;
			model.vertices[i + 3] = center + RotateZ(model.vertices[i + 3], center, incr) * scale[cfs];
			model.vertices[i + 4] += v;
			model.vertices[i + 4] = center + RotateZ(model.vertices[i + 4], center, incr) * scale[cfs];
			model.vertices[i + 5] += v;
			model.vertices[i + 5] = center + RotateZ(model.vertices[i + 5], center, incr) * scale[cfs];
		}
	}
};
struct ProtoX {
	const size_t id;
	AABB aabb;
	Client *ws;
	const float max_vel = .3f,
		m = 500.f,
		force = .2f,
		slowdown = .0003f,
		ground_level = 20.f,
		rot_max_speed = .003f, //rad/ms
		rot_inc = .000005f, // rad/ms*ms
		safe_rot = glm::radians(35.f);
	// state...
	float invincible, fade_out, visible = 1.f, blink = 1.f;
	double hit_time;
	bool hit, killed;
	glm::vec3 vel, f/*, hit_pos*/;
	int score = 0;
	size_t missile_id = 0;
	enum class Ctrl { Full, Prop, Turret };
	Ctrl ctrl = Ctrl::Full;
	bool pos_invalidated = false, // from web-message
		first = true;
	float clear_color_blink;
	float rot_speed = 0.f;
	std::shared_ptr<Envelope> e_invinciblity, e_blink;
	Audio::Source die, start;
	// TODO:: hack for missle owner cleanup in ProtoX dtor
	std::vector<Missile>& missiles;
	size_t missile_count = globals.max_missile;
	std::queue<Command> commands;
	float msg_interval = 0.f;
	struct Body {
		glm::vec3 pos;
		float rot;
		const Asset::Layer& layer;
		Val<AABB> aabb;
		Val<OBB> bbox;
		Body(const glm::vec3& pos, float rot, const Asset::Layer& layer) : pos(pos), rot(rot), layer(layer),
			aabb(TransformAABB(layer.aabb, GetModel())),
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
		size_t frame = 0;
		Audio::Source source;
		float pan, gain;
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
			if (on)	
				globals.audio->Enqueue(Audio::Command::ID::Start, source, pan, gain, true);
			else
				globals.audio->Enqueue(Audio::Command::ID::Stop, source);
		}
		Propulsion(const glm::vec3& pos, float rot, float scale, size_t frame_count, float pan, float gain) :
			pos(pos), rot(rot), scale(scale), frame_count(frame_count),
			source(globals.audio->GenSource(globals.audio->engine)),
			pan(pan), gain(gain) {}
		auto GetModel(const glm::mat4& m) const {
			return ::GetModel(m, pos, rot, /*layer.pivot*/{}, scale);
		}
		~Propulsion() {
			globals.audio->Enqueue(Audio::Command::ID::Stop, source);
		}
	}left, bottom, right;
	struct Turret {
		const float rot = 0.f;
		const float rest_rot = glm::half_pi<float>(),
			min_rot = -glm::radians(45.f) - rest_rot,
			max_rot = glm::radians(225.f) - rest_rot;
		const Asset::Layer& layer;
		const glm::vec3 missile_start_offset;
		Val<glm::vec4> missile_start_pos;
		Val<AABB> aabb;
		Val<OBB> bbox;
		Turret(const Asset::Layer& layer, const glm::mat4& m) : layer(layer),
			missile_start_offset(layer.pivot + glm::vec3(0.f, globals.missile_start_offset_y, 0.)),
			missile_start_pos(glm::vec4{ 0.f, 0.f, 0.f, 1.f }),
			aabb(TransformAABB(layer.aabb, GetModel(m))),
			bbox(TransformBBox(layer.aabb, GetModel(m))){}
		void SetRot(const float rot) {
			prev_rot = this->rot;
			const_cast<float&>(this->rot) = std::min(max_rot, std::max(min_rot, rot));
		}
		void Update(const Time& t, const glm::mat4& parent) {
			const auto m = GetModel(parent);
			aabb = TransformAABB(layer.aabb, m);
			bbox = TransformBBox(layer.aabb, m);
			missile_start_pos = m * glm::vec4{ missile_start_offset, 1.f };
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
	ProtoX(const size_t id, const Asset::Model& model, size_t frame_count, const glm::vec3& pos, std::vector<Missile>& missiles, Client* ws = nullptr) : id(id),
		aabb(model.aabb),
		ws(ws),
		left({ 88.f, 3.f, 0.f }, glm::radians(55.f), 1.f, frame_count, 1.f, .3f),
		right({ -88.f, 3.f, 0.f }, -glm::radians(55.f), 1.f, frame_count, -1.f, .3f),
		bottom({ 0.f, -20.f, 0.f }, 0.f, 2.f, frame_count, 0.f, 1.f),
		body(pos, 0.f, model.layers.front()),
		turret(model.layers.back(), body.GetModel()),
		clear_color_blink(globals.clear_color_blink_rate),
		die(globals.audio->GenSource(globals.audio->die)),
		start(globals.audio->GenSource(globals.audio->start)),
		missiles(missiles) {
		Init();
	}
	~ProtoX() {
		// TODO:: handle missiles after owner destruction
		auto it = std::partition(std::begin(missiles), std::end(missiles), [this](const Missile& m) {
			return m.owner != this; });
		missiles.erase(it, std::end(missiles));
	}
	/*auto GetPrevModel() const {
		return ::GetModel({}, prev_pos, prev_rot, layer.pivot);
	}*/
	void Kill(size_t id, size_t missile_id, int score, const glm::vec3& hit_pos, const glm::vec3& missile_vec) {
		if (!ws) return;
		Scor msg{ Tag("SCOR"), this->id, id, missile_id, score, hit_pos.x, hit_pos.y, missile_vec.x, missile_vec.y };
		ws->Send((const char*)&msg, sizeof(msg));
	}
	bool Cull(const glm::mat4& vp) {
		glm::vec4 tl(aabb.l, aabb.t, 0.f, 1.f);
		tl = vp * tl;
		if (tl.x > 1.f || tl.y < -1.f) return true;

		glm::vec4 br(aabb.r, aabb.b, 0.f, 1.f);
		br = vp * br;
		if (br.x < -1.f || br.y > 1.f) return true;
		return false;
	}
	static std::vector<glm::vec3> obb1, obb2;
	bool CollisionTest(const ProtoX& other) const {
		const AABB intersection = Intersect(aabb, other.aabb);
		// TODO:: fix intersection test
		//if (intersection.r < intersection.l || intersection.t < intersection.b) return false;

		const auto player_body_obb = ::GetConvexHullOfOBBSweep(body.bbox.val, body.bbox.prev),
			other_body_obb = ::GetConvexHullOfOBBSweep(other.body.bbox.val, other.body.bbox.prev);
		auto res = ::HitTest(player_body_obb, other_body_obb);
		if (res) {
			obb1 = player_body_obb; obb2 = other_body_obb;
			return true;
		}
		const auto player_turret_obb = ::GetConvexHullOfOBBSweep(turret.bbox.val, turret.bbox.prev),
			other_turret_obb = ::GetConvexHullOfOBBSweep(other.turret.bbox.val, other.turret.bbox.prev);
		res = ::HitTest(player_turret_obb, other_turret_obb);
		if (res) {
			obb1 = player_turret_obb; obb2 = other_turret_obb;
			return true;
		}
		res = ::HitTest(player_body_obb, other_turret_obb);
		if (res) {
			obb1 = player_body_obb; obb2 = other_turret_obb;
			return true;
		}
		res = ::HitTest(player_turret_obb, other_body_obb);
		if (res) {
			obb1 = player_turret_obb; obb2 = other_body_obb;
			return true;
		}
		return false;
	}

	void Shoot(std::vector<Missile>& missiles, double frame_time) {
		if (!missile_count) return;
		missile_count--;
		if (ctrl != Ctrl::Turret && ctrl != Ctrl::Full) return;
		if (hit || killed) return;
		const float missile_vel = 1.f;
		auto rot = turret.rest_rot + turret.rot + body.rot;
		glm::vec3 missile_vec{ std::cos(rot) * missile_vel, std::sin(rot) * missile_vel,.0f};
		auto vec = (turret.missile_start_pos.operator const glm::vec4 &() - turret.missile_start_pos.prev)/* / (float)frame_time*/;
		missiles.emplace_back(glm::vec3(turret.missile_start_pos.operator const glm::vec4 &())/*body.pos + start_pos*/, rot, glm::length(missile_vec), this, ++missile_id, glm::vec3(vec));
		if (ws) {
			auto& m = missiles.back();
			Misl misl{ Tag("MISL"), id, m.id, m.pos.x, m.pos.y, m.rot, m.vel, m.life, m.vec.x, m.vec.y};
			globals.ws->Send((char*)&misl, sizeof(misl));
		}
	}
	void TurretControl(double x, double px) {
		if (ctrl != Ctrl::Turret && ctrl != Ctrl::Full) return;
		const double rot_ratio = (turret.max_rot - turret.min_rot) / globals.width;
		turret.SetRot(float((globals.width >> 1) - x) * rot_ratio);
		//turret.rot += float((px - x) * rot_ratio);
	}
	void Move(double frame_time, bool lt, bool rt, bool bt) {
		if (hit) return;
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
			rot_speed = std::min(rot_speed + sign * rot_inc * (float)frame_time, limit);
		else if (sign<0.f)
			rot_speed = std::max(rot_speed + sign * rot_inc * (float)frame_time, limit);

		glm::vec3 val;
		if (lt) val += force_l;
		if (rt) val += force_r;
		if (bt) val += force_b;
		f = glm::rotateZ(val, body.rot);
	}
	void SetInvincibility() {
		invincible = globals.invincibility;
		e_invinciblity = std::shared_ptr<Envelope>(
			new Blink(visible, invincible, globals.timer.TotalMs(), globals.invincibility_blink_rate, 0.f));
		globals.envelopes.push_back(e_invinciblity);
	}
	void SetCtrl(Ctrl ctrl) {
		SetInvincibility();
		this->ctrl = ctrl;
		pos_invalidated = false;
	}
	void Init() {
		SetInvincibility();
		fade_out = globals.player_fade_out_time;
		e_blink = std::shared_ptr<Envelope>(
			new Blink(blink, 0., globals.timer.TotalMs(), globals.blink_rate, globals.blink_ratio));
		globals.envelopes.push_back(e_blink);
		hit = killed = false;
	}
	bool SkipDeathCheck() const { return invincible > 0.f || hit || killed; }
	void Die(const glm::vec3& hit_pos, const glm::mat4& vp, std::list<Debris>& debris, std::list<Particles>& particles, const Asset::Model& debris_model, glm::vec3 vec, double hit_time, const Camera& cam) {
		if (SkipDeathCheck()) return;
		hit = true;
		this->hit_time = hit_time;
	//	this->hit_pos = RotateZ(hit_pos, body.pos, -body.rot);
		auto l = glm::length(vec);
		if (l > 0.f) vec /= l;
		const float from_center_ratio = .5f, vec_ratio = .5f;
		debris.emplace_back(debris_model, body.GetModel(), body.pos, glm::normalize(::Center(aabb) - hit_pos) * from_center_ratio + vec * vec_ratio, hit_time);
		/*obb1.clear();
		obb1.push_back(body.pos);
		obb1.push_back(body.pos + glm::vec3(glm::inverse(body.GetModel())* glm::vec4(missile_vec, 0.f)) * 100.f);*/
		particles.emplace_back( hit_pos, vec, hit_time );
		left.Set(false); right.Set(false); bottom.Set(false);
		const auto& ndc = cam.NDC(body.pos);
		globals.audio->Enqueue(Audio::Command::ID::Start, die, glm::clamp(ndc.x, -1.f, 1.f), ::NDCToGain(ndc.x));
	}
	bool IsInRestingPos(const AABB& bounds) const {
		return body.pos.y + body.layer.aabb.b <= bounds.b + ground_level;
	}

	//auto GenBBoxEdgesCCW() {
	//	//// TODO:: bbox not aabb
	//	//std::vector<glm::vec3> res;
	//	//AABBToBBoxEdgesCCW(body.aabb, res);
	//	//AABBToBBoxEdgesCCW(turret.aabb, res);
	//	//AABBToBBoxEdgesCCW(body.aabb.prev, res);
	//	//AABBToBBoxEdgesCCW(turret.aabb.prev, res);
	//	std::vector<glm::vec3> res;
	//	
	//	res.push_back(body.bbox.operator const OBB &()[0]);
	//	res.push_back(body.bbox.operator const OBB &()[1]);
	//	res.push_back(body.bbox.operator const OBB &()[1]);
	//	res.push_back(body.bbox.operator const OBB &()[2]);
	//	res.push_back(body.bbox.operator const OBB &()[2]);
	//	res.push_back(body.bbox.operator const OBB &()[3]);
	//	res.push_back(body.bbox.operator const OBB &()[3]);
	//	res.push_back(body.bbox.operator const OBB &()[0]);
	//	
	//	res.push_back(body.bbox.prev[0]);
	//	res.push_back(body.bbox.prev[1]);
	//	res.push_back(body.bbox.prev[1]);
	//	res.push_back(body.bbox.prev[2]);
	//	res.push_back(body.bbox.prev[2]);
	//	res.push_back(body.bbox.prev[3]);
	//	res.push_back(body.bbox.prev[3]);
	//	res.push_back(body.bbox.prev[0]);

	//	res.push_back(turret.bbox.operator const OBB &()[0]);
	//	res.push_back(turret.bbox.operator const OBB &()[1]);
	//	res.push_back(turret.bbox.operator const OBB &()[1]);
	//	res.push_back(turret.bbox.operator const OBB &()[2]);
	//	res.push_back(turret.bbox.operator const OBB &()[2]);
	//	res.push_back(turret.bbox.operator const OBB &()[3]);
	//	res.push_back(turret.bbox.operator const OBB &()[3]);
	//	res.push_back(turret.bbox.operator const OBB &()[0]);

	//	res.push_back(turret.bbox.prev[0]);
	//	res.push_back(turret.bbox.prev[1]);
	//	res.push_back(turret.bbox.prev[1]);
	//	res.push_back(turret.bbox.prev[2]);
	//	res.push_back(turret.bbox.prev[2]);
	//	res.push_back(turret.bbox.prev[3]);
	//	res.push_back(turret.bbox.prev[3]);
	//	res.push_back(turret.bbox.prev[0]);
	//	return res;
	//}

	void Update(const Time& t, const AABB& bounds, std::list<Particles>& particles, std::list<Debris>& debris, bool player_self, const Camera& cam, const Asset::Model& debris_model) {
		Execute(t.total, t.frame, commands);
		msg.clear();
		if (invincible > 0.f) {
			invincible -= (float)t.frame;
			if (invincible <= 0.f) {
				invincible = 0.f;
			}
		}
		else if (hit) {
			auto fade_time = (float)(t.total - hit_time) / globals.player_fade_out_time;
			fade_out = 1.f - fade_time * fade_time * fade_time * fade_time * fade_time;
			if ((killed = (fade_out <= 0.0001f))) return;
			left.on = right.on = bottom.on = false;
			if (clear_color_blink < 0.f)
				clear_color_blink += globals.clear_color_blink_rate;
			else
				clear_color_blink -= (float)t.frame;
			return;
		}
		// TODO:: after cam update
		const auto& ndc = cam.NDC(body.pos);
		auto pan = glm::clamp(ndc.x, -1.f, 1.f), gain = ::NDCToGain(ndc.x);
		if (first) {
			globals.audio->Enqueue(Audio::Command::ID::Start, start, pan, gain);
			first = false;
		} else
			globals.audio->Enqueue(Audio::Command::ID::Ctrl, start, pan, gain);

		if (!player_self) {
			// TODO:: after cam update
			left.pan = right.pan = bottom.pan = pan;
			left.gain = right.gain = bottom.gain = gain;
		}
		left.Update(t);
		right.Update(t);
		bottom.Update(t);
		if (player_self) {
			if (ctrl == Ctrl::Full || ctrl == Ctrl::Prop) {
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
			}	else if (!pos_invalidated) {
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
				body.pos.y = bounds.b + ground_level - body.layer.aabb.b;
				if (std::abs(body.rot) > safe_rot && invincible <= 0.f) {
					glm::vec3 hit_pos = body.pos + glm::vec3{ 0.f, body.layer.aabb.b, 0.f },
						n{ 0.f, 1.f, 0.f };
					const auto vec = glm::reflect(vel, n);
					Die(hit_pos, cam.vp, debris, particles, debris_model, vec, t.total, cam);
					--score;
					Kill(id, id, score, hit_pos, vec);
				}
				else {
					body.rot = 0.f;
					if (std::abs(vel.y) < 0.001f)
						vel.y = 0.f;
					else
						vel = { 0.f, -vel.y / 2.f, 0.f };
				}
			}
			else if (turret.layer.aabb.t + body.pos.y >= bounds.t) {
				body.pos.y = bounds.t - turret.layer.aabb.t;
				vel.y = 0.f;
			}
			if (!IsInRestingPos(bounds)) {
				// wrap around -PI..PI
				float pi = (body.rot < 0.f) ? -glm::pi<float>() : glm::pi<float>();
				body.rot = std::fmod(body.rot + rot_speed * (float)t.frame + pi, pi * 2.f) - pi;
				//LOG_INFO("%g %g %g %g\n", f.x, f.y, f.z, body.rot);
				//LOG_INFO("%g\n",body.rot);
			}
			msg_interval += t.frame;
			if (ws && (globals.msg_interval == 0.f || msg_interval>=globals.msg_interval)) {
				msg_interval -= globals.msg_interval;
				Plyr player{ Tag("PLYR"), id, body.pos.x, body.pos.y, turret.rot, invincible, vel.x, vel.y, body.rot, left.on, right.on, bottom.on };
				globals.ws->Send((char*)&player, sizeof(Plyr));
			}
		}
		body.Update(t);
		turret.Update(t, body.GetModel());
		aabb = Union(Union(body.aabb, turret.aabb),
			Union(body.aabb.prev, turret.aabb.prev));
	}
	void ResetVals() {
		// TODO:: reset preview values
		body.bbox = body.bbox.val;
		turret.bbox = turret.bbox.val;
		body.aabb = body.aabb.val;
		turret.aabb = turret.aabb.val;
		aabb = Union(body.aabb, turret.aabb);
	}
	float WrapAround(float min, float max) {
		float dif = body.pos.x - max;
		if (dif >= 0) {
			ResetVals();
			body.pos.x = min + dif;
			return dif;
		}
		dif = body.pos.x - min;
		if (dif < 0) {
			ResetVals();
			body.pos.x = max + dif;
			return dif;
		}
		return 0.f;
	}
};
std::vector<glm::vec3> ProtoX::obb1, ProtoX::obb2;
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
		VBO_FOREGROUND = 14,
		VBO_COUNT = 15;
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
			std::shared_ptr<Envelope> e_sequence;
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

		// Foreground
		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_FOREGROUND]);
			glBufferData(GL_ARRAY_BUFFER, assets.foreground.vertices.size() * sizeof(assets.foreground.vertices[0]), &assets.foreground.vertices.front(), GL_STATIC_DRAW);
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
			starField = { count_per_layer, scene_bounds, vbo[VBO_STARFIELD], count_per_layer * layer_count, {-.75f, 0}, {-.5f, 0}, {-.25f, 0 } };
			starField.layer1.e_sequence = std::shared_ptr<Envelope>(new SequenceAsc(starField.layer1.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer1_blink_rate, starField.layer1.color_idx, globals.grey_palette.size(), 1, 1));
			globals.envelopes.push_back(starField.layer1.e_sequence);
			starField.layer2.e_sequence = std::unique_ptr<Envelope>(new SequenceAsc(starField.layer2.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer2_blink_rate, starField.layer2.color_idx, globals.grey_palette.size(), 1, 1));
			globals.envelopes.push_back(starField.layer2.e_sequence);
			starField.layer3.e_sequence = std::unique_ptr<Envelope>(new SequenceAsc(starField.layer3.color_idx, 0., globals.timer.ElapsedMs(),
				globals.starfield_layer3_blink_rate, starField.layer3.color_idx, globals.grey_palette.size(), 1, 1));
			globals.envelopes.push_back(starField.layer3.e_sequence);
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
		//glLineWidth(LINE_WIDTH * 2.f);
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
			if (Cull(p.pos, cam.vp)) continue;
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
	template<GLenum mode>
	void DrawLines(const Camera& cam, const std::vector<glm::vec3>& edges, const glm::vec4& col = {1.f, 1.f, 1.f, 1.f}) {
		if (edges.empty())
			return;
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
		glDrawArrays(mode, 0, edges.size());
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
	void Draw(const Camera& cam, const std::list<Debris>& debris) {
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_DEBRIS]);
		glVertexAttribPointer(0,
			3,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		for (const auto& d : debris) {
			if (Cull(d.pos, cam.vp)) continue;
			glBufferData(GL_ARRAY_BUFFER, d.model.vertices.size() * sizeof(d.model.vertices[0]), &d.model.vertices.front(), GL_DYNAMIC_DRAW);
			const auto& shader = colorShader;
			glUseProgram(shader.id);
			auto& p = assets.debris.layers.front().parts.front();
			glm::mat4 mvp = cam.vp *d.m; // * glm::translate(d.pos);//
			glUniformMatrix4fv(shader.uMVP, 1, GL_FALSE, &mvp[0][0]);
			glUniform4f(shader.uCol, p.col.r, p.col.g, p.col.b, d.fade_out);
			glDrawArrays(GL_TRIANGLES, p.first, p.count);
		}
		glDisableVertexAttribArray(0);
		glDisable(GL_BLEND);
	}
	void Draw(const Camera& cam, const ProtoX& proto, bool player = false) {
		if (!proto.visible)
			return;
		const auto m = proto.body.GetModel();
		if (proto.hit) return;
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
		//	m = glm::translate(proto.pos) * glm::scale({}, proto.scale) * proto.rthruster_model;
		//	mvp *= m;
		//	glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		//	auto frame = (proto.state.rt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
		//		: model.parts[(size_t)Asset::Parts::Thruster2];
		//	glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		//}
		//if (proto.state.bthruster) {
		//	mvp = cam.vp;
		//	m = glm::translate( proto.pos) * glm::scale({}, proto.scale) * proto.bthruster1_model;
		//	mvp *= m;
		//	glUniformMatrix4fv(colorShader.uMVP, 1, GL_FALSE, &mvp[0][0]);
		//	auto frame = (proto.state.bt_frame % thruster_frame_count) ? model.parts[(size_t)Asset::Parts::Thruster1]
		//		: model.parts[(size_t)Asset::Parts::Thruster2];
		//	glDrawArrays(GL_LINE_STRIP, frame.first, frame.count);
		//	mvp = cam.vp;
		//	m = glm::translate(proto.pos) * glm::scale({}, proto.scale) * proto.bthruster2_model;
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
		if (!player)
			return;
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
		Draw(cam, vbo[VBO_LANDSCAPE], {} , assets.land);
	}
	void DrawForeground(const Camera& cam) {
		Draw(cam, vbo[VBO_FOREGROUND], { cam.pos.x * globals.foreground_pos_x_ratio, 0.f, 0.f }, assets.foreground);
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
			if (missile.Cull(cam.vp)) continue;
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
	static int count;
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		count = action;
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
int InputHandler::count = 0;
#ifdef __EMSCRIPTEN__
std::queue<InputHandler::TouchEvent> InputHandler::touch_event_queue;
#endif

class Scene {
public:
	Asset::Assets assets;
	AABB bounds;
	Shader::Simple simple;
	Renderer renderer;
	std::vector<Missile> missiles;
	std::unique_ptr<ProtoX> player;
	std::map<size_t, std::unique_ptr<ProtoX>> players;
	std::list<Particles> particles;
	std::list<Debris> debris;
	std::vector<Text> texts;
	Camera camera{ (int)globals.width, (int)globals.height }, hud{ (int)globals.width, (int)globals.height },
		overlay;
	InputHandler inputHandler;
	//std::mutex msgMutex;
	//GLuint VertexArrayID;
	//GLuint vertexbuffer;
	//GLuint uvbuffer;
	//GLuint texID;
	//GLuint uTexSize;
	std::queue<std::vector<unsigned char>> messages;
	int wait = 0;

	float update = 0.f;
	size_t _sent = 0.f, _rec = 0.f;
	Val<size_t> sent, received;

//#ifndef __EMSCRIPTEN__
//	void* operator new(size_t i)
//	{
//		return _mm_malloc(i,16);
//	}
//	void operator delete(void* p)
//	{
//		_mm_free(p);
//	}
//#endif

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
		//return{};
	}
	void GenerateNPC() {
		static size_t i = 0;
		auto p = std::make_unique<ProtoX>((size_t)0xbeef, assets.probe, assets.propulsion.layers.size(), RandomizePos(assets.probe.aabb), missiles);
		std::uniform_real_distribution<> rot_dist(-glm::pi<float>(), glm::pi<float>());
		p->body.rot = rot_dist(mt);
		p->turret.SetRot(rot_dist(mt));
		players[++i] = std::move(p);
	}
	void GenerateNPCs() {
		for (size_t i = 0; i<MAX_NPC; ++i) {
			GenerateNPC();
		}
	}
	std::shared_ptr<Envelope> e_wsad_blink, e_mouse_blink;
	void SetCtrl(ProtoX::Ctrl ctrl) {
		if (!player) return;
		player->SetCtrl(ctrl);
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Prop) {
			e_wsad_blink = std::shared_ptr<Envelope>(new Blink(renderer.wsad_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio));
			// TODO:: replace/remove
			globals.envelopes.push_back(e_wsad_blink);
		}
		if (ctrl == ProtoX::Ctrl::Full || ctrl == ProtoX::Ctrl::Turret) {
			e_mouse_blink = std::unique_ptr<Envelope>(new Blink(renderer.mouse_decal.factor, globals.blink_duration, globals.timer.ElapsedMs(), globals.blink_rate, globals.blink_ratio));
			// TODO:: replace/remove
			globals.envelopes.push_back(e_mouse_blink);
		}
	}
#ifdef OBB_TEST
	auto AddNPC(size_t id, const glm::vec3& pos, float rot) {
		auto p = std::make_unique<ProtoX>(id, assets.probe, assets.propulsion.layers.size(), pos, missiles);
		std::uniform_real_distribution<> rot_dist(0., glm::degrees(glm::two_pi<float>()));
		p->body.rot = rot;
		//p->right.on = true;
		p->bottom.on = true;
		p->turret.SetRot(rot_dist(mt));
		auto res = p.get();
		players[id] = std::move(p);
		return res;
	}
	void AddOBBTestEntities(double total) {
		auto p1 = AddNPC(0xdead, { -340.f, 0.f, 0.f }, 0.f);
		p1->commands.push({ total, total + 1248.4, [=](double frame) {p1->Move(frame, 0, 0, 0); } });
		p1->commands.push({ total + 1248.4, total + 2247.2, [=](double frame) {p1->Move(frame, 0, 0, 1); } });
		p1->commands.push({ total + 2247.2, total + 2964.07, [=](double frame) {p1->Move(frame, 0, 1, 1); } });
		p1->commands.push({ total + 2964.07, total + 3514.16, [=](double frame) {p1->Move(frame, 0, 0, 1); } });
		p1->commands.push({ total + 3514.16, total + 3730.95, [=](double frame) {p1->Move(frame, 0, 0, 0); } });

		//{
		//	auto p1 = AddNPC(0xbeef, { 340.f, -150.f, 0.f }, 0.f);
		//	//p1->commands.push({ total + 700., total + 700.48, [=](double frame) {p1->Move(frame, 0, 0, 0); } });
		//	p1->commands.push({ total, total + 1373.34, [=](double frame) {p1->Move(frame, 0, 0, 1); } });
		//	p1->commands.push({ total + 1373.34, total + 2755.94, [=](double frame) {p1->Move(frame, 1, 0, 1); } });
		//	p1->commands.push({ total + 2755.94, total + 3539.9, [=](double frame) {p1->Move(frame, 0, 0, 1); } });
		//	p1->commands.push({ total + 3539.9, total + 3789.73, [=](double frame) {p1->Move(frame, 0, 1, 1); } });
		//	p1->commands.push({ total + 3789.73, total + 4823.01, [=](double frame) {p1->Move(frame, 0, 0, 1); } });
		//	p1->commands.push({ total + 4823.01, total + 4889.68, [=](double frame) {p1->Move(frame, 0, 0, 0); } });

		//}

	}
#endif
	Scene() : bounds(assets.land.aabb),
//#ifndef __EMSCRIPTEN__
#ifdef DEBUG_REL
//#ifndef OBB_TEST
	player(std::make_unique<ProtoX>(0xdeadbeef, assets.probe, assets.propulsion.layers.size(), glm::vec3{}, missiles)),
//#endif
#endif
//#endif
		renderer(assets, bounds),
		overlay(camera),
		sent(_sent), received(_rec) {
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
	}
	void Render() {
		renderer.PreRender();
		auto res = renderer.rt.GetCurrentRes();
		glViewport(0, HUD_RATIO(res.y), res.x, VP_RATIO(res.y));
		renderer.DrawBackground(camera);
		renderer.DrawLandscape(camera);
		
		for (const auto& p : players) {
			if (p.second->Cull(camera.vp)) continue;
			renderer.Draw(camera, *p.second.get());
			//renderer.Draw(camera, p.second->aabb);
			//renderer.DrawLines<GL_LINE_STRIP>(camera, GetConvexHullOfOBBSweep(p.second->body.bbox, p.second->body.bbox.prev), { .3f, 1.f, .3f, 1.f });
			//renderer.DrawLines<GL_LINE_STRIP>(camera, GetConvexHullOfOBBSweep(p.second->turret.bbox, p.second->turret.bbox.prev), { 1.f, .3f, .3f, 1.f });

		//	renderer.Draw(camera, p.second->aabb.Translate(p.second->pos));
		}
		renderer.Draw(camera, missiles);
		if (player) {
			renderer.Draw(camera, *player.get(), true);
			//renderer.Draw(camera, player->aabb);
			//renderer.DrawLines<GL_LINE_STRIP>(camera, GetConvexHullOfOBBSweep(player->body.bbox, player->body.bbox.prev), { .3f, 1.f, .3f, 1.f });
			//renderer.DrawLines<GL_LINE_STRIP>(camera, GetConvexHullOfOBBSweep(player->turret.bbox, player->turret.bbox.prev), { 1.f, .3f, .3f, 1.f });
			//renderer.DrawLines<GL_LINE_STRIP>(camera, ProtoX::obb1, { .3f, 1.f, .3f, 1.f });
			//renderer.DrawLines<GL_LINE_STRIP>(camera, ProtoX::obb2, { 1.f, .3f, .3f, 1.f });
		}
		renderer.Draw(camera, particles);
		renderer.Draw(camera, debris);
		renderer.DrawForeground(camera);
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
			++m.owner->missile_count;
			m = missiles.back();
			missiles.pop_back();
			return false;
		}
		++missiles.back().owner->missile_count;
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
	void HitTest(ProtoX& p1, ProtoX& p2, double total) {
		if (!p1.SkipDeathCheck() && !p2.SkipDeathCheck() && p1.CollisionTest(p2)) {
			glm::vec3 hit_pos((p1.body.pos.x + p2.body.pos.x) / 2.f, (p1.body.pos.y + p2.body.pos.y) / 2.f, 0.f);
			p1.Die(hit_pos, camera.vp, debris, particles, assets.debris, p1.vel, total, camera);
			p2.Die(hit_pos, camera.vp, debris, particles, assets.debris, p2.vel, total, camera);
		}
	}
	void Update(const Time& t) {
		const double scroll_speed = .5; // px/s
		if (inputHandler.keys[(size_t)InputHandler::Keys::Left])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		if (inputHandler.keys[(size_t)InputHandler::Keys::Right])
			camera.Translate(float(scroll_speed * t.frame), 0.f, 0.f);
		texts.clear();
		if (globals.ws) {
			update += t.frame;
			if (update >= 1000.f) {
				update -= 1000.f;
				sent = globals.ws->sent;
				received = globals.ws->received;	
			}
			texts.push_back({ { 380.f, 300.f, 0.f }, 1.f, Text::Align::Right, std::to_string(sent.val - sent.prev) + ":" + std::to_string(received.val - received.prev) });
			//texts.push_back({ { 380.f, 300.f, 0.f }, 1.f, Text::Align::Right, std::to_string(globals.ws->sent) + ":" + std::to_string(globals.ws->received) });
		}
		if (wait > 0) {
			std::stringstream ss;
			ss << "WAITING FOR " << wait << " MORE PLAYER" << ((wait == 1) ? "" : "S");
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
					a = e.touchPoints[0].canvasX >(globals.width * 2 / 3),
					shoot = e.touchPoints[0].clientY < (globals.height / 2);
				//LOG_INFO("%d %d %d %d %d %d\n", e.touchPoints[0].clientX, e.touchPoints[0].clientY, globals.width, globals.width / 4, globals.width * 4 / 5, w);
				player->Move(t.frame, a, d, w);
				if (shoot)
					player->Shoot(missiles, t.frame);
				touch = true;
			}
#endif
			// for OBBTest
			//static bool lt = false, rt = false, bt = false, first = true;
			//static double total = t.total;
			//if (first || inputHandler.keys[(size_t)InputHandler::Keys::A] != lt ||
			//inputHandler.keys[(size_t)InputHandler::Keys::D] != rt ||
			//	inputHandler.keys[(size_t)InputHandler::Keys::W] != bt) {
			//	lt = inputHandler.keys[(size_t)InputHandler::Keys::A];
			//	rt = inputHandler.keys[(size_t)InputHandler::Keys::D];
			//	bt = inputHandler.keys[(size_t)InputHandler::Keys::W];
			//	LOG_INFO("p1->commands.push({ total + %g, total + %g, [=](double frame) {p1->Move(frame, %d, %d, %d); } });\n", total, t.total, lt, rt, bt);
			//	first = false;
			//	total = t.total;
			//}
			if (!touch) {
				if (inputHandler.update) player->TurretControl(inputHandler.x, inputHandler.px);
				player->Move(t.frame, inputHandler.keys[(size_t)InputHandler::Keys::A],
					inputHandler.keys[(size_t)InputHandler::Keys::D],
					inputHandler.keys[(size_t)InputHandler::Keys::W]);
				while (!inputHandler.event_queue.empty()) {
					auto e = inputHandler.event_queue.back();
					inputHandler.event_queue.pop();
					switch (e) {
					case InputHandler::ButtonClick::LB:
						player->Shoot(missiles, t.frame);
						break;
					}
				}
			}
			player->Update(t, bounds, particles, debris, true, camera, assets.debris);
			//player->msg.push_back({ player->body.pos + glm::vec3{0.f, -100.f, 0.f}, 1.f, Text::Align::Left, std::to_string(InputHandler::count) });
			// TODO:: is this needed? auto d = camera.pos.x + player->body.pos.x;
			auto res = player->WrapAround(assets.land.layers[0].aabb.l, assets.land.layers[0].aabb.r);
			camera.Update(t, bounds, player->body.pos, player->vel);
		}
#ifdef OBB_TEST
		if (players.empty())
			AddOBBTestEntities(t.total);
		for (const auto& p : players) {
			//auto a = p.second->left.on,
			//	w = p.second->bottom.on,
			//	d = p.second->right.on;
			//p.second->Move(t, a, d, w);
			p.second->Update(t, bounds, particles, debris, true, camera, assets.debris);
		}
		auto it1 = players.find(0xdead), it2 = players.find(0xbeef);
		if (it1 != players.end() && it2 != players.end()) {
			auto& p1 = *it1->second, &p2 = *it2->second;
			HitTest(p1, p2, t.total);
		}
#else		
		for (auto& p : players) {
			p.second->Update(t, bounds, particles, debris, false, camera, assets.debris);
		}
#endif


		for (auto p = std::begin(players); p != std::end(players);) {
			if (p->second->killed) {
				players.erase(p++);
			}
			else ++p;
		}
		if (player) {
			if (player->hit && player->clear_color_blink < 0.f) {
				std::uniform_int_distribution<> col_idx_dist(0, globals.palette.size() - 1);
				renderer.clearColor = globals.palette[col_idx_dist(mt)];
			}
			if (player->killed) {
				auto ctrl = player->ctrl;
				auto score = player->score;
				player = std::make_unique<ProtoX>(player->id, assets.probe, assets.propulsion.layers.size(), RandomizePos(assets.probe.aabb), missiles, globals.ws.get());
				camera.SetPos(player->body.pos.x, player->body.pos.y, globals.camera_z);
				renderer.clearColor = { 0.f, 0.f, 0.f, 1.f };
				SetCtrl(ctrl);
				player->score = score;
			}
		}
		for (const auto& e : globals.envelopes) {
			if (auto spt = e.lock())
				spt->Update(t);
		}
		// TODO:: is resize more efficient
		globals.envelopes.erase(std::remove_if(
			std::begin(globals.envelopes), std::end(globals.envelopes), [](const auto& e) { 
			auto spt = e.lock();
			if (!spt) return true;
			return spt->Finished(); }), globals.envelopes.end());

		for (auto& m : missiles) {
			m.Update(t, camera, bounds);
		}
		auto it = missiles.begin();
		while (it != missiles.end()) {
			if (it->remove) {
				if (RemoveMissile(*it)) break;
			} else ++it;
		}
		if (player) {
			//std::stringstream ss;
			//ss << camera.pos.x << " "<<camera.pos.y;
			//
			//texts.push_back({ {}, 1.f, Text::Align::Left,  ss.str() });
			//ss.str("");
			//ss << camera.d.x << " " << camera.d.y;
			//texts.push_back({ {0.f, -40.f, 0.f}, 1.f, Text::Align::Left,  ss.str() });

			// wraparound camera hack
			// wrap around from left
			// TODO:: is this needed?
			//if (res < 0)
			//	camera.Translate(std::max(d, -float(globals.width >> 2)) - (globals.width >> 2), 0.f, 0.f);
			//else if (res > 0)
			//	camera.Translate(std::min(d, float(globals.width >> 2)) + (globals.width >> 2), 0.f, 0.f);
			
		
			for (auto& m : missiles){
				//invincible players can't kill
				if (m.owner->invincible > 0.f) {
					continue;
				}
				// TODO:: test all hits or just ours?
				if (m.owner != player.get()) {
					continue;
				}
				bool last = false;
				for (const auto& p : players) {
					// if (m.owner != p.second.get()) continue;
					glm::vec3 hit_pos;
					if (!p.second->hit && !p.second->killed && p.second->invincible == 0.f && m.HitTest(p.second->aabb/*.Translate(p.second->body.pos)*/, hit_pos)) {
						const glm::vec3 vec{ std::cos(m.rot), std::sin(m.rot), 0.f };
						p.second->Die(hit_pos, camera.vp, debris, particles, assets.debris, vec, (double)t.total, camera);
						++player->score;
						player->Kill(p.second->id, m.id, player->score, hit_pos, vec);
						m.remove = true;
						break;
					}
				}
			}
			if (!player->SkipDeathCheck()) {
				for (const auto& p : players)
					HitTest(*player, *p.second, t.total);
			}
		}
		for (auto it = particles.begin(); it != particles.end();) {
			if (!Cull(it->pos, camera.vp))
				it->Update(t);
			if (it->Kill(t.total))
				it = particles.erase(it);
			else ++it;
		}
		for (auto it = debris.begin(); it != debris.end();) {
			if (!Cull(it->pos, camera.vp))
				it->Update(t);
			if (it->Kill(t.total))
				it = debris.erase(it);
			else ++it;
		}
		globals.audio->Execute();
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
		//std::lock_guard<std::mutex> lock(msgMutex);
		messages.push(std::vector<unsigned char>{ msg, msg + (size_t)len });
	}
	void OnConn(const std::vector<unsigned char>& msg) {
		if (!SanitizeMsg<Conn>(msg.size()))
			return;
		const Conn* conn = reinterpret_cast<const Conn*>(msg.data());
		size_t id = ID5(conn->client_id, 0);
		player = std::make_unique<ProtoX>(id, assets.probe, assets.propulsion.layers.size(), RandomizePos(assets.probe.aabb), missiles, globals.ws.get());
		player->ctrl = static_cast<ProtoX::Ctrl>(conn->ctrl - 48/*TODO:: eliminate conversion*/);
	}
	void SendSessionID() {
		Sess msg{ Tag("SESS") };
		std::copy(std::begin(globals.sessionID), std::end(globals.sessionID), msg.sessionID);
		globals.ws->Send((char*)&msg, sizeof(msg));
	}
	void OnPlyr(const std::vector<unsigned char>& msg) {
		if (!SanitizeMsg<Plyr>(msg.size()))
			return;
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
			auto ptr = std::make_unique<ProtoX>(player->id, assets.probe, assets.propulsion.layers.size(), glm::vec3{ player->x, player->y, 0.f }, missiles);
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
		if (!SanitizeMsg<Misl>(msg.size()))
			return;
		const Misl* misl = reinterpret_cast<const Misl*>(&msg.front());
		ProtoX* proto = nullptr;
		if (player->id == misl->player_id) proto = player.get();
		else {
			auto it = players.find(misl->player_id);
			if (it != players.end()) proto = it->second.get();
		}
		if (!proto) return;
		missiles.emplace_back(glm::vec3(misl->x, misl->y, 0.f ), misl->rot, misl->vel, proto, misl->missile_id, glm::vec3(misl->vx, misl->vy, 0.f));
		missiles.back().life = misl->life;
	}
	void OnScor(const std::vector<unsigned char>& msg, const Time& t) {
		if (!SanitizeMsg<Scor>(msg.size()))
			return;
		if (!player) return;
		const Scor* scor = reinterpret_cast<const Scor*>(&msg.front());
		auto it = std::find_if(missiles.begin(), missiles.end(), [=](const Missile& m) {
			return m.owner->id == scor->owner_id && m.id == scor->missile_id;});
		if (it != missiles.end()) {
			it->owner->score = scor->score;
			RemoveMissile(*it);
		}
		if (scor->owner_id == player->id && player->ctrl == ProtoX::Ctrl::Prop) player->score = scor->score;
		const glm::vec3 hit_pos(scor->x, scor->y, 0.f),
			missile_vec{ scor->vec_x, scor->vec_y, 0.f };
		if (scor->target_id == player->id)
			player->Die(hit_pos, camera.vp, debris, particles, assets.debris, missile_vec, t.total, camera);
		else {
			auto it = players.find(scor->target_id);
			if (it != players.end())
				it->second->Die(hit_pos, camera.vp, debris, particles, assets.debris, missile_vec, t.total, camera);
		}
	}
	void OnKill(const std::vector<unsigned char>& msg, const Asset::Model& debris_model, const Time& t) {
		if (!SanitizeMsg<Kill>(msg.size()))
			return;
		const Kill* kill = reinterpret_cast<const Kill*>(&msg.front());
		auto clientID = ID5(kill->client_id, 0);
		auto it = std::find_if(std::begin(players), std::end(players), [&](const auto& p) {
			return p.second->id == clientID;
		});
		if (it == std::end(players)) return;
		auto& p = it->second;
		auto hit_pos = p->body.pos + glm::vec3{ p->aabb.r - p->aabb.l, p->aabb.t - p->aabb.b, 0.f };
		p->Die(hit_pos, camera.vp, debris, particles, debris_model, {}, t.total, camera);
	}
	void OnCtrl(const std::vector<unsigned char>& msg) {
		if (!SanitizeMsg<Ctrl>(msg.size()))
			return;
		const Ctrl* ctrl = reinterpret_cast<const Ctrl*>(&msg.front());
		SetCtrl(static_cast<ProtoX::Ctrl>(ctrl->ctrl));
	}
	void OnWait(const std::vector<unsigned char>& msg) {
		if (!SanitizeMsg<Wait>(msg.size()))
			return;
		auto wait = reinterpret_cast<const Wait*>(&msg.front());
		this->wait = wait->n;
		//LOG_INFO("OnWait: %d", wait->n);
	}
	void Dispatch(const std::vector<unsigned char>& msg, const Time& t) {
		if (msg.size() < sizeof(size_t))
			return;
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
			OnKill(msg, assets.debris, t);
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
		//std::lock_guard<std::mutex> lock(msgMutex);
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
		globals.audio = std::make_unique<Audio>();
		globals.randomizer = std::make_unique<Randomizer<std::array<ALuint, Audio::PEW_COUNT>>>(globals.audio->pew, mt);
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
		//if (sleep)
		//	::Sleep(150);
//	}
//	catch (...) {
//	LOG_INFO("exception has been thrown\n");
//		throw;
//	}
}