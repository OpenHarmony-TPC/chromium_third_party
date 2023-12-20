/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_

namespace gfx {
class Size;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebNativeBridge {
 public:
  WebNativeBridge() = default;
  virtual ~WebNativeBridge() = default;

  virtual void StartPipeline() = 0;

  virtual void OnTextureSizeChange(const gfx::Size& size) = 0;

  virtual int GetDelegateId() { return -1; }

  virtual gfx::Size NaturalSize() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_H_