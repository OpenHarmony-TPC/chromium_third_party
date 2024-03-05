// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_entered_fullscreen_panel_element.h"

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlEnteredFullscreenPanelElement::
    MediaControlEnteredFullscreenPanelElement(MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-entered-fullscreen-panel"));
}

void MediaControlEnteredFullscreenPanelElement::SetIsMask(bool is_mask) {
  SetClass("mask", is_mask);
}

}  // namespace blink
