#include "RT.h"

RT::RT(int width, int height)  : width(width), height(height), current(0) {
	static const GLfloat data[] = { -1.0f, -1.0f, 0.0f,
		1.f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		1.f, 1.f, 0.0f };
	glGenBuffers(1, &vbo1);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

	glGenBuffers(1, &mask_uv);

	//// Depth texture...
	//glGenRenderbuffers(1, &rbo);
	//glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, RoundToPowerOf2(width), RoundToPowerOf2(height));
	////...Depth texture

	{
		glGenBuffers(1, &uv);
		const GLfloat u = 1.f, v = 1.f;
		const GLfloat data[] = { 0.f, 0.f,
			u, .0f,
			0.f, v-.0001f/*TODO:: aperture grill distortion fix*/,
			u, v };
		glBindBuffer(GL_ARRAY_BUFFER, uv);
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
	}
	glGenTextures(COUNT, txt);
	glGenFramebuffers(COUNT, fbo);

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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	//glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void RT::GenMaskUVBufferData(float sw, float sh, float iw, float ih) {
	const auto w = sw / iw, h = sh / ih;
	const GLfloat data[] = { 0.f, 0.f,
		w, .0f,
		0.f, h,
		w, h };
	glBindBuffer(GL_ARRAY_BUFFER, mask_uv);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
}
size_t RT::Set(size_t index) {
	auto res = current;
	current = index;
	glBindFramebuffer(GL_FRAMEBUFFER, rt[index].fbo);
	glViewport(0, 0, rt[index].w, rt[index].h);
	return res;
}

size_t RT::Reset() {
	auto res = current;
	current = 0;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	return res;
}

void RT::GenRenderTargets() {
	size_t index = 0;
	//rt[index] = GenTarget(width / (maskW * maskRepeat), width / maskH, index);
	rt[index] = GenTarget(width, height, index);
	index++;
	rt[index] = GenTarget(width, height, index);
	index++;
	rt[index] = GenTarget(width, height, index);
	//GenMaskUVBufferData((float)width, (float)height, maskW, maskH);
}

