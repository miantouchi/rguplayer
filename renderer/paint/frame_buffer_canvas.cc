// Copyright 2023 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/paint/frame_buffer_canvas.h"

namespace renderer {

GLTexture::GLTexture(scoped_refptr<gpu::GLES2CommandContext> context)
    : context_(context) {
  context_->glGenTextures(1, &texture_);
}

GLTexture::~GLTexture() { context_->glDeleteTextures(1, &texture_); }

void GLTexture::AllocEmpty() {
  context_->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, 0);
}

void GLTexture::BufferData(const void* data, GLenum format) {
  context_->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, format,
                         GL_UNSIGNED_BYTE, data);
}

void GLTexture::BufferData(const base::Vec4i& bounds, const void* data,
                           GLenum format) {
  context_->glTexSubImage2D(GL_TEXTURE_2D, 0, bounds.x, bounds.y, bounds.z,
                            bounds.w, format, GL_UNSIGNED_BYTE, data);
}

void GLTexture::Activate(uint16_t tex_unit) {
  context_->glActiveTexture(GL_TEXTURE0 + tex_unit);
}

void GLTexture::Bind() { context_->glBindTexture(GL_TEXTURE_2D, texture_); }

void GLTexture::Unbind() { context_->glBindTexture(GL_TEXTURE_2D, 0); }

void GLTexture::SetTextureWrap(GLint mode) {
  context_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
  context_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
}

void GLTexture::SetTextureFilter(GLint mode) {
  context_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
  context_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
}

GLFrameBuffer::GLFrameBuffer(scoped_refptr<gpu::GLES2CommandContext> context)
    : context_(context) {
  context_->glGenFramebuffers(1, &frame_buffer_);
}

GLFrameBuffer::~GLFrameBuffer() {
  context_->glDeleteFramebuffers(1, &frame_buffer_);
}

void GLFrameBuffer::SetRenderTarget(scoped_refptr<GLTexture> target) {
  texture_ = target;

  context_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture_->GetTextureRaw(), 0);
}

void GLFrameBuffer::ReadPixels(const base::Vec4i& bounds, GLenum format,
                               GLenum data_type, void* pixels) {
  context_->glReadPixels(bounds.x, bounds.y, bounds.z, bounds.w, format,
                         data_type, pixels);
}

void GLFrameBuffer::Bind() {
  context_->glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_);
}

void GLFrameBuffer::Unbind() { context_->glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void GLFrameBuffer::Clear() { Clear(base::Vec4(0, 0, 0, 0)); }

void GLFrameBuffer::Clear(const base::Vec4& color) {
  context_->glClearColor(color.x, color.y, color.z, color.w);
  context_->glClear(GL_COLOR_BUFFER_BIT);
}

void GLFrameBuffer::BltBegin(CCLayer* cc, GLFrameBuffer* target,
                             const base::Vec2i& size) {
  auto* shader = cc->Shaders()->base_shader.get();

  cc->States()->viewport->Push(base::Rect(base::Vec2i(), size));
  cc->States()->blend->Push(false);

  if (target) target->Bind();

  shader->Bind();
  shader->SetViewportMatrix(size);
  shader->SetTransOffset(base::Vec2i());
}

void GLFrameBuffer::BltSource(CCLayer* cc, scoped_refptr<GLTexture> target) {
  auto* shader = cc->Shaders()->base_shader.get();

  shader->SetTexture(target->GetTextureRaw());
  shader->SetTextureSize(target->GetSize());
}

void GLFrameBuffer::BltClear(CCLayer* cc) {
  cc->GetContext()->glClearColor(0, 0, 0, 0);
  cc->GetContext()->glClear(GL_COLOR_BUFFER_BIT);
}

void GLFrameBuffer::BltEnd(CCLayer* cc, GLFrameBuffer* target,
                           const base::Rect& src_rect,
                           const base::Rect& dst_rect) {
  auto* quad = cc->GetQuad();

  quad->SetPosition(dst_rect);
  quad->SetTexcoord(src_rect);
  quad->Draw();

  cc->States()->blend->Pop();
  cc->States()->viewport->Pop();

  if (target) target->Unbind();
}

DoubleFrameBuffer::DoubleFrameBuffer(
    scoped_refptr<gpu::GLES2CommandContext> context, const base::Vec2i& size) {
  frames_[0].texture = new GLTexture(context);
  frames_[0].texture->Bind();
  frames_[0].texture->SetSize(size);
  frames_[0].texture->SetTextureFilter(GL_NEAREST);
  frames_[0].texture->AllocEmpty();

  frames_[0].frame_buffer.reset(new GLFrameBuffer(context));
  frames_[0].frame_buffer->Bind();
  frames_[0].frame_buffer->SetRenderTarget(frames_[0].texture);
  frames_[0].frame_buffer->Clear();
  frames_[0].frame_buffer->Unbind();

  frames_[1].texture = new GLTexture(context);
  frames_[1].texture->Bind();
  frames_[1].texture->SetSize(size);
  frames_[1].texture->SetTextureFilter(GL_NEAREST);
  frames_[1].texture->AllocEmpty();

  frames_[1].frame_buffer.reset(new GLFrameBuffer(context));
  frames_[1].frame_buffer->Bind();
  frames_[1].frame_buffer->SetRenderTarget(frames_[1].texture);
  frames_[1].frame_buffer->Clear();
  frames_[1].frame_buffer->Unbind();
}

DoubleFrameBuffer::~DoubleFrameBuffer() {}

void DoubleFrameBuffer::Resize(const base::Vec2i& size) {
  frames_[0].texture->Bind();
  frames_[0].texture->SetSize(size);
  frames_[0].texture->AllocEmpty();

  frames_[1].texture->Bind();
  frames_[1].texture->SetSize(size);
  frames_[1].texture->AllocEmpty();
}

void DoubleFrameBuffer::Swap() { std::swap(frames_[0], frames_[1]); }

}  // namespace renderer