// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_row_panel_element.h"

namespace blink {

MediaControlTimelineRowPanelElement::MediaControlTimelineRowPanelElement(
  MediaControlsImpl& media_controls) :  MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-timeline-row-panel"));
}

}