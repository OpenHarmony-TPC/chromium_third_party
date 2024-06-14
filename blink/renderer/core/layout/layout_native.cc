/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/layout_native.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/native_painter.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"

namespace blink {

namespace {

const float kInitEffectZoom = 1.0f;

}  // namespace

LayoutNative::LayoutNative(Element* native) : LayoutImage(native) {
  SetImageResource(MakeGarbageCollected<LayoutImageResource>());
  SetIntrinsicSize(CalculateIntrinsicSize(kInitEffectZoom));
}

LayoutNative::~LayoutNative() = default;

void LayoutNative::Trace(Visitor* visitor) const {
  LayoutImage::Trace(visitor);
}

LayoutSize LayoutNative::DefaultSize() {
  return LayoutSize(kDefaultWidth, kDefaultHeight);
}

void LayoutNative::UpdateIntrinsicSize(bool is_in_layout) {
  NOT_DESTROYED();
  LayoutSize size = CalculateIntrinsicSize(StyleRef().EffectiveZoom());
  // Never set the element size to zero when in a media document.
  if (size.IsEmpty() && GetNode()->ownerDocument() &&
      GetNode()->ownerDocument()->IsMediaDocument()) {
    return;
  }

  if (auto* layout_view = View()) {
    size = LayoutSize(layout_view->GetLayoutSize());
  }

  if (size == IntrinsicSize()) {
    return;
  }

  SetIntrinsicSize(size);
  SetIntrinsicLogicalWidthsDirty();
  if (!is_in_layout) {
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSizeChanged);
  }

  if (!NativeElement() || !NativeElement()->NativeLoader()) {
    return;
  }
  NativeElement()->NativeLoader()->UpdateSize(gfx::Size(size.Width().ToInt(),
                                         size.Height().ToInt()));
}

LayoutSize LayoutNative::CalculateIntrinsicSize(float scale) {
  NOT_DESTROYED();

  LayoutSize size = DefaultSize();
  size.Scale(scale);

  if (!NativeElement() || !NativeElement()->NativeLoader()) {
    return size;
  }

  if (const auto* content =
          NativeElement()->NativeLoader()->GetWebNativeBridge()) {
    gfx::Size nature_size = content->NaturalSize();
    if (!nature_size.IsEmpty()) {
      LayoutSize layout_size = LayoutSize(nature_size);
      layout_size.Scale(scale);
      return layout_size;
    }
  }

  return size;
}

void LayoutNative::ImageChanged(WrappedImagePtr new_image,
                                CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutImage::ImageChanged(new_image, defer);

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  UpdateIntrinsicSize(/* is_in_layout */ false);
}

void LayoutNative::PaintReplaced(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  NativePainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutNative::UpdateLayout() {
  NOT_DESTROYED();
  UpdateNativeContent(/* is_in_layout */ true);

  LayoutImage::UpdateLayout();
}

HTMLPlugInElement* LayoutNative::NativeElement() const {
  NOT_DESTROYED();
  return To<HTMLPlugInElement>(GetNode());
}

void LayoutNative::UpdateFromElement() {
  NOT_DESTROYED();
  LayoutImage::UpdateFromElement();
  UpdateNativeContent(/* is_in_layout */ false);

  SetShouldDoFullPaintInvalidation();
}

void LayoutNative::UpdateNativeContent(bool is_in_layout) {
  NOT_DESTROYED();
  UpdateIntrinsicSize(is_in_layout);

  if (!NativeElement() || !NativeElement()->NativeLoader()) {
    return;
  }

  auto* native_bridge = NativeElement()->NativeLoader()->GetWebNativeBridge();
  if (!native_bridge) {
    return;
  }

  if (!NativeElement()->InActiveDocument()) {
    return;
  }

  NativeElement()->SetNeedsCompositingUpdate();
}

PhysicalRect LayoutNative::ReplacedContentRectFrom(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding) const {
  NOT_DESTROYED();

  return PreSnappedRectForPersistentSizing(
      ComputeReplacedContentRect(size, border_padding));
}

bool LayoutNative::SupportsAcceleratedRendering() const {
  NOT_DESTROYED();
  if (!NativeElement() || !NativeElement()->NativeLoader()) {
    return false;
  }
  return !!NativeElement()->NativeLoader()->CcLayer();
}

CompositingReasons LayoutNative::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  return CompositingReason::kVideo;
}

}  // namespace blink