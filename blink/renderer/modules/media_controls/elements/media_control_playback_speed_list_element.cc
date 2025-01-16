// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_list_element.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_scrollintoviewoptions.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#ifdef OHOS_VIDEO_ASSISTANT
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "ui/base/l10n/l10n_util.h"
#include "ohos_nweb_ex/overrides/ui/strings/grit/hw_ui_strings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#endif
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

namespace {

// This enum is used to record histograms. Do not reorder.
enum class MediaControlsPlaybackSpeed {
  k0_25X = 0,
  k0_5X,
  k0_75X,
  k1X,
  k1_25X,
  k1_5X,
  k1_75X,
  k2X,
#ifdef OHOS_VIDEO_ASSISTANT
  k3X,
  kMaxValue = k3X,
#else
  kMaxValue = k2X,
#endif
};

void RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed playback_speed) {
  base::UmaHistogramEnumeration("Media.Controls.PlaybackSpeed", playback_speed);
}

struct PlaybackSpeed {
  const int display_name;
  const double playback_rate;
};

#ifdef OHOS_VIDEO_ASSISTANT
static const PlaybackSpeed kPlaybackSpeedsOhos[] = {
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 0.5},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 0.75},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 1.0},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 1.25},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 1.5},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 2.0},
    {IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE, 3.0}};
#endif

static const PlaybackSpeed kPlaybackSpeeds[] = {
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_0_25X_TITLE, 0.25},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_0_5X_TITLE, 0.5},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_0_75X_TITLE, 0.75},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_NORMAL_TITLE, 1.0},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_1_25X_TITLE, 1.25},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_1_5X_TITLE, 1.5},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_1_75X_TITLE, 1.75},
    {IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_2X_TITLE, 2.0}};

const QualifiedName& PlaybackRateAttrName() {
  // Save the playback rate in an attribute.
  DEFINE_STATIC_LOCAL(QualifiedName, playback_rate_attr,
                      (g_null_atom, "data-playback-rate", g_null_atom));
  return playback_rate_attr;
}

}  // anonymous namespace

class MediaControlPlaybackSpeedListElement::RequestAnimationFrameCallback final
    : public FrameCallback {
 public:
  explicit RequestAnimationFrameCallback(
      MediaControlPlaybackSpeedListElement* list)
      : list_(list) {}

  RequestAnimationFrameCallback(const RequestAnimationFrameCallback&) = delete;
  RequestAnimationFrameCallback& operator=(
      const RequestAnimationFrameCallback&) = delete;

  void Invoke(double) override { list_->CenterCheckedItem(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(list_);
    FrameCallback::Trace(visitor);
  }

 private:
  Member<MediaControlPlaybackSpeedListElement> list_;
};

MediaControlPlaybackSpeedListElement::MediaControlPlaybackSpeedListElement(
    MediaControlsImpl& media_controls)
    : MediaControlPopupMenuElement(media_controls) {
  setAttribute(html_names::kRoleAttr, "menu");
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(
                   IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_SUBMENU_TITLE)));
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-playback-speed-list"));
}

bool MediaControlPlaybackSpeedListElement::WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlPlaybackSpeedListElement::SetIsWanted(bool wanted) {
  if (wanted)
    RefreshPlaybackSpeedListMenu();

#ifdef OHOS_VIDEO_ASSISTANT
  if (!GetMediaControls().ShouldShowVideoControlsHM()) {
#endif
  if (!wanted && !GetMediaControls().OverflowMenuIsWanted())
    GetMediaControls().CloseOverflowMenu();
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif

  MediaControlPopupMenuElement::SetIsWanted(wanted);
}

void MediaControlPlaybackSpeedListElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kClick) {
    // This handles the back button click. Clicking on a menu item triggers the
    // change event instead.
#ifdef OHOS_VIDEO_ASSISTANT
    if (!GetMediaControls().ShouldShowVideoControlsHM()) {
#endif
    GetMediaControls().ToggleOverflowMenu();
#ifdef OHOS_VIDEO_ASSISTANT
    }
#endif
    event.SetDefaultHandled();
  } else if (event.type() == event_type_names::kChange) {
    // Identify which input element was selected and update playback speed
    Node* target = event.target()->ToNode();
    if (!target || !target->IsElementNode())
      return;

    double playback_rate =
        To<Element>(target)->GetFloatingPointAttribute(PlaybackRateAttrName());
    MediaElement().setDefaultPlaybackRate(playback_rate);
    MediaElement().setPlaybackRate(playback_rate);
#ifdef OHOS_VIDEO_ASSISTANT
    if (GetMediaControls().ShouldShowVideoControlsHM()) {
      GetMediaControls().RefreshPlaybackSpeedButton();
    }
#endif

    RecordPlaybackSpeed(playback_rate);

    // Close the playback speed list.
    SetIsWanted(false);
    event.SetDefaultHandled();
  }
  MediaControlPopupMenuElement::DefaultEventHandler(event);
}

