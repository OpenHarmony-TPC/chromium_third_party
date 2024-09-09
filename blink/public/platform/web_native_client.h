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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_CLIENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class Layer;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebNativeClient {
 public:
  virtual void Repaint() = 0;
  virtual void SetCcLayer(cc::Layer*) = 0;

  using RectChangeCB = base::RepeatingCallback<void(const gfx::Rect&, const bool)>;
  virtual void OnCreateNativeSurface(int native_embed_id,
                                     RectChangeCB rect_changed_cb) = 0;
  virtual void OnLayerRectChange(const gfx::Rect& rect) = 0;
  virtual void OnLayerRectVisibilityChange(bool visibility) = 0;
  virtual void OnDestroyNativeSurface() = 0;

  // Returns the color space to render media into if.
  // Rendering media into this color space may avoid some conversions.
  virtual gfx::ColorSpace TargetColorSpace() {
    return gfx::ColorSpace();
  }

 protected:
  ~WebNativeClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_CLIENT_H_
