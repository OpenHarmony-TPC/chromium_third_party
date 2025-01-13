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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_HEIF_HEIF_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_HEIF_HEIF_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"

namespace OHOS::NWeb {
class OhosImageDecoderAdapter;
}

namespace blink {

// Software decode is not supported now.This class is only used to implicate
// the image type and size.
class PLATFORM_EXPORT HEIFImageDecoder final : public ImageDecoder {
 public:
  HEIFImageDecoder(AlphaOption,
                   HighBitDepthDecodingOption,
                   const ColorBehavior&,
                   wtf_size_t max_decoded_bytes,
                   AnimationOption);
  HEIFImageDecoder(const HEIFImageDecoder&) = delete;
  HEIFImageDecoder& operator=(const HEIFImageDecoder&) = delete;
  ~HEIFImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "heif"; }
  const AtomicString& MimeType() const override;
  bool ImageIsHighBitDepth() override;
  void OnSetData(SegmentReader* data) override;
  cc::YUVSubsampling GetYUVSubsampling() const override;
  gfx::Size DecodedYUVSize(cc::YUVIndex) const override;
  wtf_size_t DecodedYUVWidthBytes(cc::YUVIndex) const override;
  SkYUVColorSpace GetYUVColorSpace() const override;
  uint8_t GetYUVBitDepth() const override;
  absl::optional<gfx::HDRMetadata> GetHDRMetadata() const override;
  void DecodeToYUV() override;
  int RepetitionCount() const override;
  bool FrameIsReceivedAtIndex(wtf_size_t) const override;
  absl::optional<base::TimeDelta> FrameTimestampAtIndex(
      wtf_size_t) const override;
  base::TimeDelta FrameDurationAtIndex(wtf_size_t) const override;
  bool ImageHasBothStillAndAnimatedSubImages() const override;

  static bool MatchesHeifSignature(const sk_sp<SkData>& data);
  static OHOS::NWeb::OhosImageDecoderAdapter* GetDecoderAdapter();

 private:
  scoped_refptr<SegmentReader> data_;
  // ImageDecoder:
  void DecodeSize() override;
  wtf_size_t DecodeFrameCount() override;
  void InitializeNewFrame(wtf_size_t) override;
  void Decode(wtf_size_t) override;
  bool CanReusePreviousFrameBuffer(wtf_size_t) const override;

  // The bit depth from the container.
  uint8_t bit_depth_ = 0;
  absl::optional<gfx::HDRMetadata> hdr_metadata_;

  // This is only used to parse image size and type.
  static std::unique_ptr<OHOS::NWeb::OhosImageDecoderAdapter> decoder_adapter_;
  wtf_size_t decoded_frame_count_ = 0;
  SkYUVColorSpace yuv_color_space_ = SkYUVColorSpace::kIdentity_SkYUVColorSpace;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_HEIF_HEIF_IMAGE_DECODER_H_
