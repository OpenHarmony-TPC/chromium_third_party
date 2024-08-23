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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_

namespace gfx {
class Rect;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebNativeBridge {
 public:
  WebNativeBridge() = default;
  virtual ~WebNativeBridge() = default;

  virtual void StartPipeline() = 0;

  virtual void OnLayerRectChange(const gfx::Rect& rect) = 0;

  virtual void OnLayerRectVisibleChange(bool visibility) = 0;

  virtual int GetDelegateId() { return -1; }

  virtual gfx::Size NaturalSize() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_