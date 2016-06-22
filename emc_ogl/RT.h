#pragma once
#include <glm/glm.hpp>
#include "Helpers.h"
#include "Shader/RTShader.h"
#include "Shader/VBlur3x.h"
#include "Shader/HBlur3x.h"
#include "Shader/Contrast.h"
#include "Shader/Spherical.h"
class RT {
	static const size_t	COUNT = 3;
	const int width, height;
	size_t current;
	Shader::VBlur3x vBlur3x;
	Shader::HBlur3x hBlur3x;
	Shader::Contrast contrastShader;
	Shader::Spherical sphericalShader;
	GLuint /*vao,*/ vbo1, uv, txt[COUNT], rbo, fbo[COUNT], mask_uv;
	struct Target{
		GLuint  fbo, txt;
		GLsizei w, h; // vp size
	}rt[COUNT];
	void ShadowMaskStage(size_t index);
	template<typename T>
	void BlurStage(T& shader, size_t index);
	void ContrastStage(size_t index);
	void SphericalStage(size_t index);
	Target GenTarget(int width, int height, size_t index);
	size_t Reset();
public:
	Shader::RTShader shadowMask;
	GLuint mask = 0;
	float contrast = 2.14999890f, brightness = 0.550000072f;
	float maskOpacity = 1.f, maskRepeat = .75f, crtRadius = 3.f;
	RT(int width, int height);
	void GenMaskUVBufferData(float sw, float sh, float iw, float ih);
	void Render();
	size_t Set(size_t index = 0);
	void GenRenderTargets(GLuint mask, int maskW, int maskH);
	glm::ivec2 GetCurrentRes() const;
	~RT();
};
