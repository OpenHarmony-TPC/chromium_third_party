// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_button_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#ifdef OHOS_VIDEO_ASSISTANT
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ohos_nweb_ex/overrides/ui/strings/grit/hw_ui_strings.h"
#endif
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlPlaybackSpeedButtonElement::MediaControlPlaybackSpeedButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_SHOW_PLAYBACK_SPEED_MENU_BUTTON)));
  setType(input_type_names::kButton);
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-playback-speed-button"));
#ifdef OHOS_VIDEO_ASSISTANT
  if (GetMediaControls().ShouldShowVideoControlsHM()) {
    setAttribute(html_names::kAriaHiddenAttr, "true");
    setAttribute(html_names::kValueAttr, AtomicString(GetLocale().QueryString(
                 IDS_OHOS_MEDIA_VIDEO_PLAYBACK_SPEED_BUTTON)));
  }
#endif
}

#ifdef OHOS_VIDEO_ASSISTANT
static constexpr int playback_speed_rate_integerize = 100;
static constexpr int keep_one_decimal = 10;
void MediaControlPlaybackSpeedButtonElement::RefreshPlaybackSpeedButton() {
  double playback_rate = MediaElement().playbackRate();
  if (1.0 == playback_rate) {
    setAttribute(html_names::kValueAttr, AtomicString(GetLocale().QueryString(
                 IDS_OHOS_MEDIA_VIDEO_PLAYBACK_SPEED_BUTTON)));
  } else {
    std::string playback_rate_precised;
    if (0 == int(playback_rate * playback_speed_rate_integerize) % keep_one_decimal) {
      playback_rate_precised = base::StringPrintf("%.1f", playback_rate);
    } else {
      playback_rate_precised = base::StringPrintf("%.2f", playback_rate);
    }
    std::string format = l10n_util::GetStringUTF8(
        IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE);
    std::string content;
    base::StringAppendF(&content, format.c_str(), playback_rate_precised.c_str());
    setAttribute(html_names::kValueAttr, AtomicString(String::FromUTF8(content.c_str())));
  }
  setAttribute(html_names::kAriaHiddenAttr, "true");
}
#endif

bool MediaControlPlaybackSpeedButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

int MediaControlPlaybackSpeedButtonElement::GetOverflowStringId() const {
  return IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED;
}

bool MediaControlPlaybackSpeedButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlPlaybackSpeedButtonElement::GetNameForHistograms()
    const {
  return IsOverflowElement() ? "PlaybackSpeedOverflowButton"
                             : "PlaybackSpeedButton";
}

void MediaControlPlaybackSpeedButtonElement::DefaultEventHandler(Event& event) {
#ifdef OHOS_VIDEO_ASSISTANT
  if (GetMediaControls().ShouldShowVideoControlsHM()) {
    if ((event.type() == event_type_names::kClick ||
        event.type() == event_type_names::kGesturetap)
        && !GetMediaControls().PlaybackSpeedListIsWanted()) {
      GetMediaControls().TogglePlaybackSpeedList();
    }
  } else {
#endif
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    GetMediaControls().TogglePlaybackSpeedList();
  }
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
