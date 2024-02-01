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