void MediaControlPlaybackSpeedListElement::RecordPlaybackSpeed(const double playback_rate) {
  static constexpr double playback_speed_rate = 3.0;
  if (playback_rate == 0.25) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k0_25X);
  } else if (playback_rate == 0.5) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k0_5X);
  } else if (playback_rate == 0.75) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k0_75X);
  } else if (playback_rate == 1.0) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k1X);
  } else if (playback_rate == 1.25) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k1_25X);
  } else if (playback_rate == 1.5) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k1_5X);
  } else if (playback_rate == 1.75) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k1_75X);
  } else if (playback_rate == 2.0) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k2X);
#ifdef OHOS_VIDEO_ASSISTANT
  } else if (playback_rate == playback_speed_rate) {
    RecordPlaybackSpeedUMA(MediaControlsPlaybackSpeed::k3X);
#endif
  } else {
    NOTREACHED();
  }
}

Element* MediaControlPlaybackSpeedListElement::CreatePlaybackSpeedListItem(
    const int display_name,
    const double playback_rate) {
  auto* playback_speed_item =
      MakeGarbageCollected<HTMLLabelElement>(GetDocument());
  playback_speed_item->SetShadowPseudoId(
      AtomicString("-internal-media-controls-playback-speed-list-item"));
  auto* playback_speed_item_input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags());
  playback_speed_item_input->SetShadowPseudoId(
      AtomicString("-internal-media-controls-playback-speed-list-item-input"));
  playback_speed_item_input->setAttribute(html_names::kAriaHiddenAttr, "true");
  playback_speed_item_input->setType(input_type_names::kCheckbox);
  playback_speed_item_input->SetFloatingPointAttribute(PlaybackRateAttrName(),
                                                       playback_rate);
  if (playback_rate == MediaElement().playbackRate()) {
    playback_speed_item_input->SetChecked(true);
    playback_speed_item->setAttribute(html_names::kAriaCheckedAttr, "true");
    checked_item_ = playback_speed_item;
  }
  // Allows to focus the list entry instead of the button.
  playback_speed_item->setTabIndex(0);
  playback_speed_item_input->setTabIndex(-1);

  // Set playback speed label into an aria-hidden span so that aria will not
  // repeat the contents twice.
  String playback_speed_label;
