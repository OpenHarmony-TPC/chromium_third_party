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

#include "third_party/blink/renderer/platform/image-decoders/heif/heif_image_decoder.h"

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "base/bits.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "media/video/half_float_maker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/src/codec/SkHeifCodec.h"
#include "third_party/skia/include/codec/SkEncodedImageFormat.h"
#include "third_party/bounds_checking_function/include/securec.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace blink {

std::unique_ptr<OHOS::NWeb::OhosImageDecoderAdapter>
    HEIFImageDecoder::decoder_adapter_;

HEIFImageDecoder::HEIFImageDecoder(AlphaOption alpha_option,
                                   HighBitDepthDecodingOption hbd_option,
                                   const ColorBehavior& color_behavior,
                                   wtf_size_t max_decoded_bytes,
                                   AnimationOption animation_option)
    : ImageDecoder(alpha_option,
                   hbd_option,
                   color_behavior,
                   max_decoded_bytes) {}

HEIFImageDecoder::~HEIFImageDecoder() {
  decoder_adapter_ = nullptr;
}

const AtomicString& HEIFImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, heif_mime_type, ("image/heif"));
  return heif_mime_type;
}

bool HEIFImageDecoder::ImageIsHighBitDepth() {
  return bit_depth_ > 8;
}

void HEIFImageDecoder::OnSetData(SegmentReader* data) {
  if (!IsAllDataReceived() || !data || data->size() == 0) {
    return;
  }
  LOG(INFO) << "[HeifSupport] HEIFImageDecoder::OnSetData parse image size " << data->size();
  auto sk_data = data->GetAsSkData();
  base::span<const uint8_t> encoded_data =
      base::make_span(sk_data->bytes(), sk_data->size());

  OHOS::NWeb::OhosImageDecoderAdapter* decoder_adapter = GetDecoderAdapter();
  if (decoder_adapter == nullptr) {
    LOG(ERROR) << "[HeifSupport] Fail to get decoder adapter.";
    SetFailed();
    return;
  }

  if (decoder_adapter->ParseImageInfo(encoded_data.data(), (uint32_t)encoded_data.size())) {
    LOG(INFO) << "[HeifSupport] HEIF image size " << decoder_adapter->GetImageWidth()
              << " * " << decoder_adapter->GetImageHeight();
    SetSize(decoder_adapter->GetImageWidth(),
            decoder_adapter->GetImageHeight());
    data_ = data;
    return;
  }
  LOG(ERROR) << "[HeifSupport] Fail to parsing image information. ";
  SetFailed();
}

cc::YUVSubsampling HEIFImageDecoder::GetYUVSubsampling() const {
  return cc::YUVSubsampling::k420;
}

gfx::Size HEIFImageDecoder::DecodedYUVSize(cc::YUVIndex index) const {
  DCHECK(IsDecodedSizeAvailable());
  return Size();
}

wtf_size_t HEIFImageDecoder::DecodedYUVWidthBytes(cc::YUVIndex index) const {
  return 0;
}

SkYUVColorSpace HEIFImageDecoder::GetYUVColorSpace() const {
  DCHECK(CanDecodeToYUV());
  DCHECK_NE(yuv_color_space_, SkYUVColorSpace::kIdentity_SkYUVColorSpace);
  return yuv_color_space_;
}

uint8_t HEIFImageDecoder::GetYUVBitDepth() const {
  DCHECK(CanDecodeToYUV());
  return bit_depth_;
}

absl::optional<gfx::HDRMetadata> HEIFImageDecoder::GetHDRMetadata() const {
  return hdr_metadata_;
}

void HEIFImageDecoder::DecodeToYUV() {}

int HEIFImageDecoder::RepetitionCount() const {
  return kAnimationNone;
}

bool HEIFImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  return true;
}

absl::optional<base::TimeDelta> HEIFImageDecoder::FrameTimestampAtIndex(
    wtf_size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Timestamp()
             : absl::nullopt;
}

base::TimeDelta HEIFImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Duration()
             : base::TimeDelta();
}

bool HEIFImageDecoder::ImageHasBothStillAndAnimatedSubImages() const {
  return false;
}

void HEIFImageDecoder::DecodeSize() {}

wtf_size_t HEIFImageDecoder::DecodeFrameCount() {
  decoded_frame_count_ = 1;
  return IsDecodedSizeAvailable() ? decoded_frame_count_
                                  : frame_buffer_cache_.size();
}

void HEIFImageDecoder::InitializeNewFrame(wtf_size_t index) {}

