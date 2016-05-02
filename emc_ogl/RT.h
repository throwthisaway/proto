#pragma once
#include "Helpers.h"
#include "Shader/RTShader.h"
#include "Shader/VBlur3x.h"
#include "Shader/HBlur3x.h"
#include "Shader/Contrast.h"
class RT {
	static const size_t	COUNT = 3;
	const int width, height;
	size_t current;
	Shader::RTShader shadowMask;
	Shader::VBlur3x vBlur3x;
	Shader::HBlur3x hBlur3x;
	Shader::Contrast contrastShader;
	GLuint /*vao,*/ vbo1, uv[COUNT], txt[COUNT], rbo, fbo[COUNT], mask_uv;
	struct Target{
		GLuint  fbo, txt, uv;
		GLfloat uw, vh; // pixel ratio
		GLsizei w, h; // vp size
	}rt[COUNT];
	void ShadowMaskStage(size_t index);
	template<typename T>
	void BlurStage(T& shader, size_t index);
	void ContrastStage(size_t index);
	Target GenTarget(int width, int height, size_t index);
	size_t Reset();
public:
	GLuint mask = 0;
	float contrast = 2.14999890f, brightness = 0.550000072f;
	float maskOpacity = 1.f, maskRepeat = .75f;
	RT(int width, int height);
	void GenMaskUVBufferData(float sw, float sh, float iw, float ih);
	void Render();
	size_t Set(size_t index = 0);
	~RT();
};