#ifdef OHOS_VIDEO_ASSISTANT
  String playback_speed_label_hm;
  if (GetMediaControls().ShouldShowVideoControlsHM()) {
    playback_speed_label_hm = CreatePlaybackSpeedLabelContentHM(playback_rate);
  } else {
#endif
  playback_speed_label = GetLocale().QueryString(display_name);
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif

  auto* playback_speed_label_span =
      MakeGarbageCollected<HTMLSpanElement>(GetDocument());
#ifdef OHOS_VIDEO_ASSISTANT
  if (GetMediaControls().ShouldShowVideoControlsHM()) {
    playback_speed_label_span->setInnerText(playback_speed_label_hm);
    playback_speed_item->setAttribute(html_names::kAriaLabelAttr,
                                      WTF::AtomicString(playback_speed_label_hm));
  } else {
#endif
  playback_speed_label_span->setInnerText(playback_speed_label);
  playback_speed_item->setAttribute(html_names::kAriaLabelAttr,
                                    WTF::AtomicString(playback_speed_label));
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif
  playback_speed_label_span->setAttribute(html_names::kAriaHiddenAttr, "true");
  playback_speed_item->ParserAppendChild(playback_speed_label_span);
  playback_speed_item->ParserAppendChild(playback_speed_item_input);

  return playback_speed_item;
}

#ifdef OHOS_VIDEO_ASSISTANT
String MediaControlPlaybackSpeedListElement::CreatePlaybackSpeedLabelContentHM(const double playback_rate) {
  static constexpr int playback_speed_rate_integerize = 100;
  static constexpr int keep_one_decimal = 10;
  std::string format = l10n_util::GetStringUTF8(
      IDS_HW_MEDIA_VIDEO_PLAYBACK_SPEED_VALUE);
  std::string rate_item_content;
  std::string playback_rate_precised;
  if (0 == int(playback_rate * playback_speed_rate_integerize) % keep_one_decimal) {
    playback_rate_precised = base::StringPrintf("%.1f", playback_rate);
  } else {
    playback_rate_precised = base::StringPrintf("%.2f", playback_rate);
  }
  base::StringAppendF(&rate_item_content, format.c_str(), playback_rate_precised.c_str());
  return String(rate_item_content);
}
#endif

Element* MediaControlPlaybackSpeedListElement::CreatePlaybackSpeedHeaderItem() {
  auto* header_item = MakeGarbageCollected<HTMLLabelElement>(GetDocument());
  header_item->SetShadowPseudoId(
      "-internal-media-controls-playback-speed-list-header");
  header_item->ParserAppendChild(
      Text::Create(GetDocument(),
                   GetLocale().QueryString(
                       IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED_SUBMENU_TITLE)));
  header_item->setAttribute(html_names::kRoleAttr, "button");
  header_item->setAttribute(html_names::kAriaLabelAttr,
                            AtomicString(GetLocale().QueryString(
                                IDS_AX_MEDIA_BACK_TO_OPTIONS_BUTTON)));
  header_item->setTabIndex(0);
  return header_item;
}

void MediaControlPlaybackSpeedListElement::RefreshPlaybackSpeedListMenu() {
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
  RemoveChildren(kOmitSubtreeModifiedEvent);

#ifdef OHOS_VIDEO_ASSISTANT
  if (!GetMediaControls().ShouldShowVideoControlsHM()) {
#endif
  ParserAppendChild(CreatePlaybackSpeedHeaderItem());
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif

  checked_item_ = nullptr;

  const PlaybackSpeed* playbackSpeedsRef = nullptr;
  unsigned playbackSpeedsSize;
#ifdef OHOS_VIDEO_ASSISTANT
  if (GetMediaControls().ShouldShowVideoControlsHM()) {
    playbackSpeedsRef = kPlaybackSpeedsOhos;
    playbackSpeedsSize = std::size(kPlaybackSpeedsOhos);
  } else {
#endif
  playbackSpeedsRef = kPlaybackSpeeds;
  playbackSpeedsSize = std::size(kPlaybackSpeeds);
#ifdef OHOS_VIDEO_ASSISTANT
  }
#endif
  // Construct a menu for playback speeds.
  for (unsigned i = 0; i < playbackSpeedsSize; i++) {
    auto& playback_speed = playbackSpeedsRef[i];
    auto* playback_speed_item = CreatePlaybackSpeedListItem(
        playback_speed.display_name, playback_speed.playback_rate);
    playback_speed_item->setAttribute(
        html_names::kAriaSetsizeAttr,
        WTF::AtomicString::Number(playbackSpeedsSize));
    playback_speed_item->setAttribute(html_names::kAriaPosinsetAttr,
                                      WTF::AtomicString::Number(i + 1));
    playback_speed_item->setAttribute(html_names::kRoleAttr,
                                      "menuitemcheckbox");
    ParserAppendChild(playback_speed_item);
#ifdef OHOS_VIDEO_ASSISTANT
    if (GetMediaControls().ShouldShowVideoControlsHM()) {
      if (i == playbackSpeedsSize - 1) {
        break;
      }
      Element* split_line_item = MakeGarbageCollected<HTMLHRElement>(GetDocument());
      ParserAppendChild(split_line_item);
    }
#endif
  }
  RequestAnimationFrameCallback* callback =
      MakeGarbageCollected<RequestAnimationFrameCallback>(this);
  GetDocument().RequestAnimationFrame(callback);
}

void MediaControlPlaybackSpeedListElement::CenterCheckedItem() {
  if (!checked_item_)
    return;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("center");
  auto* arg =
      MakeGarbageCollected<V8UnionBooleanOrScrollIntoViewOptions>(options);
  checked_item_->scrollIntoView(arg);
  checked_item_->Focus();
}

void MediaControlPlaybackSpeedListElement::Trace(Visitor* visitor) const {
  visitor->Trace(checked_item_);
  MediaControlPopupMenuElement::Trace(visitor);
}

}  // namespace blink
