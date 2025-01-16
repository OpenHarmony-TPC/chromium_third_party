// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_scrubbing_message_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#ifdef OHOS_VIDEO_ASSISTANT
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#endif
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlScrubbingMessageElement::MediaControlScrubbingMessageElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-scrubbing-message"));
  CreateUserAgentShadowRoot();
  SetIsWanted(false);
#ifdef OHOS_VIDEO_ASSISTANT
  SetMsgSpan();
#endif
}

void MediaControlScrubbingMessageElement::PopulateChildren() {
  ShadowRoot* shadow_root = GetShadowRoot();

  // This stylesheet element will contain rules that are specific to the
  // scrubbing message. The shadow DOM protects these rules from bleeding
  // across to the parent DOM.
  auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                       CreateElementFlags());
  style->setTextContent(
      MediaControlsResourceLoader::GetScrubbingMessageStyleSheet());
  shadow_root->ParserAppendChild(style);

  HTMLDivElement* arrow_left_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left1", shadow_root);
  HTMLDivElement* arrow_left_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left2", shadow_root);
  HTMLDivElement* message_div =
      MediaControlElementsHelper::CreateDivWithId("message", shadow_root);
  HTMLDivElement* arrow_right_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right1", shadow_root);
  HTMLDivElement* arrow_right_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right2", shadow_root);

  arrow_left_div1->setInnerHTML(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  arrow_left_div2->setInnerHTML(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  message_div->setInnerText(
      MediaElement().GetLocale().QueryString(IDS_MEDIA_SCRUBBING_MESSAGE_TEXT));
  arrow_right_div1->setInnerHTML(
      MediaControlsResourceLoader::GetArrowRightSVGImage());
  arrow_right_div2->setInnerHTML(
      MediaControlsResourceLoader::GetArrowRightSVGImage());
}

void MediaControlScrubbingMessageElement::SetIsWanted(bool wanted) {
  // Populate the DOM on demand.
  if (wanted && !GetShadowRoot()->firstChild())
    PopulateChildren();

  MediaControlDivElement::SetIsWanted(wanted);
}

#ifdef OHOS_VIDEO_ASSISTANT

void MediaControlScrubbingMessageElement::SetMsgSpan() {
  ShadowRoot* shadow_root = GetShadowRoot();
  auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument(), CreateElementFlags());
  style->setTextContent(MediaControlsResourceLoader::GetScrubbingMessageStyleSheet());
  shadow_root->ParserAppendChild(style);
  current_time_span_ = MediaControlElementsHelper::CreateSpanWithId("messageCurrentTime", shadow_root);
  duration_time_span_ = MediaControlElementsHelper::CreateSpanWithId("messageDurationTime", shadow_root);
  duration_progress_ = MediaControlElementsHelper::CreateProgressWithId("durationProgress", shadow_root);
}

void MediaControlScrubbingMessageElement::updateScrubbingMsg(bool wanted) {
  if (wanted) {
    const double current = MediaElement().currentTime();
    const double duration = MediaElement().duration();
    String formatted_current_time;
    String formatted_duration_time;
    if (GetMediaControls().ShouldShowVideoControlsHM()) {
      formatted_current_time = MediaControlsSharedHelpers::FormatTimeHM(current);
      formatted_duration_time = MediaControlsSharedHelpers::FormatTimeHM(duration);
    } else {
      formatted_current_time = MediaControlsSharedHelpers::FormatTime(current);
      formatted_duration_time = MediaControlsSharedHelpers::FormatTime(duration);
    }
    formatted_duration_time = "/" + formatted_duration_time;

    current_time_span_->setInnerText(formatted_current_time);
    duration_time_span_->setInnerText(formatted_duration_time);
    duration_progress_->setMax(duration);
    duration_progress_->setValue(current);
  } else {
    current_time_span_->setInnerText("");
    duration_time_span_->setInnerText("");
  }

  MediaControlDivElement::SetIsWanted(wanted);
}
#endif

}  // namespace blink
