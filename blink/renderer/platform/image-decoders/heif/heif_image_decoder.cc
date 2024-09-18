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
#include "third_party/bounds_checking_function/include/securec.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace blink {

namespace {

// Refer to
// fundation/multimedia/image_framework/plugins/common/libs/image/formatagentplugin/src/heif_format_agent.cpp.
constexpr uint32_t kHeaderSize = 32;
constexpr uint32_t kHeaderLeastSize = 8;
constexpr size_t kHeaderNextSize = 16;
constexpr uint32_t kOffsetSize = 8;

constexpr uint32_t kShiftBase = 8;
constexpr uint32_t kTimesSeven = 7;
constexpr uint32_t kTimesFive = 5;
constexpr uint32_t kTimesThree = 3;
constexpr uint32_t kTimesTwo = 2;

uint32_t Fourcc(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4) {
  return (c1 << (kShiftBase * kTimesThree)) | (c2 << (kShiftBase * kTimesTwo)) |
         (c3 << kShiftBase) | (c4);
}

uint32_t EndianSwap32(uint32_t value) {
  return ((value & 0xFF) << (kShiftBase * kTimesThree)) |
         ((value & 0xFF00) << kShiftBase) | ((value & 0xFF0000) >> kShiftBase) |
         (value >> (kShiftBase * kTimesThree));
}

uint64_t EndianSwap64(uint64_t value) {
  return (((value & 0x00000000000000FFULL) << (kShiftBase * kTimesSeven)) |
          ((value & 0x000000000000FF00ULL) << (kShiftBase * kTimesFive)) |
          ((value & 0x0000000000FF0000ULL) << (kShiftBase * kTimesThree)) |
          ((value & 0x00000000FF000000ULL) << (kShiftBase)) |
          ((value & 0x000000FF00000000ULL) >> (kShiftBase)) |
          ((value & 0x0000FF0000000000ULL) >> (kShiftBase * kTimesThree)) |
          ((value & 0x00FF000000000000ULL) >> (kShiftBase * kTimesFive)) |
          ((value) >> (kShiftBase * kTimesSeven)));
}

bool IsHeif64(const void* buffer,
              const size_t bytes_read,
              int64_t& offset,
              uint64_t& chunk_size) {
  // If it is 1, a 64-bit check is required.
  if (chunk_size == 1) {
    // This indicates that the next 8 bytes represent the chunk size,
    // and chunk data comes after that.
    if (bytes_read < kHeaderNextSize) {
      LOG(INFO) << "[HeifSupport] bytes_read " << bytes_read
                << " is less than sixteen.";
      return false;
    }
    auto* chunk_size_ptr =
        static_cast<const uint64_t*>(buffer) + (offset / sizeof(uint64_t));
    chunk_size = EndianSwap64(*chunk_size_ptr);
    if (chunk_size < kHeaderNextSize) {
      // The smallest valid chunk is 16 bytes long in this case.
      LOG(INFO) << "[HeifSupport] chunk_size "
                << static_cast<unsigned long long>(chunk_size)
                << " is less than sixteen.";
      return false;
    }
    offset += kOffsetSize;
  } else if (chunk_size < kHeaderLeastSize) {
    // The smallest valid chunk is 8 bytes long.
    LOG(INFO) << "[HeifSupport] chunk_size "
              << static_cast<unsigned long long>(chunk_size)
              << " is less than eight.";
    return false;
  }

  if (chunk_size > bytes_read) {
    chunk_size = bytes_read;
  }
  return true;
}

bool CheckFormat(const void* header_data, uint32_t data_size) {
  if (!header_data) {
    LOG(INFO) << "[HeifSupport] header_data is null.";
    return false;
  }
  // Any valid ftyp box should have at least 8 bytes.
  if (data_size < kHeaderLeastSize) {
    LOG(INFO) << "[HeifSupport] data_size " << data_size
              << " is less than eight.";
    return false;
  }

  uint32_t tmp_buff[kHeaderSize];
  if (memcpy_s(tmp_buff, kHeaderSize, header_data, kHeaderSize) != EOK) {
    return false;
  }

  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tmp_buff);
  uint64_t chunk_size = EndianSwap32(ptr[0]);  // first item
  uint32_t chunk_type = EndianSwap32(ptr[1]);  // second item
  if (chunk_type != Fourcc('f', 't', 'y', 'p')) {
    LOG(INFO) << "[HeifSupport] head type is not ftyp.";
    return false;
  }

