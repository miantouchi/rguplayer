// Copyright 2023 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODULES_SPRITE_H_
#define MODULES_SPRITE_H_

#include "base/math/transform.h"
#include "modules/bitmap.h"
#include "modules/disposable.h"
#include "modules/drawable.h"
#include "modules/flashable.h"
#include "modules/graphics.h"
#include "modules/utility.h"
#include "modules/viewport.h"

namespace modules {

class Sprite : public base::RefCounted<Sprite>,
               public Disposable,
               public ViewportDrawable,
               public Flashable {
 public:
  Sprite(Graphics* screen);
  Sprite(Graphics* screen, scoped_refptr<Viewport> viewport);
  ~Sprite() override;

  Sprite(const Sprite&) = delete;
  Sprite& operator=(const Sprite&) = delete;

  void Update() override;

  int GetWidth() const;
  int GetHeight() const;

  void SetBitmap(scoped_refptr<Bitmap> bitmap);
  scoped_refptr<Bitmap> GetBitmap() const;

  void SetSrcRect(scoped_refptr<Rect> src_rect);
  scoped_refptr<Rect> GetSrcRect() const;

  void SetX(int x);
  int GetX() const;

  void SetY(int y);
  int GetY() const;

  void SetOX(int ox);
  int GetOX() const;

  void SetOY(int oy);
  int GetOY() const;

  void SetZoomX(float zoom_x);
  float GetZoomX() const;

  void SetZoomY(float zoom_y);
  float GetZoomY() const;

  void SetAngle(float angle);
  float GetAngle() const;

  void SetWaveAmp(int wave_amp);
  int GetWaveAmp() const;

  void SetWaveLength(int wave_length);
  int GetWaveLength() const;

  void SetWaveSpeed(int wave_speed);
  int GetWaveSpeed() const;

  void SetWavePhase(int wave_phase);
  int GetWavePhase() const;

  void SetMirror(bool mirror);
  bool GetMirror() const;

  void SetBushDepth(int depth);
  int GetBushDepth() const;

  void SetBushOpacity(int opacity);
  int GetBushOpacity() const;

  void SetOpacity(int opacity);
  int GetOpacity() const;

  void SetBlendMode(renderer::BlendMode mode);
  renderer::BlendMode GetBlendMode() const;

  void SetColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetColor() const;

  void SetTone(scoped_refptr<Tone> tone);
  scoped_refptr<Tone> GetTone() const;

  void InitRefCountedAttributes();

 private:
  void InitSpriteRendererInternal();
  void OnSrcRectChangedInternal();

  void OnObjectDisposed() override;

  void Paint() override;
  void ViewportRectChanged(
      const DrawableManager::DrawableViewport& viewport) override;
  void NeedCheckAccess() const override { CheckedForDispose(); }

  void OnSrcRectChanged();

  Graphics* screen_;

  base::TransformMatrix transform_;

  scoped_refptr<Bitmap> bitmap_;
  scoped_refptr<Rect> src_rect_;
  scoped_refptr<Color> color_;
  scoped_refptr<Tone> tone_;
  renderer::BlendMode blend_mode_ = renderer::BlendMode::Default;
  int opacity_ = 255;
  float fopacity_ = 1.f;
  bool mirror_ = false;

  base::CallbackListSubscription src_rect_observer_;

  std::unique_ptr<renderer::QuadDrawable> quad_;

  base::WeakPtrFactory<Sprite> weak_ptr_factory_{this};
};

}  // namespace modules

#endif  // MODULES_SPRITE_H_