// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_FONT_H_
#define CONTENT_PUBLIC_FONT_H_

#include "SDL_ttf.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/public/utility.h"

namespace content {

class Font : public base::RefCounted<Font> {
 public:
  static void InitStaticFont();

  static bool Existed(const std::string& name);

  static void SetDefaultName(const std::vector<std::string>& name);
  static std::vector<std::string> GetDefaultName();
  static void SetDefaultSize(int size);
  static int GetDefaultSize();
  static void SetDefaultBold(bool bold);
  static bool GetDefaultBold();
  static void SetDefaultItalic(bool italic);
  static bool GetDefaultItalic();
  static void SetDefaultShadow(bool shadow);
  static bool GetDefaultShadow();
  static void SetDefaultOutline(bool outline);
  static bool GetDefaultOutline();
  static void SetDefaultColor(scoped_refptr<Color> color);
  static scoped_refptr<Color> GetDefaultColor();
  static void SetDefaultOutColor(scoped_refptr<Color> color);
  static scoped_refptr<Color> GetDefaultOutColor();

  Font();
  Font(const std::vector<std::string>& name);
  Font(const std::vector<std::string>& name, int size);
  ~Font();

  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;

  void SetName(const std::vector<std::string>& name);
  std::vector<std::string> GetName() const;
  void SetSize(int size);
  int GetSize() const;
  void SetBold(bool bold);
  bool GetBold() const;
  void SetItalic(bool italic);
  bool GetItalic() const;
  void SetShadow(bool shadow);
  bool GetShadow() const;
  void SetOutline(bool outline);
  bool GetOutline() const;
  void SetColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetColor() const;
  void SetOutColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetOutColor() const;

  TTF_Font* AsSDLFont();

 private:
  std::vector<std::string> name_;
  int size_ = 24;
  bool bold_ = false;
  bool italic_ = false;
  bool outline_ = true;
  bool shadow_ = false;
  scoped_refptr<Color> color_;
  scoped_refptr<Color> out_color_;

  TTF_Font* sdl_font_ = nullptr;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_FONT_H_