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
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#undef protected
#undef private
#include <gmock/gmock.h>
#include <memory>
#include "base/task/single_thread_task_runner.h"
#include "gtest/gtest.h"

namespace blink {
class MockWebPlatformMediaStreamSource
    : public blink::WebPlatformMediaStreamSource {
 public:
  explicit MockWebPlatformMediaStreamSource(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : WebPlatformMediaStreamSource(task_runner) {}
  MOCK_METHOD(size_t, NumTracks, (), (const, override));
  MOCK_METHOD(void, DoStopSource, (), (override));
  MOCK_METHOD(void,
              DoChangeSource,
              (const MediaStreamDevice& new_device),
              (override));
};

class WebPlatformMediaStreamSourceTest : public testing::Test {
 protected:
  void SetUp() override {
    web_meida_ptr = std::make_unique<MockWebPlatformMediaStreamSource>(nullptr);
  }
  void TearDown() override { web_meida_ptr.reset(); }
  std::unique_ptr<WebPlatformMediaStreamSource> web_meida_ptr;
};

TEST_F(WebPlatformMediaStreamSourceTest, FinalizeStopSource) {
  web_meida_ptr->FinalizeStopSource();
  ASSERT_TRUE(web_meida_ptr->stop_callback_.is_null());
}

TEST_F(WebPlatformMediaStreamSourceTest, SetSourceMuted) {
  bool is_muted = true;
  web_meida_ptr->SetSourceMuted(is_muted);
  ASSERT_TRUE(!web_meida_ptr->Owner());
}

}  // namespace blink
