// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_ohos.h"

#include "base/ohos/sys_info_utils.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/base/ui_base_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeOhos::Create() {
  return base::AdoptRef(new LayoutThemeOhos());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeOhos::Create()));
  return *layout_theme;
}

LayoutThemeOhos::~LayoutThemeOhos() {}

Color LayoutThemeOhos::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF99C8FF)
             : LayoutThemeMobile::PlatformActiveSelectionBackgroundColor(
                   color_scheme);
}

Color LayoutThemeOhos::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF3B3B3B)
             : LayoutThemeMobile::PlatformActiveSelectionForegroundColor(
                   color_scheme);
}

void LayoutThemeOhos::AdjustInnerSpinButtonStyle(
    ComputedStyleBuilder& builder) const {
  if (!base::ohos::IsPcDevice()) {
    // For other devices, InnerSpinButton should not be displayed.
    // This Matches LayoutThemeMobile::AdjustInnerSpinButtonStyle.
    return;
  }
  LayoutThemeDefault::AdjustInnerSpinButtonStyle(builder);
}

}  // namespace blink
