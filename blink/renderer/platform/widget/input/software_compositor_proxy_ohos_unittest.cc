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

#if defined(OHOS_UNITTESTS)
#define private public
#include "third_party/blink/renderer/platform/widget/input/software_compositor_proxy_ohos.h"
#undef private
#include "software_compositor_proxy_ohos.cc"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#else  // OHOS_UNITTESTS
#include "third_party/blink/renderer/platform/widget/input/software_compositor_proxy_ohos.h"
#endif  // OHOS_UNITTESTS

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"

#include "cc/mojo_embedder/software_compositor_renderer_ohos.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {
namespace {
std::shared_ptr<SoftwareCompositorProxyOhos> g_softwareCompositor;
}

using DemandDrawSwAsyncCallback = base::OnceCallback<void(bool)>;

class SoftwareCompositorProxyOhosTest : public testing::Test {
 public:
  static void SetUpTestCase(void);
  static void TearDownTestCase(void);
  void SetUp();
  void TearDown();
};

void SoftwareCompositorProxyOhosTest::SetUpTestCase(void) {}

void SoftwareCompositorProxyOhosTest::TearDownTestCase(void) {}

void SoftwareCompositorProxyOhosTest::SetUp(void) {
  g_softwareCompositor = std::make_shared<SoftwareCompositorProxyOhos>();
  ASSERT_NE(g_softwareCompositor, nullptr);
}

void SoftwareCompositorProxyOhosTest::TearDown(void) {
  g_softwareCompositor = nullptr;
}

class MockAsyncLayerTreeFrameSink
    : public cc::mojo_embedder::AsyncLayerTreeFrameSink {
 public:
  MockAsyncLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<cc::RasterContextProviderWrapper>
          worker_context_provider_wrapper,
      InitParams* params)
      : AsyncLayerTreeFrameSink(context_provider,
                                worker_context_provider_wrapper,
                                params) {}
  MockAsyncLayerTreeFrameSink(const MockAsyncLayerTreeFrameSink&) = delete;
  ~MockAsyncLayerTreeFrameSink() = default;
};

class MockSoftwareCompositorRegistryOhos
    : public cc::mojo_embedder::SoftwareCompositorRegistryOhos {
 public:
  MockSoftwareCompositorRegistryOhos() = default;
  ~MockSoftwareCompositorRegistryOhos() = default;
  void RegisterSoftwareRenderer(
      cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer);
  void UnregisterSoftwareRenderer(
      cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer);
};
void MockSoftwareCompositorRegistryOhos::RegisterSoftwareRenderer(
    cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer) {}
void MockSoftwareCompositorRegistryOhos::UnregisterSoftwareRenderer(
    cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer) {}

class WritableSharedMemoryRegionMock : public base::WritableSharedMemoryRegion {
 public:
  WritableSharedMemoryRegionMock() = default;
  WritableSharedMemoryRegionMock(const WritableSharedMemoryRegionMock&) =
      delete;
  ~WritableSharedMemoryRegionMock() = default;

  MOCK_METHOD0(IsValid, bool());
  MOCK_METHOD0(Map, base::WritableSharedMemoryMapping());
};

TEST_F(SoftwareCompositorProxyOhosTest, BindChannel) {
  mojo::PendingReceiver<mojom::blink::SoftwareCompositorOhos>
      compositor_request;

  g_softwareCompositor->BindChannel(std::move(compositor_request));
}

TEST_F(SoftwareCompositorProxyOhosTest, SetSharedMemory) {
  base::WritableSharedMemoryRegion shm_region =
      WritableSharedMemoryRegionMock::Create(1024 * 1024);
  bool callback_result = false;

  g_softwareCompositor->SetSharedMemory(
      std::move(shm_region),
      base::BindOnce(
          [](bool* out_result, bool result) { *out_result = result; },
          &callback_result));

  base::WritableSharedMemoryRegion invalid_shm_region;
  g_softwareCompositor->SetSharedMemory(
      std::move(invalid_shm_region),
      base::BindOnce(
          [](bool* out_result, bool result) { *out_result = result; },
          &callback_result));
}

TEST_F(SoftwareCompositorProxyOhosTest, SetSoftwareRenderer) {
  g_softwareCompositor->SetSoftwareRenderer(nullptr);
}

TEST_F(SoftwareCompositorProxyOhosTest, DrawRect) {
  g_softwareCompositor->SetSoftwareRenderer(nullptr);
  gfx::Rect rect(10, 10, 100, 100);
  EXPECT_DEATH(g_softwareCompositor->DrawRect(rect),
               "software render init error");
}

#if defined(OHOS_UNITTESTS)
TEST_F(SoftwareCompositorProxyOhosTest, DemandDrawSwAsync001) {
  gfx::SizeF size(800.0f, 600.0f);
  gfx::PointF offset(10.0f, 20.0f);
  DemandDrawSwAsyncCallback callback = base::BindOnce([](bool lost) {});
  mojom::blink::SoftwareCompositorDemandDrawSwParamsPtr params =
      mojom::blink::SoftwareCompositorDemandDrawSwParams::New(size, offset);
  size_t buffer_size = params->size.width() * params->size.height() * 4;
  base::WritableSharedMemoryMapping shm_mapping;
  g_softwareCompositor->software_draw_shm_ =
      std::make_unique<SoftwareCompositorProxyOhos::SharedMemoryWithSize>(
          std::move(shm_mapping), buffer_size);
  ASSERT_EQ(g_softwareCompositor->software_render_, nullptr);
  g_softwareCompositor->DemandDrawSwAsync(std::move(params),
                                          std::move(callback));
}
#endif  // OHOS_UNITTESTS

}  // namespace blink
