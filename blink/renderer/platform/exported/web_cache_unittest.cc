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

#include <memory>
#define private public
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#undef private
#if BUILDFLAG(IS_OHOS)
#include <string>
#include <vector>
#endif

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(OHOS_UNITTESTS)
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#endif
#if BUILDFLAG(IS_OHOS)
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#endif

namespace blink {
class WebCacheTest : public ::testing::Test {
 public:
  void SetUp() override {
    resource_response_ = std::make_unique<ResourceResponse>();
  }
  void TearDown() override { resource_response_.reset(); }
  std::unique_ptr<ResourceResponse> resource_response_;
};

TEST_F(WebCacheTest, GetResourceResponseTest1) {
  base::flat_map<std::string, std::string> response_headers = {
      {"Content-Type", "text/html"}, {"Custom-Header", "value"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  EXPECT_EQ(response.HttpHeaderField(AtomicString("Content-Type")),
            AtomicString("text/html"));
}

TEST_F(WebCacheTest, GetResourceResponseTest2) {
  base::flat_map<std::string, std::string> response_headers = {
      {"Content-Type", "text/html"}, {"Custom-Header", "value"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  EXPECT_EQ(response.HttpHeaderField(AtomicString("Custom-Header")),
            AtomicString("value"));
}

TEST_F(WebCacheTest, GetResourceResponseTest3) {
  base::flat_map<std::string, std::string> response_headers = {};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("max-age=86400"));
}

TEST_F(WebCacheTest, GetResourceResponseTest4) {
  base::flat_map<std::string, std::string> response_headers = {
      {"Cache-Control", "some-value"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("some-value"));
}

TEST_F(WebCacheTest, GetResourceResponseTest5) {
  base::flat_map<std::string, std::string> response_headers = {
      {"Expires", "some-date"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_TRUE(cache_control_after.empty());
}

TEST_F(WebCacheTest, GetResourceResponseTest6) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "some-value"}, {"Expires", "some-date"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("some-value"));
}

TEST_F(WebCacheTest, GetResourceResponseTest7) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "some-value"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  EXPECT_FALSE(response.CacheControlContainsNoCache());
  ResourceResponse response2 =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response2.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("some-value"));
}

TEST_F(WebCacheTest, GetResourceResponseTest8) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "no-cache"}};
  KURL kurl;
  resource_response_->cache_control_header_.contains_no_cache = 0;
  ResourceResponse response2 =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response2.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("max-age=86400"));
}

TEST_F(WebCacheTest, GetResourceResponseTest9) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "no-store"}, {"Expires", "some-date"}};
  KURL kurl;
  resource_response_->cache_control_header_.contains_no_store = 0;
  ResourceResponse response2 =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response2.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("max-age=86400"));
}

TEST_F(WebCacheTest, GetResourceResponseTest10) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "no-cache,no-store"}};
  KURL kurl;
  resource_response_->cache_control_header_.contains_no_store = 0;
  resource_response_->cache_control_header_.contains_no_cache = 0;
  ResourceResponse response2 =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString cache_control_after =
      response2.HttpHeaderField(AtomicString("Cache-Control"));
  EXPECT_EQ(cache_control_after, AtomicString("max-age=86400"));
}

TEST_F(WebCacheTest, GetResourceResponseTest11) {
  base::flat_map<std::string, std::string> response_headers{
      {"Cache-Control", "no-cache,no-store"}, {"Expires", "some-date"}};
  KURL kurl;
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  AtomicString mimeType = response.MimeType();
  EXPECT_TRUE(mimeType.empty());
}

