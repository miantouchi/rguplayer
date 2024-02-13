// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/graphics.h"

#include "content/config/core_config.h"
#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/worker/binding_worker.h"
#include "content/worker/event_runner.h"
#include "content/worker/renderer_worker.h"
#include "renderer/quad/quad_drawable.h"

#include "SDL_timer.h"

namespace content {

Graphics::Graphics(base::WeakPtr<BindingRunner> dispatcher,
                   scoped_refptr<RenderRunner> renderer,
                   const base::Vec2i& initial_resolution)
    : config_(dispatcher->config()),
      dispatcher_(dispatcher),
      renderer_(renderer),
      resolution_(initial_resolution),
      frozen_(false),
      brightness_(255),
      frame_count_(0),
      frame_rate_(dispatcher->rgss_version() >= RGSSVersion::RGSS2 ? 60 : 40),
      average_fps_(0),
      fps_manager_(std::make_unique<fpslimiter::FPSLimiter>(frame_rate_)),
      fps_display_{0, SDL_GetPerformanceCounter()} {
  // Initial resolution
  viewport_rect().rect = initial_resolution;

  // Init font attributes
  Font::InitStaticFont();

  InitScreenBufferInternal();
}

Graphics::~Graphics() {
  Font::DestroyStaticFont();

  DestroyBufferInternal();
}

int Graphics::GetBrightness() const {
  return brightness_;
}

void Graphics::SetBrightness(int brightness) {
  brightness = std::clamp(brightness, 0, 255);
  brightness_ = brightness;
}

void Graphics::Wait(int duration) {
  for (int i = 0; i < duration; ++i) {
    Update();
  }
}

scoped_refptr<Bitmap> Graphics::SnapToBitmap() {
  scoped_refptr<Bitmap> snap = new Bitmap(this, resolution_.x, resolution_.y);
  SnapToBitmapInternal(snap);
  return snap;
}

void Graphics::FadeOut(int duration) {
  duration = std::max(duration, 1);

  float current_brightness = static_cast<float>(brightness_);
  for (int i = 0; i < duration; ++i) {
    SetBrightness(current_brightness -
                  current_brightness * (i / static_cast<float>(duration)));
    if (frozen_) {
      PresentScreenInternal(frozen_snapshot_);

      FrameProcessInternal();
      if (dispatcher_->CheckFlags())
        break;
    } else {
      Update();
    }
  }

  /* Set final brightness */
  SetBrightness(0);

  /* Raise flags */
  dispatcher_->RaiseFlags();
}

void Graphics::FadeIn(int duration) {
  duration = std::max(duration, 1);

  float current_brightness = static_cast<float>(brightness_);
  float diff = 255.0f - current_brightness;
  for (int i = 0; i < duration; ++i) {
    SetBrightness(current_brightness +
                  diff * (i / static_cast<float>(duration)));

    if (frozen_) {
      PresentScreenInternal(frozen_snapshot_);

      FrameProcessInternal();
      if (dispatcher_->CheckFlags())
        break;
    } else {
      Update();
    }
  }

  /* Set final brightness */
  SetBrightness(255);

  /* Raise flags */
  dispatcher_->RaiseFlags();
}

void Graphics::Update() {
  if (!frozen_) {
    if (fps_manager_->RequireFrameSkip()) {
      if (config_->allow_frame_skip()) {
        return FrameProcessInternal();
      } else {
        fps_manager_->Reset();
      }
    }

    CompositeScreenInternal();
    PresentScreenInternal(screen_buffer_[0]);
  }

  FrameProcessInternal();

  /* Check flags */
  dispatcher_->CheckFlags();
  dispatcher_->RaiseFlags();
}

void Graphics::ResizeScreen(const base::Vec2i& resolution) {
  if (resolution_ == resolution)
    return;

  resolution_ = resolution;
  ResizeResolutionInternal();
}

void Graphics::Reset() {
  /* Reset freeze */
  frozen_ = false;

  /* Disposed all elements */
  for (auto it = disposable_elements_.tail(); it != disposable_elements_.end();
       it = it->previous()) {
    it->value_as_init()->Dispose();
  }

  /* Reset attribute */
  SetFrameRate(dispatcher_->rgss_version() >= RGSSVersion::RGSS2 ? 60 : 40);
  SetBrightness(255);
  FrameReset();
}

void Graphics::Freeze() {
  if (frozen_)
    return;
  FreezeSceneInternal();
  frozen_ = true;
}

void Graphics::Transition(int duration,
                          scoped_refptr<Bitmap> trans_bitmap,
                          int vague) {
  if (trans_bitmap && trans_bitmap->IsDisposed())
    return;

  if (!frozen_)
    return;

  SetBrightness(255);
  vague = std::clamp<int>(vague, 1, 256);

  TransitionSceneInternal(duration, trans_bitmap, vague);
  renderer::GSM.states.blend.Push(false);
  for (int i = 0; i < duration; ++i) {
    TransitionSceneInternalLoop(i, duration, trans_bitmap);
    FrameProcessInternal();

    /* Break draw loop for quit flag */
    if (dispatcher_->CheckFlags())
      break;
  }
  renderer::GSM.states.blend.Pop();

  /* Transition process complete */
  frozen_ = false;

  /* Raise signal notify */
  dispatcher_->RaiseFlags();
}

void Graphics::SetFrameRate(int rate) {
  rate = std::max(rate, 10);
  fps_manager_->SetFrameRate(rate);
  frame_rate_ = rate;
}

int Graphics::GetFrameRate() const {
  return frame_rate_;
}

void Graphics::SetFrameCount(int64_t count) {
  frame_count_ = count;
}

int Graphics::GetFrameCount() const {
  return frame_count_;
}

void Graphics::FrameReset() {
  fps_manager_->Reset();
}

uint64_t Graphics::GetWindowHandle() {
  uint64_t window_handle = 0;
#if defined(OS_WIN)
  window_handle = (uint64_t)SDL_GetProperty(
      SDL_GetWindowProperties(renderer()->window()->AsSDLWindow()),
      "SDL.window.win32.hwnd", NULL);
#else
  // TODO: other platform window handle
#endif
  return window_handle;
}

RGSSVersion Graphics::content_version() const {
  return dispatcher_->rgss_version();
}

filesystem::Filesystem* Graphics::filesystem() {
  return dispatcher_->filesystem();
}

void Graphics::InitScreenBufferInternal() {
  screen_buffer_[0] = renderer::TextureFrameBuffer::Gen();
  renderer::TextureFrameBuffer::Alloc(screen_buffer_[0], resolution_.x,
                                      resolution_.y);
  renderer::TextureFrameBuffer::LinkFrameBuffer(screen_buffer_[0]);

  screen_buffer_[1] = renderer::TextureFrameBuffer::Gen();
  renderer::TextureFrameBuffer::Alloc(screen_buffer_[1], resolution_.x,
                                      resolution_.y);
  renderer::TextureFrameBuffer::LinkFrameBuffer(screen_buffer_[1]);

  frozen_snapshot_ = renderer::TextureFrameBuffer::Gen();
  renderer::TextureFrameBuffer::Alloc(frozen_snapshot_, resolution_.x,
                                      resolution_.y);
  renderer::TextureFrameBuffer::LinkFrameBuffer(frozen_snapshot_);

  screen_quad_ = std::make_unique<renderer::QuadDrawable>();
  screen_quad_->SetPositionRect(base::Vec2(resolution_));
  screen_quad_->SetTexCoordRect(base::Vec2(resolution_));
}

void Graphics::DestroyBufferInternal() {
  renderer::TextureFrameBuffer::Del(screen_buffer_[0]);
  renderer::TextureFrameBuffer::Del(screen_buffer_[1]);
  renderer::TextureFrameBuffer::Del(frozen_snapshot_);

  screen_quad_.reset();
}

void Graphics::CompositeScreenInternal() {
  /* Prepare composite notify */
  DrawableParent::NotifyPrepareComposite();

  renderer::FrameBuffer::Bind(screen_buffer_[0].fbo);
  renderer::GSM.states.clear_color.Set(base::Vec4());
  renderer::FrameBuffer::Clear();

  renderer::GSM.states.scissor_rect.Set(resolution_);
  renderer::GSM.states.viewport.Set(resolution_);

  /* Composite screen to screen buffer */
  DrawableParent::CompositeChildren();

  if (brightness_ < 255) {
    auto& shader = renderer::GSM.shaders->flat;
    shader.Bind();
    shader.SetProjectionMatrix(resolution_);
    shader.SetColor(base::Vec4(0, 0, 0, (255 - brightness_) / 255.0f));

    screen_quad_->Draw();
  }
}

void Graphics::ResizeResolutionInternal() {
  renderer::TextureFrameBuffer::Alloc(screen_buffer_[0], resolution_.x,
                                      resolution_.y);
  renderer::TextureFrameBuffer::Alloc(screen_buffer_[1], resolution_.x,
                                      resolution_.y);
  renderer::TextureFrameBuffer::Alloc(frozen_snapshot_, resolution_.x,
                                      resolution_.y);

  screen_quad_->SetPositionRect(base::Vec2(resolution_));
  screen_quad_->SetTexCoordRect(base::Vec2(resolution_));

  viewport_rect().rect = resolution_;
  NotifyViewportChanged();
}

void Graphics::PresentScreenInternal(
    const renderer::TextureFrameBuffer& screen_buffer) {
  base::WeakPtr<ui::Widget> window = renderer_->window();
  UpdateWindowViewportInternal();

  // Flip screen for Y
  base::Rect target_rect(display_viewport_.x,
                         display_viewport_.y + display_viewport_.height,
                         display_viewport_.width, -display_viewport_.height);

  // Blit screen buffer to window buffer
  renderer::Blt::BeginScreen(window_size_);
  renderer::FrameBuffer::ClearColor();
  renderer::FrameBuffer::Clear();
  renderer::Blt::TexSource(screen_buffer);
  renderer::Blt::BltDraw(resolution_, target_rect);
  renderer::Blt::EndDraw();

  SDL_GL_SwapWindow(window->AsSDLWindow());
}

void Graphics::SnapToBitmapInternal(scoped_refptr<Bitmap> target) {
  CompositeScreenInternal();

  renderer::Blt::BeginDraw(target->AsGLType());
  renderer::Blt::TexSource(screen_buffer_[0]);
  renderer::Blt::BltDraw(resolution_, resolution_);
  renderer::Blt::EndDraw();
}

void Graphics::FreezeSceneInternal() {
  CompositeScreenInternal();

  renderer::Blt::BeginDraw(frozen_snapshot_);
  renderer::Blt::TexSource(screen_buffer_[0]);
  renderer::Blt::BltDraw(resolution_, resolution_);
  renderer::Blt::EndDraw();
}

void Graphics::TransitionSceneInternal(int duration,
                                       scoped_refptr<Bitmap> trans_bitmap,
                                       int vague) {
  // Snap to backend buffer
  CompositeScreenInternal();

  auto& alpha_shader = renderer::GSM.shaders->alpha_trans;
  auto& vague_shader = renderer::GSM.shaders->vague_shader;

  if (!trans_bitmap) {
    alpha_shader.Bind();
    alpha_shader.SetProjectionMatrix(
        renderer::GSM.states.viewport.Current().Size());
    alpha_shader.SetTransOffset(base::Vec2());
    alpha_shader.SetTextureSize(resolution_);
  } else {
    vague_shader.Bind();
    vague_shader.SetProjectionMatrix(
        renderer::GSM.states.viewport.Current().Size());
    vague_shader.SetTransOffset(base::Vec2());
    vague_shader.SetTextureSize(resolution_);
    vague_shader.SetVague(vague / 256.0f);
  }
}

void Graphics::TransitionSceneInternalLoop(int i,
                                           int duration,
                                           scoped_refptr<Bitmap> trans_bitmap) {
  auto& alpha_shader = renderer::GSM.shaders->alpha_trans;
  auto& vague_shader = renderer::GSM.shaders->vague_shader;
  float progress = i * (1.0f / duration);

  if (!trans_bitmap) {
    alpha_shader.Bind();
    alpha_shader.SetFrozenTexture(frozen_snapshot_.tex);
    alpha_shader.SetCurrentTexture(screen_buffer_[0].tex);
    alpha_shader.SetProgress(progress);
  } else {
    vague_shader.Bind();
    vague_shader.SetFrozenTexture(frozen_snapshot_.tex);
    vague_shader.SetCurrentTexture(screen_buffer_[0].tex);
    vague_shader.SetTransTexture(trans_bitmap->AsGLType().tex);
    vague_shader.SetProgress(progress);
  }

  renderer::FrameBuffer::Bind(screen_buffer_[1].fbo);
  renderer::FrameBuffer::Clear();
  screen_quad_->Draw();

  // present with backend buffer
  PresentScreenInternal(screen_buffer_[1]);
}

void Graphics::FrameProcessInternal() {
  /* Control frame delay */
  fps_manager_->Delay();

  /* Increase frame render count */
  ++frame_count_;

  /* Update average fps */
  UpdateAverageFPSInternal();
}

void Graphics::AddDisposable(Disposable* disp) {
  disposable_elements_.Append(&disp->link_);
}

void Graphics::RemoveDisposable(Disposable* disp) {
  disp->link_.RemoveFromList();
}

void Graphics::RenderEffectRequire(const base::Vec4& color,
                                   const base::Vec4& tone,
                                   const base::Vec4& flash_color) {
  ApplyViewportEffect(screen_buffer_[0], screen_buffer_[1], *screen_quad_,
                      color, tone, flash_color);
}

void Graphics::ApplyViewportEffect(renderer::TextureFrameBuffer& frontend,
                                   renderer::TextureFrameBuffer& backend,
                                   renderer::QuadDrawable& quad,
                                   const base::Vec4& color,
                                   const base::Vec4& tone,
                                   const base::Vec4& flash_color) {
  const base::Rect& viewport_rect = renderer::GSM.states.scissor_rect.Current();
  const base::Rect& screen_rect = resolution_;

  const bool has_tone_effect =
      (tone.x != 0 || tone.y != 0 || tone.z != 0 || tone.w != 0);
  const bool has_color_effect = color.w != 0;
  const bool has_flash_effect = flash_color.w != 0;

  if (!has_tone_effect && !has_color_effect && !has_flash_effect)
    return;

  renderer::GSM.states.scissor.Push(false);
  renderer::Blt::BeginDraw(backend);
  renderer::Blt::TexSource(frontend);
  renderer::Blt::BltDraw(screen_rect, screen_rect);
  renderer::Blt::EndDraw();
  renderer::GSM.states.scissor.Pop();

  renderer::FrameBuffer::Bind(frontend.fbo);
  auto& shader = renderer::GSM.shaders->viewport;
  shader.Bind();
  shader.SetProjectionMatrix(renderer::GSM.states.viewport.Current().Size());
  shader.SetTone(tone);
  shader.SetColor((flash_color.w > color.w) ? flash_color : color);

  shader.SetTexture(backend.tex);
  shader.SetTextureSize(screen_rect.Size());

  renderer::GSM.states.blend.Push(false);
  quad.Draw();
  renderer::GSM.states.blend.Pop();
}

void Graphics::UpdateAverageFPSInternal() {
  SDL_Event quit_event;
  quit_event.type =
      dispatcher_->user_event_id() + EventRunner::UPDATE_FPS_DISPLAY;
  SDL_PushEvent(&quit_event);
}

void Graphics::UpdateWindowViewportInternal() {
  base::WeakPtr<ui::Widget> window = renderer_->window();
  window_size_ = window->GetSize();

  float window_ratio = static_cast<float>(window_size_.x) / window_size_.y;
  float screen_ratio = static_cast<float>(resolution_.x) / resolution_.y;

  display_viewport_.width = window_size_.x;
  display_viewport_.height = window_size_.y;

  if (screen_ratio > window_ratio)
    display_viewport_.height = display_viewport_.width / screen_ratio;
  else if (screen_ratio < window_ratio)
    display_viewport_.width = display_viewport_.height * screen_ratio;

  display_viewport_.x = (window_size_.x - display_viewport_.width) / 2.0f;
  display_viewport_.y = (window_size_.y - display_viewport_.height) / 2.0f;
}

}  // namespace content