void HEIFImageDecoder::Decode(wtf_size_t index) {
  if (frame_buffer_cache_.empty()) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode frame_buffer_cache is empty.";
    SetFailed();
    return;
  }

  const char* segment = nullptr;
  const size_t bytes = data_->GetSomeData(segment, 0);
  if (segment == nullptr || bytes == 0) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode GetSomeData failed.";
    SetFailed();
    return;
  }

  bool decode_rst = GetDecoderAdapter()->Decode((const uint8_t*)segment, (uint32_t)data_->size(),
                                                OHOS::NWeb::AllocatorType::kShareMemAlloc, false);
  if (!decode_rst) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode Decode failed.";
    SetFailed();
    return;
  }
  LOG(DEBUG) << "[HeifSupport] HEIFImageDecoder::Decode GetDecoderAdapter()->Decode succeed.";

  void *ptr = GetDecoderAdapter()->GetDecodeData();
  if (ptr == nullptr) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode GetDecodeData failed.";
    SetFailed();
    return;
  }

  if (!InitFrameBuffer(index)) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode InitFrameBuffer failed.";
    SetFailed();
    return;
  }

  ImageFrame& buffer = frame_buffer_cache_[index];
  ImageFrame::PixelData* pixel = buffer.GetAddr(0, 0);
  if (pixel == nullptr) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode GetAddr failed.";
    SetFailed();
    return;
  }

  int stride = GetDecoderAdapter()->GetStride();
  if (stride <= 0) {
    LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode stride error. stride = " << stride;
    SetFailed();
    return;
  }

  int width = buffer.Bitmap().width();
  int height = buffer.Bitmap().height();
  size_t rowBytes = buffer.Bitmap().rowBytes();
  int bytesPerPixel = buffer.Bitmap().bytesPerPixel();
  for (int i = 0; i < height; i++) {
    if (memcpy_s((uint8_t*)pixel + i * width * bytesPerPixel, rowBytes,
                 (uint8_t*)ptr + i * stride, width * bytesPerPixel) != EOK) {
      LOG(ERROR) << "[HeifSupport] HEIFImageDecoder::Decode memcpy failed."
                 << "width = " << width
                 << ", height = " << height
                 << ", rowBytes = " << rowBytes
                 << ", bytesPerPixel = " << bytesPerPixel
                 << ", stride = " << stride;
      SetFailed();
      return;
    }
  }

  GetDecoderAdapter()->ReleasePixelMap();

  buffer.SetPixelsChanged(true);
  buffer.SetStatus(ImageFrame::kFrameComplete);
  LOG(DEBUG) << "[HeifSupport] HEIFImageDecoder::Decode decode complete.";
}

bool HEIFImageDecoder::CanReusePreviousFrameBuffer(wtf_size_t index) const {
  return true;
}

bool HEIFImageDecoder::MatchesHeifSignature(const sk_sp<SkData>& data) {
  base::span<const uint8_t> encoded_data =
      base::make_span(data->bytes(), data->size());

  if (!encoded_data.data()) {
    LOG(INFO) << "[HeifSupport] HEIFImageDecoder::MatchesHeifSignature, encoded_data is null.";
    return false;
  }

  SkEncodedImageFormat format;
  if ((SkHeifCodec::IsSupported(encoded_data.data(), (size_t)encoded_data.size(), &format))
      && format == SkEncodedImageFormat::kHEIF) {
    LOG(INFO) << "[HeifSupport] HEIFImageDecoder::MatchesHeifSignature is heif format.";
    return true;
  }

  OHOS::NWeb::OhosImageDecoderAdapter* decoder_adapter = GetDecoderAdapter();
  if (decoder_adapter == nullptr) {
    LOG(ERROR) << "[HeifSupport] Fail to get decoder adapter.";
    return false;
  }

  // Do double check.
  if (decoder_adapter->ParseImageInfo(encoded_data.data(), (uint32_t)encoded_data.size())) {
    return decoder_adapter->GetEncodedFormat() == "image/heif";
  }
  LOG(INFO) << "[HeifSupport] Fail to parsing image information. ";
  return false;
}

OHOS::NWeb::OhosImageDecoderAdapter* HEIFImageDecoder::GetDecoderAdapter() {
  if (!decoder_adapter_) {
    decoder_adapter_ = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                           .CreateOhosImageDecoderAdapter();
    if (!decoder_adapter_) {
      return nullptr;
    }
  }

  return decoder_adapter_.get();
}

}  // namespace blink
