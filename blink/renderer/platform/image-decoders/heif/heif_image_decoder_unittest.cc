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

#include <stdint.h>
#include <cstring>
#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/heif/heif_image_decoder.cc"
#include "third_party/blink/renderer/platform/image-decoders/heif/heif_image_decoder.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

using namespace blink;
class HeifImageDecoderTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(HeifImageDecoderTest, IsHeif64_001) {
  int64_t offset_ = 10;
  uint64_t chunk_size_ = 1;
  size_t bytes_read = 10;
  void* buffer;
  EXPECT_TRUE(bytes_read < kHeaderNextSize);
  bool flag = IsHeif64(buffer, bytes_read, offset_, chunk_size_);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_002) {
  uint64_t chunk_size_ = 1;
  size_t bytes_read = 20;
  uint64_t buffer[8] = {0};
  int64_t offset_ = 4 * sizeof(uint64_t);
  EXPECT_TRUE(chunk_size_ < kHeaderNextSize);
  bool flag = IsHeif64(buffer, bytes_read, offset_, chunk_size_);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_003) {
  int64_t offset_ = 10;
  uint64_t chunk_size_ = 7;
  size_t bytes_read = 20;
  void* buffer = std::unique_ptr<uint8_t[]>(new uint8_t[bytes_read]).get();
  bool flag = IsHeif64(buffer, bytes_read, offset_, chunk_size_);
  EXPECT_FALSE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_004) {
  int64_t offset_ = 10;
  uint64_t chunk_size_ = 11;
  size_t bytes_read = 10;
  void* buffer = std::unique_ptr<uint8_t[]>(new uint8_t[bytes_read]).get();
  bool flag = IsHeif64(buffer, bytes_read, offset_, chunk_size_);
  EXPECT_TRUE(chunk_size_ == bytes_read);
  EXPECT_TRUE(flag);
}

TEST_F(HeifImageDecoderTest, IsHeif64_005) {
  int64_t offset_ = 10;
  uint64_t chunk_size_ = 9;
  size_t bytes_read = 10;
  void* buffer = std::unique_ptr<uint8_t[]>(new uint8_t[bytes_read]).get();
  bool flag = IsHeif64(buffer, bytes_read, offset_, chunk_size_);
  EXPECT_TRUE(flag);
}
