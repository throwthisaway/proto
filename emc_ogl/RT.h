#pragma once
#include <glm/glm.hpp>
#include "Helpers.h"
#include "shader/CRTShader.h"
#include "shader/VBlur7x.h"
#include "shader/HBlur7x.h"
#include "shader/VBlur9x.h"
#include "shader/HBlur9x.h"
#include "shader/Contrast.h"
#include "shader/Spherical.h"
//#include "Shader/BloomPass1.h"
//#include "Shader/BloomPass2.h"
#include "shader/CombineMAdd.h"
#include "shader/Highlight.h"
class RT {
	static const size_t	COUNT = 3;
	const int width, height;
	size_t current;
	Shader::VBlur7x vBlur7x;
	Shader::HBlur7x hBlur7x;
	Shader::VBlur9x vBlur9x;
	Shader::HBlur9x hBlur9x;
	Shader::Contrast contrastShader;
	Shader::Spherical sphericalShader;
	//Shader::BloomPass1 bloomPass1;
	//Shader::BloomPass2 bloomPass2;
	Shader::Highlight highlight;
	Shader::CombineMAdd combineMAdd;
	GLuint /*vao,*/ vbo1, uv, txt[COUNT], rbo, fbo[COUNT], mask_uv;
	struct Target{
		GLuint  fbo, txt;
		GLsizei w, h; // vp size
	}rt[COUNT];
	void ShadowMaskStage(size_t index);
	template<typename T>
	void BlurStage(T& shader, size_t index, float offsetMul = 1.f);
	void ContrastStage(size_t index);
	void SphericalStage(size_t index);
	//void BloomPass1Stage(size_t index);
	//void BloomPass2Stage(size_t index, GLuint txt2);
	void Highlight(size_t index);
	void CombineMAdd(size_t index, GLuint txt2);
	Target GenTarget(int width, int height, size_t index);
	size_t Reset();
public:
	Shader::CRTShader shadowMask;
	float contrast = 2.14999890f, brightness = 0.550000072f;
	float /*maskOpacity = 0.149999812f, maskRepeat = .75f,*/ crtRadius = 3.f,
		bloomThreshold = .2f, bloomRamp = .1f, bloomMix = .7f;
	RT(int width, int height);
	void GenMaskUVBufferData(float sw, float sh, float iw, float ih);
	void Render();
	size_t Set(size_t index = 0);
	void GenRenderTargets();
	glm::ivec2 GetCurrentRes() const;
	~RT();
};
