// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_OHOS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_OHOS_H_

#include "third_party/blink/renderer/core/layout/layout_theme_mobile.h"

namespace blink {

class LayoutThemeOhos final : public LayoutThemeMobile {
 public:
  static scoped_refptr<LayoutTheme> Create();

  bool DelegatesMenuListRendering() const override { return false; }
  Color PlatformActiveSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const override;
  Color PlatformActiveSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const override;
  void AdjustInnerSpinButtonStyle(ComputedStyleBuilder&) const override;
  bool SupportsSelectionForegroundColors() const override { return false; }

 private:
  ~LayoutThemeOhos() override;
  static constexpr Color kDefaultActiveSelectionBackgroundColor =
      Color::FromRGBA32(0x330a59f7);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_OHOS_H_
