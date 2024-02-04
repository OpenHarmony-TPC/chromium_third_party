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

#include "third_party/blink/renderer/core/layout/layout_native.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_native_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"
#include "third_party/blink/renderer/core/paint/native_painter.h"

namespace blink {

namespace {

const float kInitEffectZoom = 1.0f;

}  // namespace

LayoutNative::LayoutNative(HTMLNativeElement* native) : LayoutImage(native) {
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

void LayoutNative::IntrinsicSizeChanged() {
  NOT_DESTROYED();
  UpdateIntrinsicSize(/* is_in_layout */ false);
}

void LayoutNative::UpdateIntrinsicSize(bool is_in_layout) {
  NOT_DESTROYED();
  LayoutSize size = CalculateIntrinsicSize(StyleRef().EffectiveZoom());

  // Never set the element size to zero when in a media document.
  if (size.IsEmpty() && GetNode()->ownerDocument() &&
      GetNode()->ownerDocument()->IsMediaDocument())
    return;

  if (auto* layout_view = View()) {
    size = LayoutSize(layout_view->GetLayoutSize());
  }

  if (size == IntrinsicSize())
    return;

  SetIntrinsicSize(size);
  SetIntrinsicLogicalWidthsDirty();
  if (!is_in_layout) {
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSizeChanged);
  }

  NativeElement()->SizeChanged(ToRoundedSize(size));
}

LayoutSize LayoutNative::CalculateIntrinsicSize(float scale) {
  NOT_DESTROYED();

  if (const auto* content = NativeElement()->GetWebNativeBridge()) {
    auto size = content->NaturalSize();
    if (!size.IsEmpty()) {
      LayoutSize layout_size = LayoutSize(size);
      layout_size.Scale(scale);
      return layout_size;
    }
  }

  LayoutSize size = DefaultSize();
  size.Scale(scale);
  return size;
}

void LayoutNative::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutImage::ImageChanged(new_image, defer);

  // Cache the image intrinsic size so we can continue to use it to draw the
  // image correctly even if we know the video intrinsic size but aren't able to
  // draw video frames yet (we don't want to scale the poster to the video size
  // without keeping aspect ratio). We do not need to check
  // |ShouldDisplayPosterImage| because the image can be ready before we find
  // out we actually need it.
  cached_image_size_ = IntrinsicSize();

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  UpdateIntrinsicSize(/* is_in_layout */ false);
}

bool LayoutNative::IsChildAllowed(LayoutObject* child,
                                 const ComputedStyle& style) const {
  NOT_DESTROYED();
  // Two types of child layout objects are allowed: media controls
  // and the text track container. Filter children by node type.
  DCHECK(child->GetNode());

  // Out-of-flow positioned or floating child breaks layout hierarchy.
  if (style.HasOutOfFlowPosition() ||
      (style.IsFloating() && !style.IsInsideDisplayIgnoringFloatingChildren()))
    return false;

  return false;
}

void LayoutNative::PaintReplaced(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  NativePainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutNative::UpdateLayout() {
  NOT_DESTROYED();
  UpdatePlayer(/* is_in_layout */ true);

  LayoutImage::UpdateLayout();
}

HTMLNativeElement* LayoutNative::NativeElement() const {
  NOT_DESTROYED();
  return To<HTMLNativeElement>(GetNode());
}

void LayoutNative::UpdateFromElement() {
  NOT_DESTROYED();
  LayoutImage::UpdateFromElement();
  UpdatePlayer(/* is_in_layout */ false);

  SetShouldDoFullPaintInvalidation();
}

void LayoutNative::UpdatePlayer(bool is_in_layout) {
  NOT_DESTROYED();
  UpdateIntrinsicSize(is_in_layout);

  auto* native_bridge = NativeElement()->GetWebNativeBridge();
  if (!native_bridge)
    return;

  if (!NativeElement()->InActiveDocument())
    return;

  NativeElement()->SetNeedsCompositingUpdate();
}

PhysicalRect LayoutNative::ReplacedContentRectFrom(
    const LayoutSize size,
    const NGPhysicalBoxStrut& border_padding) const {
  NOT_DESTROYED();

  // If we are displaying the poster image no pre-rounding is needed, but the
  // size of the image should be used for fitting instead.
  return ComputeReplacedContentRect(size, border_padding, &cached_image_size_);
}


bool LayoutNative::SupportsAcceleratedRendering() const {
  NOT_DESTROYED();
  return !!NativeElement()->CcLayer();
}

CompositingReasons LayoutNative::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  return CompositingReason::kVideo;
}

}  // namespace blink