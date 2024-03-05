// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"

#if defined(OHOS_MEDIA)
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#endif // defined(OHOS_MEDIA)
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

void MediaControlDivElement::SetOverflowElementIsWanted(bool) {}

void MediaControlDivElement::MaybeRecordDisplayed() {
  // No-op. At the moment, usage is only recorded in the context of CTR. It
  // could be recorded for MediaControlDivElement but there is no need for it at
  // the moment.
}

MediaControlDivElement::MediaControlDivElement(
    MediaControlsImpl& media_controls)
    : HTMLDivElement(media_controls.GetDocument()),
      MediaControlElementBase(media_controls, this) {}

bool MediaControlDivElement::IsMediaControlElement() const {
  return true;
}

gfx::Size MediaControlDivElement::GetSizeOrDefault() const {
  return MediaControlElementsHelper::GetSizeOrDefault(*this, gfx::Size());
}

bool MediaControlDivElement::IsDisabled() const {
  // Div elements cannot be disabled.
  return false;
}

#if defined(OHOS_MEDIA)
void MediaControlDivElement::SetClass(const AtomicString& class_name,
                                      bool should_have_class) {
  if (should_have_class) {
    classList().Add(class_name);
  } else {
    classList().Remove(class_name);
  }
}
#endif // defined(OHOS_MEDIA)

void MediaControlDivElement::Trace(Visitor* visitor) const {
  HTMLDivElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
}

}  // namespace blink
