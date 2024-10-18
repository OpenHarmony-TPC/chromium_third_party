// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#ifdef OHOS_DISPLAY_CUTOUT
#include "third_party/blink/renderer/core/page/page.h"
#include "ui/gfx/geometry/insets.h"
#endif

namespace blink {

DisplayCutoutClientImpl::DisplayCutoutClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver)
    : frame_(frame), receiver_(this, std::move(receiver)) {}

void DisplayCutoutClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>
        receiver) {
  if (!frame)
    return;
  MakeGarbageCollected<DisplayCutoutClientImpl>(frame, std::move(receiver));
}

#ifdef OHOS_DISPLAY_CUTOUT
void DisplayCutoutClientImpl::UpdateSafeArea(LocalFrame* frame) {
  if (!frame || !frame->GetDocument() || !frame->GetPage()) return;

  DocumentStyleEnvironmentVariables& vars =
      frame->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
  gfx::Insets safe_area = frame->GetPage()->SafeAreaScaled();
  LOG(DEBUG) << __func__ << " " << safe_area.ToString();;

  vars.SetVariable(UADefinedVariable::kSafeAreaInsetTop,
                   StyleEnvironmentVariables::FormatPx(safe_area.top()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetLeft,
                   StyleEnvironmentVariables::FormatPx(safe_area.left()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetBottom,
                   StyleEnvironmentVariables::FormatPx(safe_area.bottom()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetRight,
                   StyleEnvironmentVariables::FormatPx(safe_area.right()));
}
#endif

void DisplayCutoutClientImpl::SetSafeArea(const gfx::Insets& safe_area) {
#ifdef OHOS_DISPLAY_CUTOUT
  if (frame_->GetPage()) {
    frame_->GetPage()->SetSafeArea(safe_area);
  }
  DisplayCutoutClientImpl::UpdateSafeArea(frame_);
#else
  DocumentStyleEnvironmentVariables& vars =
      frame_->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();

  vars.SetVariable(UADefinedVariable::kSafeAreaInsetTop,
                   StyleEnvironmentVariables::FormatPx(safe_area.top()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetLeft,
                   StyleEnvironmentVariables::FormatPx(safe_area.left()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetBottom,
                   StyleEnvironmentVariables::FormatPx(safe_area.bottom()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetRight,
                   StyleEnvironmentVariables::FormatPx(safe_area.right()));
#endif
}

void DisplayCutoutClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
