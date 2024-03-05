// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ENTERED_FULLSCREEN_TITLE_DISPLAY_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ENTERED_FULLSCREEN_TITLE_DISPLAY_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"

namespace blink {

class MediaControlsImpl;

class MediaControlEnteredFullscreenTitleDisplayElement final
    : public MediaControlDivElement {
 public:
  explicit MediaControlEnteredFullscreenTitleDisplayElement(MediaControlsImpl&);

  void UpdateTitle();
  void Trace(blink::Visitor*) const override;

 private:
  // The title text representation in a div.
  Member<HTMLDivElement> title_span_;
  void SetTitle(const String& title);
  String title_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ENTERED_FULLSCREEN_TITLE_DISPLAY_ELEMENT_H_
