/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_

#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_defaults.h"
#include "third_party/blink/renderer/core/editing/selection_strategy.h"
#include "third_party/blink/renderer/core/frame/settings_delegate.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/parser/parser_scripting_flag_policy.h"
#include "third_party/blink/renderer/core/html/track/text_track_kind_user_preference.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/settings_macros.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
#include "third_party/blink/renderer/platform/graphics/lcd_text_preference.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/gfx/geometry/size.h"
#if BUILDFLAG(IS_OHOS)
#include "third_party/blink/public/platform/web_string.h"
#endif

namespace blink {

class CORE_EXPORT Settings {
  USING_FAST_MALLOC(Settings);

 public:
  Settings();

  // Default copy and assignment are forbidden because SettingsDelegate only
  // supports 1:1 relationship with Settings.
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;

  GenericFontFamilySettings& GetGenericFontFamilySettings() {
    return generic_font_family_settings_;
  }
  void NotifyGenericFontFamilyChange() {
    Invalidate(SettingsDelegate::ChangeType::kFontFamily);
  }

  SETTINGS_GETTERS_AND_SETTERS

  void SetPreferCompositingToLCDTextForTesting(bool enabled);

#if defined(OHOS_INPUT_EVENTS)
  void SetVerticalHideScrollbars(bool hide_vertical_scrollbars) {
    hide_vertical_scrollbars_ = hide_vertical_scrollbars;
  }
  void SetHorizontalHideScrollbars(bool hide_horizontal_scrollbars) {
    hide_horizontal_scrollbars_ = hide_horizontal_scrollbars;
  }
  bool GetVerticalHideScrollbars() { return hide_vertical_scrollbars_; }
  bool GetHorizontalHideScrollbars() { return hide_horizontal_scrollbars_; }
  void SetOverscrollMode(int overscrollMode) {
    overscroll_mode_ = overscrollMode;
  }
  int GetOverscrollMode() {
    return overscroll_mode_;
  }
  void SetScrollable(bool enabled) {
    scroll_enabled_ = enabled;
  }
  bool GetScrollable() {
    return scroll_enabled_;
  }
#endif  // defined(OHOS_INPUT_EVENTS)

#ifdef OHOS_SCROLLBAR
  void SetScrollBarColor(uint32_t colorValue) { scrollbar_color_ = colorValue; }

  uint32_t GetScrollBarColor() { return scrollbar_color_; }
#endif  // OHOS_SCROLLBAR

#if defined(OHOS_EX_FREE_COPY)
  void SetContextMenuCustomization(bool contextmenu_customization_enabled) {
    contextmenu_customization_enabled_ = contextmenu_customization_enabled;
  }

  bool IsContextMenuCustomizationEnabled() {
    return contextmenu_customization_enabled_;
  }
#endif

#if BUILDFLAG(IS_OHOS)
  void RegisterNativeEmbedRule(const WebString& tag, const WebString& type) {
    embed_rule_[tag] = type;
  }

  std::map<WebString, WebString> NativeEmbedRule() {
    return embed_rule_;
  }
#endif

#ifdef OHOS_EX_BLANK_TARGET_POPUP_INTERCEPT
  void EnableBlankTargetPopupIntercept(bool enabled) {
    blank_target_popup_intercept_enabled_ = enabled;
  }

  bool IsBlankTargetPopupInterceptEnabled() const {
    return blank_target_popup_intercept_enabled_;
  }
#endif

  void SetDelegate(SettingsDelegate*);

#if defined(OHOS_CLIPBOARD)
  void SetCopyOption(blink::mojom::CopyOptionMode copyOption) {
    copy_option_ = copyOption;
  }

  mojom::CopyOptionMode GetCopyOption() {
    return copy_option_;
  }
#endif // defined(OHOS_CLIPBOARD)

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
void SetCustomVideoPlayerEnabled(bool enable) {
  custom_video_player_enabled_ = enable;
}
bool IsCustomVideoPlayerEnabled() const {
  return custom_video_player_enabled_;
}
void SetCustomVideoPlayerOverlay(bool overlay) {
  custom_video_player_overlay_ = overlay;
}
bool IsCustomVideoPlayerOverlay() const {
  return custom_video_player_overlay_;
}
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#if defined(OHOS_MEDIA_CAPABILITIES_ENHANCE)
void SetUsageScenario(int32_t usage_scenario) {
  usage_scenario_ = usage_scenario;
}

int32_t GetUsageScenario() const {
  return usage_scenario_;
}
#endif // OHOS_MEDIA_CAPABILITIES_ENHANCE

#ifdef OHOS_VIDEO_ASSISTANT
  void SetVideoAssistantEnabled(bool enabled) {
    video_assistant_enabled_ = enabled;
  }
  bool GetVideoAssistantEnabled() {
    return video_assistant_enabled_;
  }
  void SetCustomMediaPlayerEnabled(bool enabled) {
    custom_media_player_enabled_ = enabled;
  }
  bool GetCustomMediaPlayerEnabled() {
    return custom_media_player_enabled_;
  }
#endif // OHOS_VIDEO_ASSISTANT

 private:
  void Invalidate(SettingsDelegate::ChangeType);

  SettingsDelegate* delegate_;

  GenericFontFamilySettings generic_font_family_settings_;

  SETTINGS_MEMBER_VARIABLES

#if defined(OHOS_INPUT_EVENTS)
  bool hide_vertical_scrollbars_ = true;
  bool hide_horizontal_scrollbars_ = true;
  int overscroll_mode_ = 0;
  bool scroll_enabled_ = true;
#endif  // defined(OHOS_INPUT_EVENTS)

#ifdef OHOS_SCROLLBAR
  uint32_t scrollbar_color_ = 0;
#endif  // OHOS_SCROLLBAR

#if defined(OHOS_EX_FREE_COPY)
  bool contextmenu_customization_enabled_ = false;
#endif  // OHOS_EX_FREE_COPY

#ifdef OHOS_EX_BLANK_TARGET_POPUP_INTERCEPT
  bool blank_target_popup_intercept_enabled_ = true;
#endif

#if BUILDFLAG(IS_OHOS)
  std::map<WebString, WebString> embed_rule_;
#endif

#if defined(OHOS_CLIPBOARD)
  mojom::CopyOptionMode copy_option_ = mojom::CopyOptionMode::CROSS_DEVICE;
#endif // defined(OHOS_CLIPBOARD)

#if defined(OHOS_CUSTOM_VIDEO_PLAYER)
  bool custom_video_player_enabled_ = false;
  bool custom_video_player_overlay_ = false;
#endif // OHOS_CUSTOM_VIDEO_PLAYER

#if defined(OHOS_MEDIA_CAPABILITIES_ENHANCE)
  int32_t usage_scenario_ = 1;
#endif // OHOS_MEDIA_CAPABILITIES_ENHANCE

#ifdef OHOS_VIDEO_ASSISTANT
  bool video_assistant_enabled_ = false;
  bool custom_media_player_enabled_ = false;
#endif // OHOS_VIDEO_ASSISTANT
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_