void RT::Render() {
	auto back = Set(1);
	BlurStage(hBlur7x, back);
	back = Set(0);
	BlurStage(vBlur7x, back);
	back = Set(1);
	SphericalStage(back);
	/*back = Set(1);
	ContrastStage(back);*/
	size_t src = back, bloom = 2,
		original = Set(bloom);
	back = original;
	Highlight(back);
	back = Set(src);
	BlurStage(hBlur9x, back);
	back = Set(bloom);
	BlurStage(vBlur9x, back);
	back = Set(src);
	BlurStage(hBlur9x, back, 2.f);
	back = Set(bloom);
	BlurStage(vBlur9x, back, 2.f);
	back = Set(src);
	BlurStage(hBlur9x, back, 3.f);
	back = Set(bloom);
	BlurStage(vBlur9x, back, 3.f);

	bloom = Set(src);
	ShadowMaskStage(original);

	back = Reset();
	CombineMAdd(bloom, rt[back].txt);
	//--------------------------------
	//auto back = Set(1);
	//BlurStage(hBlur7x, back);
	//back = Set(0);
	//BlurStage(vBlur7x, back);
	//back = Set(1);
	//SphericalStage(back);
	//back = Set(0);
	//ShadowMaskStage(back);
	///*back = Set(1);
	//ContrastStage(back);*/
	//size_t src = back, bloom = 2,
	//	original = Set(bloom);
	//back = original;
	//Highlight(back);
	//back = Set(src);
	//BlurStage(hBlur9x, back);
	//back = Set(bloom);
	//BlurStage(vBlur9x, back);
	//back = Set(src);
	//BlurStage(hBlur9x, back, 2.f);
	//back = Set(bloom);
	//BlurStage(vBlur9x, back, 2.f);
	//back = Set(src);
	//BlurStage(hBlur9x, back, 3.f);
	//back = Set(bloom);
	//BlurStage(vBlur9x, back, 3.f);
	//back = Reset();
	//CombineMAdd(back, rt[original].txt);
	glActiveTexture(GL_TEXTURE0);
#ifndef VAO_SUPPORT
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
#endif
}
RT::~RT() {
	glDeleteFramebuffers(sizeof(fbo) / sizeof(fbo[0]), fbo);
	glDeleteBuffers(1, &uv);
	glDeleteTextures(sizeof(txt) / sizeof(txt[0]), txt);
	glDeleteRenderbuffers(1, &rbo);
	glDeleteBuffers(1, &vbo1);
	glDeleteBuffers(1, &mask_uv);
#ifdef VAO_SUPPORT
	glDeleteVertexArrays(1, &vao);
#endif
}
void RT::ShadowMaskStage(size_t index) {
	auto& shader = shadowMask;
	glUseProgram(shader.id);
#ifdef VAO_SUPPORT
	glBindVertexArray(vao);
#else
	// render target quad vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	// render target uv
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
#endif
	glUniform2f(shader.uScreenSize, width, height);
	glUniform2f(shader.uTexelSize, float(1./width), float(1./height));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glUniform1i(shader.uSmpRT, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
template<typename T>
void RT::BlurStage(T& shader, size_t index, float offsetMul) {
	// render target quad vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	// render target uv
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glUseProgram(shader.id);
	glUniform1i(shader.uSmp, 0);
	glUniform2f(shader.uOffset, 1.f/rt[index].w * offsetMul, 1.f/rt[index].h * offsetMul);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void RT::ContrastStage(size_t index) {
	auto& shader = contrastShader;
	glUseProgram(shader.id);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	// render target uv
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glUniform1i(shader.uSmp, 0);
	glUniform1f(shader.uContrast, contrast);
	glUniform1f(shader.uBrightness, brightness);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void RT::SphericalStage(size_t index) {
	auto& shader = sphericalShader;
	glUseProgram(shader.id);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	// render target uv
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glUniform1i(shader.uSmp, 0);
	glUniform1f(shader.uR, crtRadius);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
RT::Target RT::GenTarget(int width, int height, size_t index) {
	 Target res{ fbo[index], txt[index], width, height};

	glBindTexture(GL_TEXTURE_2D, txt[index]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo[index]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, txt[index], 0);
	return res;
}

//void RT::BloomPass1Stage(size_t index) {
//	auto& shader = bloomPass1;
//	glEnableVertexAttribArray(0);
//	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
//	glVertexAttribPointer(shader.aPos,
//		3,
//		GL_FLOAT,
//		GL_FALSE,
//		0,
//		(void*)0);
//	// render target uv
//	glEnableVertexAttribArray(1);
//	glBindBuffer(GL_ARRAY_BUFFER, uv);
//	glVertexAttribPointer(shader.aUV,
//		2,
//		GL_FLOAT,
//		GL_FALSE,
//		0,
//		(void*)0);
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
//	glUseProgram(shader.id);
//	glUniform1i(shader.uSmp, 0);
//	glUniform2f(shader.uOffset, 1.f / rt[index].w, 1.f / rt[index].h);
//	glUniform1f(shader.uThreshold, bloomThreshold);
//	glUniform1f(shader.uRamp, bloomRamp);
//	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//}
//
//void RT::BloomPass2Stage(size_t index, GLuint txt2) {
//	auto& shader = bloomPass2;
//	glEnableVertexAttribArray(0);
//	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
//	glVertexAttribPointer(shader.aPos,
//		3,
//		GL_FLOAT,
//		GL_FALSE,
//		0,
//		(void*)0);
//	// render target uv
//	glEnableVertexAttribArray(1);
//	glBindBuffer(GL_ARRAY_BUFFER, uv);
//	glVertexAttribPointer(shader.aUV,
//		2,
//		GL_FLOAT,
//		GL_FALSE,
//		0,
//		(void*)0);
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
//	glActiveTexture(GL_TEXTURE1);
//	glBindTexture(GL_TEXTURE_2D, txt2);
//	glUseProgram(shader.id);
//	glUniform1i(shader.uSmp1, 0);
//	glUniform1i(shader.uSmp2, 1);
//	glUniform2f(shader.uOffset, 1.f / rt[index].w, 1.f / rt[index].h);
//	glUniform1f(shader.uMix, bloomMix);
//	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//	glActiveTexture(GL_TEXTURE0);
//}

glm::ivec2 RT::GetCurrentRes() const {
	return{ rt[current].w, rt[current].h };
}

void RT::Highlight(size_t index) {
	auto& shader = highlight;
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glUseProgram(shader.id);
	glUniform1i(shader.uSmp, 0);
	glUniform1f(shader.uThreshold, bloomThreshold);
	glUniform1f(shader.uRamp, bloomRamp);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void RT::CombineMAdd(size_t index, GLuint txt2) {
	auto& shader = combineMAdd;
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glVertexAttribPointer(shader.aPos,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv);
	glVertexAttribPointer(shader.aUV,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt[index].txt);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, txt2);
	glUseProgram(shader.id);
	glUniform1i(shader.uSmp1, 0);
	glUniform1i(shader.uSmp2, 1);
	glUniform1f(shader.uMix, bloomMix);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glActiveTexture(GL_TEXTURE0);
}