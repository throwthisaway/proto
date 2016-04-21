#pragma once
#include "Helpers.h"
#include "Shader/RTShader.h"
class RT {
	Shader::RTShader shader;
	GLuint vao, vbo1, vbo2, txt, rbo, fbo, mask_uv;
	const size_t /*sw, hw,*/ w, h;
	const float u, v;
public:
	GLuint mask = 0;
	float maskOpacity = 1.f, maskRepeat = .75f;
	RT(unsigned int width, unsigned int height) :/* sw(width), hw(width),*/ w(RoundToPowerOf2(width)),
		h(RoundToPowerOf2(height)),
		u((float)width / w),
		v((float)height / h) {
		static const GLfloat g_vertex_buffer_data[] = { -1.0f, -1.0f, 0.0f,
			1.f, -1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f,
			1.f, 1.f, 0.0f };
		glGenBuffers(1, &vbo1);
		glBindBuffer(GL_ARRAY_BUFFER, vbo1);
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

		static const GLfloat g_uv_buffer_data[] = { 0.f, 0.f,
			u,.0f,
			0.f,  v,
			u, v };
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

		glGenBuffers(1, &mask_uv);
	}

	void GenMaskUVBufferData(float sw, float sh, float iw, float ih) {
		//const GLfloat uv_buffer_data[] = { 0.f, 0.f,
		//	1.f,.0f,
		//	0.f,  1.0f,
		//	1.f, 1.f }; 
		/*const GLfloat uv_buffer_data[] = { 0.f, 0.f,
		sw / iw,.0f,
		0.f,  sh / ih,
		sw / iw, sh / ih };*/
		const auto w = sw / iw, h = sh / ih;
		const GLfloat uv_buffer_data[] = { 0.f, 0.f,
			w, .0f,
			0.f, h,
			w, h };
		glBindBuffer(GL_ARRAY_BUFFER, mask_uv);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);
	}
	void Set() {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		//glViewport(0, 0, sw, hw);
	}
	void Reset() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		//glViewport(0, 0, sw, hw);
		glDisable(GL_DEPTH_TEST);
	}
	void Render() {
		Reset();
		//glEnable(GL_TEXTURE);
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
		glBindBuffer(GL_ARRAY_BUFFER, vbo2);
		glVertexAttribPointer(shader.aRT,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
		// mask uv
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, mask_uv);
		glVertexAttribPointer(shader.aMask,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0);
#endif
		glUniform1f(shader.uMaskOpacity, maskOpacity);
		glUniform1f(shader.uMaskVRepeat, maskRepeat);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, txt);
		glUniform1i(shader.uSmpRT, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, mask);
		glUniform1i(shader.uSmpMask, 1);
		//glUniform2f(shader.uScreenSize, (GLfloat)sw, (GLfloat)hw);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glDisable(GL_TEXTURE);
		glActiveTexture(GL_TEXTURE0);
#ifndef VAO_SUPPORT
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
#endif
	}
	~RT() {
		glDeleteFramebuffers(1, &fbo);
		glDeleteRenderbuffers(1, &rbo);
		glDeleteBuffers(1, &vbo1);
		glDeleteBuffers(1, &vbo2);
		glDeleteBuffers(1, &mask_uv);
#ifdef VAO_SUPPORT
		glDeleteVertexArrays(1, &vao);
#endif
	}
};
