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

#include <cstddef>
#define private public
#define protected public
#include "include/core/SkData.h"
#include "third_party/blink/renderer/platform/image-decoders/heif/heif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#undef protected
#undef private
#include <stdint.h>
#include <cstring>
#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/heif/heif_image_decoder.cc"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using namespace blink;

class MockSegmentReader : public SegmentReader {
 public:
  MOCK_METHOD(size_t, size, (), (const, override));
  MOCK_METHOD(size_t,
              GetSomeData,
              (const char*& data, size_t position),
              (const, override));
  MOCK_METHOD(sk_sp<SkData>, GetAsSkData, (), (const, override));
};

class MockOhosImageDecoderAdapter : public OHOS::NWeb::OhosImageDecoderAdapter {
 public:
  MOCK_METHOD(bool,
              ParseImageInfo,
              (const uint8_t* data, uint32_t size),
              (override));
  MOCK_METHOD(std::string, GetEncodedFormat, (), (override));
  MOCK_METHOD(int32_t, GetImageWidth, (), (override));
  MOCK_METHOD(int32_t, GetImageHeight, (), (override));
  MOCK_METHOD(bool,
              DecodeToPixelMap,
              (const uint8_t* data, uint32_t size),
              (override));
  MOCK_METHOD(int32_t, GetFd, (), (override));
  MOCK_METHOD(int32_t, GetStride, (), (override));
  MOCK_METHOD(int32_t, GetOffset, (), (override));
  MOCK_METHOD(uint64_t, GetSize, (), (override));
  MOCK_METHOD(void*, GetNativeWindowBuffer, (), (override));
  MOCK_METHOD(int32_t, GetPlanesCount, (), (override));
  MOCK_METHOD(void, ReleasePixelMap, (), (override));
  MOCK_METHOD(bool,
              Decode,
              (const uint8_t* data,
               uint32_t size,
               OHOS::NWeb::AllocatorType type,
               bool useYuv),
              (override));
};

class HeifImageDecoderTest : public testing::Test {
 protected:
  void SetUp() override {
    herfTest = std::make_unique<HEIFImageDecoder>(
        ImageDecoder::kAlphaNotPremultiplied,
        ImageDecoder::HighBitDepthDecodingOption::kDefaultBitDepth,
        ColorBehavior::TransformToSRGB(),
        ImageDecoder::kNoDecodedImageByteLimit,
        ImageDecoder::AnimationOption::kPreferStillImage);
    mockReader = std::make_unique<MockSegmentReader>();
  }
  void TearDown() override { herfTest.reset(); }
  std::unique_ptr<HEIFImageDecoder> herfTest;
  std::unique_ptr<MockSegmentReader> mockReader;
};

