/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NATIVE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NATIVE_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutNative;
struct PaintInfo;
struct PhysicalOffset;

class NativePainter {
  STACK_ALLOCATED();

 public:
  explicit NativePainter(const LayoutNative& layout_native)
      : layout_native_(layout_native) {}

  void PaintReplaced(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  const LayoutNative& layout_native_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NATIVE_PAINTER_H_
