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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_NATIVE_H_

#include "third_party/blink/renderer/core/layout/layout_image.h"

namespace blink {

class HTMLNativeElement;

class LayoutNative final : public LayoutImage {
 public:
  explicit LayoutNative(HTMLNativeElement*);
  ~LayoutNative() override;
  void Trace(Visitor*) const override;

  static LayoutSize DefaultSize();

  PhysicalRect ReplacedContentRectFrom(
      const LayoutSize size,
      const NGPhysicalBoxStrut& border_padding) const final;

  bool SupportsAcceleratedRendering() const;

  HTMLNativeElement* NativeElement() const;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNative";
  }

  void IntrinsicSizeChanged() override;

  OverflowClipAxes ComputeOverflowClipAxes() const final {
    NOT_DESTROYED();
    return kOverflowClipBothAxis;
  }

 private:
  void UpdateFromElement() override;

  LayoutSize CalculateIntrinsicSize(float scale);
  void UpdateIntrinsicSize(bool is_in_layout);

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final;

  bool BackgroundShouldAlwaysBeClipped() const final {
    NOT_DESTROYED();
    return false;
  }

  bool IsImage() const final {
    NOT_DESTROYED();
    return false;
  }

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectVideo || type == kLayoutObjectMedia ||
           LayoutImage::IsOfType(type);
  }

  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

  void UpdateLayout() override;

  bool CanHaveAdditionalCompositingReasons() const override {
    NOT_DESTROYED();
    return true;
  }
  CompositingReasons AdditionalCompositingReasons() const override;

  void UpdatePlayer(bool is_in_layout);

  LayoutSize cached_image_size_;
};

template <>
struct DowncastTraits<LayoutNative> {
  static bool AllowFrom(const LayoutObject& object) { return object.IsVideo(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_NATIVE_H_