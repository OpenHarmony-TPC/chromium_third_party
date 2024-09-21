/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <memory>
#include <new>
#include "gtest/gtest.h"
#define private public
#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"
#undef private
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_worklet_input.h"
#include "gmock/gmock.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
class MockPaintWorkletInput : public cc::PaintWorkletInput {
 public:
  MockPaintWorkletInput() { keys.emplace_back("customPropertyName", id1); }
  gfx::SizeF GetSize() const override { return gfx::SizeF(10, 10); }

  int WorkletId() const override { return 42; }

  const PropertyKeys& GetPropertyKeys() const override { return keys; }

  bool IsCSSPaintWorkletInput() const override { return true; }

  bool KnownToBeOpaque() const override { return false; }

  ~MockPaintWorkletInput() override {}

 private:
  PropertyKeys keys;
  cc::ElementId id1;
};

class ImageTest : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

#ifdef OHOS_DRAG_DROP
TEST_F(ImageTest, ClipResizeAndOrientImage001) {
  const PaintImage image;
  const gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kOriginTopRight;
  float opacity = 1.0f;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  ASSERT_FALSE(orientation.UsesWidthAsHeight());
}

TEST_F(ImageTest, ClipResizeAndOrientImage002) {
  const PaintImage image;
  const gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kOriginRightBottom;
  float opacity = 1.0f;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  ASSERT_TRUE(orientation.UsesWidthAsHeight());
}

TEST_F(ImageTest, ClipResizeAndOrientImage003) {
  PaintImage image;
  const gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  float opacity = 1.0f;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  image.paint_worklet_input_ = new MockPaintWorkletInput();
  image_scale.set_x(1.f);
  image_scale.set_y(1.f);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  ASSERT_TRUE(opacity);
}

TEST_F(ImageTest, ClipResizeAndOrientImage004) {
  PaintImage image;
  gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  float opacity = 1;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  image.paint_worklet_input_ = new MockPaintWorkletInput();
  image_scale.set_x(1.f);
  image_scale.set_y(1.f);
  clip_rect.set_width(10);
  clip_rect.set_height(10);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  EXPECT_EQ(clip_rect.height(), image.height());
}

TEST_F(ImageTest, ClipResizeAndOrientImage005) {
  PaintImage image;
  gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  float opacity = 2;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  image.paint_worklet_input_ = new MockPaintWorkletInput();
  image_scale.set_x(1.f);
  image_scale.set_y(1.f);
  clip_rect.set_width(10);
  clip_rect.set_height(10);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  EXPECT_EQ(clip_rect.height(), image.height());
}

TEST_F(ImageTest, ClipResizeAndOrientImage006) {
  PaintImage image;
  gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  float opacity = 1;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  image.paint_worklet_input_ = new MockPaintWorkletInput();
  image_scale.set_x(1.f);
  image_scale.set_y(1.f);
  clip_rect.set_width(15);
  clip_rect.set_height(10);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  EXPECT_NE(clip_rect.width(), image.width());
}

TEST_F(ImageTest, ClipResizeAndOrientImage007) {
  PaintImage image;
  gfx::Rect clip_rect;
  gfx::Vector2dF image_scale;
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  float opacity = 1;
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(cc::PaintFlags::FilterQuality::kNone);
  image.paint_worklet_input_ = new MockPaintWorkletInput();
  image_scale.set_x(1.f);
  image_scale.set_y(1.f);
  clip_rect.set_width(10);
  clip_rect.set_height(15);
  Image::ClipResizeAndOrientImage(image, orientation, clip_rect, image_scale,
                                  opacity, interpolation_quality);
  EXPECT_NE(clip_rect.height(), image.height());
}
#endif

}  // namespace blink