  int64_t offset = kOffsetSize;
  if (!IsHeif64(tmp_buff, data_size, offset, chunk_size)) {
    return false;
  }
  int64_t chunk_data_size = static_cast<int64_t>(chunk_size) - offset;
  // It should at least have major brand (4-byte) and minor version (4-bytes).
  // The rest of the chunk (if any) is a list of (4-byte) compatible brands.
  if (chunk_data_size < kHeaderLeastSize) {
    LOG(INFO) << "[HeifSupport] chunk data size "
              << static_cast<long long>(chunk_data_size)
              << " is less than eight.";
    return false;
  }
  uint32_t num_compatible_brands =
      (uint32_t)(chunk_data_size - kOffsetSize) / sizeof(uint32_t);
  if (num_compatible_brands != 0 &&
      num_compatible_brands + kTimesTwo < kHeaderSize) {
    for (size_t i = 0; i < num_compatible_brands + 2;
         ++i) {
      // need next 2 item
      if (i == 1) {
        // Skip this index, it refers to the minorVersion, not a brand.
        continue;
      }
      auto* brand_ptr =
          static_cast<const uint32_t*>(tmp_buff) + (num_compatible_brands + i);
      uint32_t brand = EndianSwap32(*brand_ptr);
      if (brand == Fourcc('m', 'i', 'f', '1') ||
          brand == Fourcc('h', 'e', 'i', 'c') ||
          brand == Fourcc('m', 's', 'f', '1') ||
          brand == Fourcc('h', 'e', 'v', 'c')) {
        return true;
      }
    }
  }
  LOG(INFO) << "[HeifSupport] check heif format is failed.";
  return false;
}

}  // namespace

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

  if (GetDecoderAdapter()->ParseImageInfo(encoded_data.data(),
                                          (uint32_t)encoded_data.size())) {
    LOG(INFO) << "[HeifSupport] HEIF image size " << GetDecoderAdapter()->GetImageWidth()
              << " * " << GetDecoderAdapter()->GetImageHeight();
    SetSize(GetDecoderAdapter()->GetImageWidth(),
            GetDecoderAdapter()->GetImageHeight());
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

void HEIFImageDecoder::Decode(wtf_size_t index) {}

bool HEIFImageDecoder::CanReusePreviousFrameBuffer(wtf_size_t index) const {
  return true;
}

bool HEIFImageDecoder::MatchesHeifSignature(const sk_sp<SkData>& data) {
  base::span<const uint8_t> encoded_data =
      base::make_span(data->bytes(), data->size());
  if (CheckFormat(encoded_data.data(), (uint32_t)encoded_data.size())) {
    LOG(INFO) << "[HeifSupport] HEIFImageDecoder::MatchesHeifSignature is heif "
                 "format.";
    return true;
  }

  // Do double check.
  if (GetDecoderAdapter()->ParseImageInfo(encoded_data.data(),
                                          (uint32_t)encoded_data.size())) {
    return GetDecoderAdapter()->GetEncodedFormat() == "image/heif";
  }
  LOG(INFO) << "[HeifSupport] Fail to parsing image information. ";
  return false;
}

OHOS::NWeb::OhosImageDecoderAdapter* HEIFImageDecoder::GetDecoderAdapter() {
  if (!decoder_adapter_) {
    decoder_adapter_ = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                           .CreateOhosImageDecoderAdapter();
  }

  return decoder_adapter_.get();
}

}  // namespace blink