TEST_F(HeifImageDecoderTest, IsHeif64_001) {
  int64_t offset = 10;
  uint64_t chunk_size = 1;
  size_t bytes_read = 10;
  void* buffer;
  EXPECT_TRUE(bytes_read < kHeaderNextSize);
  bool flag = IsHeif64(buffer, bytes_read, offset, chunk_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_002) {
  uint64_t chunk_size = 1;
  size_t bytes_read = 20;
  uint64_t buffer[8] = {0};
  int64_t offset = 4 * sizeof(uint64_t);
  EXPECT_TRUE(chunk_size < kHeaderNextSize);
  bool flag = IsHeif64(buffer, bytes_read, offset, chunk_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_003) {
  int64_t offset = 10;
  uint64_t chunk_size = 7;
  size_t bytes_read = 20;
  void* buffer;
  bool flag = IsHeif64(buffer, bytes_read, offset, chunk_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_004) {
  int64_t offset = 10;
  uint64_t chunk_size = 11;
  size_t bytes_read = 10;
  void* buffer;
  bool flag = IsHeif64(buffer, bytes_read, offset, chunk_size);
  EXPECT_EQ(chunk_size, bytes_read);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_005) {
  int64_t offset = 10;
  uint64_t chunk_size = 9;
  size_t bytes_read = 10;
  void* buffer;
  bool flag = IsHeif64(buffer, bytes_read, offset, chunk_size);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_001) {
  uint32_t data_size = 9;
  void* header_data = nullptr;
  EXPECT_FALSE(header_data);
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_002) {
  uint32_t data_size = 9;
  uint32_t header_data[8] = {0};
  EXPECT_TRUE(header_data);
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_003) {
  uint32_t data_size = 7;
  uint32_t header_data[8] = {0};
  EXPECT_TRUE(header_data);
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_004) {
  uint32_t data_size = 9;
  uint32_t header_data[8] = {0};
  EXPECT_TRUE(header_data);
  EXPECT_NE(EndianSwap32(header_data[1]), Fourcc('f', 't', 'y', 'p'));
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_005) {
  uint32_t data_size = 10;
  uint32_t header_data[16] = {0};
  EXPECT_TRUE(header_data);
  uint8_t* header_as_uint8 = reinterpret_cast<uint8_t*>(header_data);
  header_as_uint8[0] = 9;
  header_as_uint8[4] = 'f';
  header_as_uint8[5] = 't';
  header_as_uint8[6] = 'y';
  header_as_uint8[7] = 'p';
  EXPECT_EQ(EndianSwap32(header_data[1]), Fourcc('f', 't', 'y', 'p'));
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_006) {
  uint32_t data_size = 20;
  uint32_t header_data[16] = {0};
  EXPECT_TRUE(header_data);
  uint8_t* header_as_uint8 = reinterpret_cast<uint8_t*>(header_data);
  header_as_uint8[0] = 7;
  header_as_uint8[4] = 'f';
  header_as_uint8[5] = 't';
  header_as_uint8[6] = 'y';
  header_as_uint8[7] = 'p';
  EXPECT_EQ(EndianSwap32(header_data[1]), Fourcc('f', 't', 'y', 'p'));
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_007) {
  uint32_t header_data[16] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  EXPECT_TRUE(header_data);
  uint8_t* header_as_uint8 = reinterpret_cast<uint8_t*>(header_data);
  header_as_uint8[4] = 'f';
  header_as_uint8[5] = 't';
  header_as_uint8[6] = 'y';
  header_as_uint8[7] = 'p';
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_EQ(EndianSwap32(header_data[1]), Fourcc('f', 't', 'y', 'p'));
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_008) {
  uint32_t header_data[16] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  EXPECT_TRUE(header_data);
  uint32_t* header_as_uint32 = reinterpret_cast<uint32_t*>(header_data);
  header_as_uint32[0] = 9;
  uint8_t* header_as_uint8 = reinterpret_cast<uint8_t*>(header_data);
  header_as_uint8[4] = 'f';
  header_as_uint8[5] = 't';
  header_as_uint8[6] = 'y';
  header_as_uint8[7] = 'p';
  EXPECT_EQ(EndianSwap32(header_data[1]), Fourcc('f', 't', 'y', 'p'));
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_009) {
  uint64_t header_data[8] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  uint32_t* header_as_uint32 = reinterpret_cast<uint32_t*>(header_data);
  header_as_uint32[0] = EndianSwap32(24);
  header_as_uint32[1] = EndianSwap32(Fourcc('f', 't', 'y', 'p'));
  header_as_uint32[2] = EndianSwap32(Fourcc('m', 'i', 'f', '1'));
  header_as_uint32[3] = 0;
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_010) {
  uint64_t header_data[8] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  uint32_t* header_as_uint32 = reinterpret_cast<uint32_t*>(header_data);
  header_as_uint32[0] = EndianSwap32(24);
  header_as_uint32[1] = EndianSwap32(Fourcc('f', 't', 'y', 'p'));
  header_as_uint32[2] = EndianSwap32(Fourcc('h', 'e', 'i', 'c'));
  header_as_uint32[3] = 0;
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_011) {
  uint64_t header_data[8] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  uint32_t* header_as_uint32 = reinterpret_cast<uint32_t*>(header_data);
  header_as_uint32[0] = EndianSwap32(24);
  header_as_uint32[1] = EndianSwap32(Fourcc('f', 't', 'y', 'p'));
  header_as_uint32[2] = EndianSwap32(Fourcc('m', 's', 'f', '1'));
  header_as_uint32[3] = 0;
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, CheckFormat_012) {
  uint64_t header_data[8] = {0};
  int64_t data_size = 4 * sizeof(uint64_t);
  uint32_t* header_as_uint32 = reinterpret_cast<uint32_t*>(header_data);
  header_as_uint32[0] = EndianSwap32(24);
  header_as_uint32[1] = EndianSwap32(Fourcc('f', 't', 'y', 'p'));
  header_as_uint32[2] = EndianSwap32(Fourcc('h', 'e', 'v', 'c'));
  header_as_uint32[3] = 0;
  bool flag = CheckFormat(header_data, data_size);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, OnSetData_001) {
  ASSERT_TRUE(herfTest);
  herfTest->OnSetData(mockReader.get());
  EXPECT_FALSE(herfTest->IsAllDataReceived());
}

TEST_F(HeifImageDecoderTest, OnSetData_002) {
  ASSERT_TRUE(herfTest);
  herfTest->is_all_data_received_ = true;
  EXPECT_TRUE(herfTest->IsAllDataReceived());
  ASSERT_TRUE(mockReader);
  EXPECT_CALL(*mockReader, size()).Times(AtLeast(1));
  EXPECT_CALL(*mockReader, GetSomeData(_, _)).Times(AtLeast(0));
  EXPECT_CALL(*mockReader, GetAsSkData()).Times(AtLeast(0));
  herfTest->OnSetData(mockReader.get());
  EXPECT_TRUE(mockReader);
  size_t num = 0;
  EXPECT_EQ(mockReader->size(), num);
}

TEST_F(HeifImageDecoderTest, OnSetData_003) {
  ASSERT_TRUE(herfTest);
  herfTest->is_all_data_received_ = true;
  EXPECT_TRUE(herfTest->IsAllDataReceived());
  ASSERT_TRUE(mockReader);
  mockReader = nullptr;
  herfTest->OnSetData(mockReader.get());
  EXPECT_FALSE(mockReader);
}

TEST_F(HeifImageDecoderTest, OnSetData_004) {
  ASSERT_TRUE(herfTest);
  herfTest->is_all_data_received_ = true;
  EXPECT_TRUE(herfTest->IsAllDataReceived());
  std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04};
  EXPECT_CALL(*mockReader, size())
      .WillRepeatedly(::testing::Return(testData.size()));
  EXPECT_CALL(*mockReader, GetAsSkData())
      .WillRepeatedly(::testing::Return(sk_sp(new SkData(4))));
  herfTest->OnSetData(mockReader.get());
  size_t num = 0;
  EXPECT_NE(mockReader->size(), num);
}

TEST_F(HeifImageDecoderTest, OnSetData_005) {
  ASSERT_TRUE(herfTest);
  herfTest->is_all_data_received_ = true;
  EXPECT_TRUE(herfTest->IsAllDataReceived());
  std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04};
  EXPECT_CALL(*mockReader, size())
      .WillRepeatedly(::testing::Return(testData.size()));
  EXPECT_CALL(*mockReader, GetAsSkData())
      .WillRepeatedly(::testing::Return(sk_sp(new SkData(4))));
  std::unique_ptr<MockOhosImageDecoderAdapter> mock_decoder_adapter =
      std::make_unique<MockOhosImageDecoderAdapter>();
  EXPECT_CALL(*mock_decoder_adapter, ParseImageInfo(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(*mock_decoder_adapter, GetImageWidth())
      .Times(::testing::AtLeast(2))
      .WillRepeatedly(::testing::Return(800));
  EXPECT_CALL(*mock_decoder_adapter, GetImageHeight())
      .Times(::testing::AtLeast(2))
      .WillRepeatedly(::testing::Return(600));
  herfTest->decoder_adapter_ = std::move(mock_decoder_adapter);
  herfTest->OnSetData(mockReader.get());
  size_t num = 0;
  EXPECT_NE(mockReader->size(), num);
}

TEST_F(HeifImageDecoderTest, FrameTimestampAtIndex_001) {
  ImageFrame image;
  wtf_size_t index = 6;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  auto result = herfTest->FrameTimestampAtIndex(index);
  EXPECT_EQ(result, absl::nullopt);
}

TEST_F(HeifImageDecoderTest, FrameTimestampAtIndex_002) {
  ImageFrame image;
  wtf_size_t index = 0;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  auto result = herfTest->FrameTimestampAtIndex(index);
  EXPECT_EQ(result, herfTest->frame_buffer_cache_[index].Timestamp());
}

TEST_F(HeifImageDecoderTest, FrameDurationAtIndex_001) {
  ImageFrame image;
  wtf_size_t index = 6;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  auto before = base::TimeDelta();
  auto result = herfTest->FrameDurationAtIndex(index);
  auto after = base::TimeDelta();
  EXPECT_TRUE(before <= result && result <= after);
}

TEST_F(HeifImageDecoderTest, FrameDurationAtIndex_002) {
  ImageFrame image;
  wtf_size_t index = 0;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  auto result = herfTest->FrameDurationAtIndex(index);
  EXPECT_EQ(result, herfTest->frame_buffer_cache_[index].Duration());
}

TEST_F(HeifImageDecoderTest, DecodeFrameCount_001) {
  herfTest->failed_ = false;
  herfTest->size_available_ = false;
  ImageFrame image;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  auto result = herfTest->DecodeFrameCount();
  EXPECT_EQ(result, herfTest->frame_buffer_cache_.size());
}

TEST_F(HeifImageDecoderTest, DecodeFrameCount_002) {
  herfTest->failed_ = false;
  herfTest->size_available_ = true;
  ImageFrame image;
  herfTest->frame_buffer_cache_.clear();
  herfTest->frame_buffer_cache_.emplace_back(std::move(image));
  EXPECT_TRUE(herfTest->IsDecodedSizeAvailable());
  wtf_size_t num = 1;
  auto result = herfTest->DecodeFrameCount();
  EXPECT_EQ(result, num);
}

TEST_F(HeifImageDecoderTest, MatchesHeifSignature) {
  size_t size = 2;
  sk_sp<SkData> data = sk_make_sp<SkData>(size);
  auto result = herfTest->MatchesHeifSignature(data);
  EXPECT_FALSE(result);
}

TEST_F(HeifImageDecoderTest, GetDecoderAdapter_001) {
  herfTest->decoder_adapter_ = nullptr;
  auto result = herfTest->GetDecoderAdapter();
  EXPECT_TRUE(result);
}

TEST_F(HeifImageDecoderTest, GetDecoderAdapter_002) {
  std::unique_ptr<MockOhosImageDecoderAdapter> mock_decoder_adapter =
      std::make_unique<MockOhosImageDecoderAdapter>();
  herfTest->decoder_adapter_ = std::move(mock_decoder_adapter);
  auto result = herfTest->GetDecoderAdapter();
  EXPECT_TRUE(result);
}
