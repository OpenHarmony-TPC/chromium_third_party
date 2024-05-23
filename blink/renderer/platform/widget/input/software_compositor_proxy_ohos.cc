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

#include "third_party/blink/renderer/platform/widget/input/software_compositor_proxy_ohos.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {

SoftwareCompositorProxyOhos::SoftwareCompositorProxyOhos() {}

SoftwareCompositorProxyOhos::~SoftwareCompositorProxyOhos() {
  software_draw_shm_.reset();
}

struct SoftwareCompositorProxyOhos::SharedMemoryWithSize {
  base::WritableSharedMemoryMapping shared_memory;
  const size_t buffer_size;
  bool zeroed;

  SharedMemoryWithSize(base::WritableSharedMemoryMapping shm_mapping,
                       size_t buffer_size)
      : shared_memory(std::move(shm_mapping)),
        buffer_size(buffer_size),
        zeroed(true) {}
};

void SoftwareCompositorProxyOhos::DemandDrawSwAsync(
    mojom::blink::SoftwareCompositorDemandDrawSwParamsPtr params,
    DemandDrawSwAsyncCallback callback) {
  LOG(INFO) << "DemandDrawSwAsync in blink";

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params->size.width(), params->size.height());
  size_t stride = info.minRowBytes();

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shared_memory.memory(),
                            stride)) {
    LOG(ERROR) << "bit map installPixels error";
    std::move(callback).Run(false);
    return;
  }
  SkCanvas canvas(bitmap);
  if (!software_render_) {
    LOG(ERROR) << "software render init error";
    std::move(callback).Run(false);
    return;
  }
  software_render_->DemandDrawSw(&canvas, params->size, params->offset);
  std::move(callback).Run(true);
}

void SoftwareCompositorProxyOhos::BindChannel(
    mojo::PendingReceiver<mojom::blink::SoftwareCompositorOhos>
        compositor_request) {
  receiver_.Bind(std::move(compositor_request));
}

void SoftwareCompositorProxyOhos::SetSharedMemory(
    base::WritableSharedMemoryRegion shm_region,
    SetSharedMemoryCallback callback) {
  bool result = false;
  if (shm_region.IsValid()) {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    if (shm_mapping.IsValid()) {
      software_draw_shm_ = std::make_unique<SharedMemoryWithSize>(
          std::move(shm_mapping), shm_mapping.size());
      result = true;
    }
  }
  std::move(callback).Run(result);
}

void SoftwareCompositorProxyOhos::SetSoftwareRenderer(
    cc::mojo_embedder::SoftwareCompositorRendererOhos* software_render) {
  software_render_ = software_render;
}
}  // namespace blink