#if defined(OHOS_UNITTESTS)
TEST_F(WebCacheTest, AddResourceToCache001) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MakeRefCounted<base::TestSimpleTaskRunner>());
  base::SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner);
  std::string url = "https://example.com";
  std::string origin = "https://example2.com";
  std::vector<uint8_t> resource;
  resource.push_back(0);
  resource.push_back(1);
  base::flat_map<std::string, std::string> response_headers = {
      {"StreamType", "CRAS_STREAM_TYPE_DEFAULT"},
      {"ClientType", "CRAS_CLIENT_TYPE_CHROME"},
      {"NodeType", "USB_INTERNAL_SPEAKER"}};
  uint64_t type = 1;
  WebCache::AddResourceToCache(url, origin, resource, response_headers, type);
  MemoryCache* cache = MemoryCache::Get();
  KURL kurl((String(url)));
  KURL origin_url((String(origin)));
  Resource* resource_obj =
      CSSStyleSheetResource::CreateForOfflineResource(kurl, origin_url);
  const auto resource_maps_it =
      cache->resource_maps_.find(resource_obj->CacheIdentifier());
  EXPECT_NE(resource_maps_it, cache->resource_maps_.end());
}

TEST_F(WebCacheTest, AddResourceToCache002) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MakeRefCounted<base::TestSimpleTaskRunner>());
  base::SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner);
  std::string url = "https://example.com";
  std::string origin = "https://example2.com";
  std::vector<uint8_t> resource;
  resource.push_back(0);
  resource.push_back(1);
  base::flat_map<std::string, std::string> response_headers = {
      {"StreamType", "CRAS_STREAM_TYPE_DEFAULT"},
      {"ClientType", "CRAS_CLIENT_TYPE_CHROME"},
      {"NodeType", "USB_INTERNAL_SPEAKER"}};
  uint64_t type = 2;
  WebCache::AddResourceToCache(url, origin, resource, response_headers, type);
  MemoryCache* cache = MemoryCache::Get();
  OfflineResourceType resource_type = static_cast<OfflineResourceType>(type);
  KURL kurl((String(url)));
  KURL origin_url((String(origin)));
  ResourceResponse response =
      WebCache::GetResourceResponse(kurl, response_headers);
  Resource* resource_obj = ScriptResource::CreateForOfflineResource(
      kurl, origin_url, response,
      resource_type == OfflineResourceType::MODULE_JS);
  const auto resource_maps_it =
      cache->resource_maps_.find(resource_obj->CacheIdentifier());
  EXPECT_NE(resource_maps_it, cache->resource_maps_.end());
}

TEST_F(WebCacheTest, AddResourceToCache003) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MakeRefCounted<base::TestSimpleTaskRunner>());
  base::SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner);
  std::string url = "https://example.com";
  std::string origin = "https://example2.com";
  std::vector<uint8_t> resource;
  resource.push_back(0);
  resource.push_back(1);
  base::flat_map<std::string, std::string> response_headers = {
      {"StreamType", "CRAS_STREAM_TYPE_DEFAULT"},
      {"ClientType", "CRAS_CLIENT_TYPE_CHROME"},
      {"NodeType", "USB_INTERNAL_SPEAKER"}};
  uint64_t type = 3;
  WebCache::AddResourceToCache(url, origin, resource, response_headers, type);
  OfflineResourceType resource_type = static_cast<OfflineResourceType>(type);
  EXPECT_EQ(resource_type, OfflineResourceType::MODULE_JS);
}

TEST_F(WebCacheTest, AddResourceToCache004) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MakeRefCounted<base::TestSimpleTaskRunner>());
  base::SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner);
  std::string url = "https://example.com";
  std::string origin = "https://example2.com";
  std::vector<uint8_t> resource;
  resource.push_back(0);
  resource.push_back(1);
  base::flat_map<std::string, std::string> response_headers = {
      {"StreamType", "CRAS_STREAM_TYPE_DEFAULT"},
      {"ClientType", "CRAS_CLIENT_TYPE_CHROME"},
      {"NodeType", "USB_INTERNAL_SPEAKER"}};
  uint64_t type = 7;
  WebCache::AddResourceToCache(url, origin, resource, response_headers, type);
  OfflineResourceType resource_type = static_cast<OfflineResourceType>(type);
  EXPECT_NE(resource_type, OfflineResourceType::MODULE_JS);
}
#endif  // OHOS_UNITTESTS

}  // namespace blink
