// Copyright 2023 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/script/drawable.h"

namespace content {

Drawable::Drawable(DrawableParent* parent, int z, bool visible)
    : parent_(parent), z_(z), visible_(visible) {
  parent_->InsertDrawable(this);
}

Drawable::~Drawable() {
  if (parent_) RemoveFromList();
}

void Drawable::SetParent(DrawableParent* parent) {
  CheckDisposed();

  if (parent_) RemoveFromList();

  parent_ = parent;
  parent_->InsertDrawable(this);

  OnViewportRectChanged(parent->viewport_rect());
}

void Drawable::SetZ(int z) {
  CheckDisposed();

  if (!parent_ || z_ == z) return;

  z_ = z;

  RemoveFromList();
  parent_->InsertDrawable(this);
}

DrawableParent::DrawableParent() {}

DrawableParent::~DrawableParent() {
  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next()) {
    it->value()->parent_ = nullptr;
  }
}

void DrawableParent::InsertDrawable(Drawable* drawable) {
  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next())
    if (it->value()->z_ < drawable->z_)
      return it->value()->InsertBefore(drawable);

  drawables_.Append(drawable);
}

void DrawableParent::CompositeChildren() {
  if (drawables_.empty()) return;

  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next()) {
    it->value()->Composite();
  }
}

void DrawableParent::NotifyViewportChanged() {
  for (auto it = drawables_.head(); it != drawables_.end(); it = it->next()) {
    it->value()->OnViewportRectChanged(viewport_rect_);
  }
}

}  // namespace content