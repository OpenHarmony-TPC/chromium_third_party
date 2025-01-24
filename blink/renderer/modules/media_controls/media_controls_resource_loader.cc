// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/media_controls/resources/grit/media_controls_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if BUILDFLAG(IS_OHOS) && defined(OHOS_MEDIA)
#include "base/ohos/sys_info_utils.h"
#endif

namespace {

bool ShouldLoadAndroidCSS() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return blink::RuntimeEnabledFeatures::MobileLayoutThemeEnabled();
#endif
}

}  // namespace

namespace blink {

MediaControlsResourceLoader::MediaControlsResourceLoader()
    : UAStyleSheetLoader() {}

MediaControlsResourceLoader::~MediaControlsResourceLoader() = default;

String MediaControlsResourceLoader::GetMediaControlsCSS() const {
#if defined(OHOS_VIDEO_ASSISTANT)
  if (custom_media_player_enabled_) {
    return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_HM_CSS);
  }
#endif
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_CSS);
}

String MediaControlsResourceLoader::GetMediaControlsAndroidCSS() const {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS);
}

#if BUILDFLAG(IS_OHOS) && defined(OHOS_MEDIA)
String MediaControlsResourceLoader::GetMediaControlsOHOSCSS() const {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_OHOS_CSS);
}
#endif

// static
String MediaControlsResourceLoader::GetShadowLoadingStyleSheet() {
  return UncompressResourceAsString(IDR_SHADOWSTYLE_MEDIA_CONTROLS_LOADING_CSS);
}

// static
String MediaControlsResourceLoader::GetJumpSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_JUMP_SVG);
}

// static
String MediaControlsResourceLoader::GetArrowRightSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_ARROW_RIGHT_SVG);
}

// static
String MediaControlsResourceLoader::GetArrowLeftSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_ARROW_LEFT_SVG);
}

// static
String MediaControlsResourceLoader::GetScrubbingMessageStyleSheet() {
#if defined(OHOS_VIDEO_ASSISTANT)
  if (custom_media_player_enabled_) {
    return UncompressResourceAsString(IDR_SHADOWSTYLE_MEDIA_CONTROLS_SCRUBBING_MESSAGE_HM_CSS);
  }
#endif
  return UncompressResourceAsString(
      IDR_SHADOWSTYLE_MEDIA_CONTROLS_SCRUBBING_MESSAGE_CSS);
}

// static
String MediaControlsResourceLoader::GetAnimatedArrowStyleSheet() {
  return UncompressResourceAsString(
      IDR_SHADOWSTYLE_MEDIA_CONTROLS_ANIMATED_ARROW_CSS);
}

// static
String MediaControlsResourceLoader::GetMediaInterstitialsStyleSheet() {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_INTERSTITIALS_CSS);
}

String MediaControlsResourceLoader::GetUAStyleSheet() {
#if BUILDFLAG(IS_OHOS) && defined(OHOS_MEDIA)
  // On ohos, custom video css styles only work for mobile devices.
  if (base::ohos::IsMobileDevice()) {
    return GetMediaControlsCSS() + GetMediaControlsAndroidCSS() +
           GetMediaControlsOHOSCSS() + GetMediaInterstitialsStyleSheet();
  }
#endif

  if (ShouldLoadAndroidCSS()) {
    return GetMediaControlsCSS() + GetMediaControlsAndroidCSS() +
           GetMediaInterstitialsStyleSheet();
  }
  return GetMediaControlsCSS() + GetMediaInterstitialsStyleSheet();
}

void MediaControlsResourceLoader::InjectMediaControlsUAStyleSheet() {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  std::unique_ptr<MediaControlsResourceLoader> loader =
      std::make_unique<MediaControlsResourceLoader>();

  if (!default_style_sheets.HasMediaControlsStyleSheetLoader())
    default_style_sheets.SetMediaControlsStyleSheetLoader(std::move(loader));
}

#ifdef OHOS_VIDEO_ASSISTANT

bool MediaControlsResourceLoader::custom_media_player_enabled_ = false;

void MediaControlsResourceLoader::SetCustomMediaPlayerEnabled(bool enable) {
  custom_media_player_enabled_ = enable;
}
#endif

}  // namespace blink
