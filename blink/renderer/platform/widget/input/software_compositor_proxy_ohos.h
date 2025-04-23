/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SOFTWARE_COMPOSITOR_PROXY_OHOS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SOFTWARE_COMPOSITOR_PROXY_OHOS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/writable_shared_memory_region.h"
#include "cc/mojo_embedder/software_compositor_renderer_ohos.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/input/software_compositor_ohos.mojom-blink.h"

namespace blink {

class SoftwareCompositorProxyOhos
    : public mojom::blink::SoftwareCompositorOhos {
 public:
  SoftwareCompositorProxyOhos();
  ~SoftwareCompositorProxyOhos() override;

  SoftwareCompositorProxyOhos(const SoftwareCompositorProxyOhos&) = delete;
  SoftwareCompositorProxyOhos& operator=(const SoftwareCompositorProxyOhos&) =
      delete;

  // blink::mojom::SoftwareCompositorOhos overrides.
  void DemandDrawSwAsync(
      mojom::blink::SoftwareCompositorDemandDrawSwParamsPtr params,
      DemandDrawSwAsyncCallback callback) final;
      
  // blink::mojom::SoftwareCompositorOhos overrides.    
  void DrawRect(const gfx::Rect& rect) final;

  void SetSharedMemory(base::WritableSharedMemoryRegion shm_region,
                       SetSharedMemoryCallback callback) final;

  void BindChannel(mojo::PendingReceiver<mojom::blink::SoftwareCompositorOhos>
                       compositor_request);
  void SetSoftwareRenderer(
      cc::mojo_embedder::SoftwareCompositorRendererOhos* software_render);

 private:
  struct SharedMemoryWithSize;
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;  // From browser.

  mojo::Receiver<mojom::blink::SoftwareCompositorOhos> receiver_{this};
  raw_ptr<cc::mojo_embedder::SoftwareCompositorRendererOhos> software_render_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SOFTWARE_COMPOSITOR_PROXY_OHOS_H_