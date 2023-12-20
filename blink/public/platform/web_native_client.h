/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_CLIENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "ui/gfx/color_space.h"

namespace cc {
class Layer;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebNativeClient {
 public:
  virtual void Repaint() = 0;
  virtual void SizeChanged(const gfx::Size&) = 0;
  virtual void SetCcLayer(cc::Layer*) = 0;

  virtual void OnCreateNativeSurface(int native_embed_id) = 0;
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
