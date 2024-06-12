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

#include "third_party/blink/renderer/platform/web_native_bridge_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "cc/layers/video_layer.h"
#include "media/base/native_pipeline_impl.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/media/video_frame_compositor.h"
#include "third_party/blink/public/platform/web_native_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {

namespace {

// Handles destruction of media::Renderer dependent components after the
// renderer has been destructed on the media thread.
void DestructionHelper(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector) {
  // Since the media::Renderer is gone we can now destroy the compositor and
  // renderer factory selector.
  vfc_task_runner->DeleteSoon(FROM_HERE, std::move(compositor));
  main_task_runner->DeleteSoon(FROM_HERE, std::move(renderer_factory_selector));
}

}  // namespace

WebNativeBridgeImpl::WebNativeBridgeImpl(
    WebLocalFrame* frame,
    WebNativeClient* client,
    WebNativeDelegate* delegate,
    std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
    std::unique_ptr<VideoFrameCompositor> compositor,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner>
        video_frame_compositor_task_runner)
    : frame_(frame),
      main_task_runner_(frame->GetTaskRunner(TaskType::kMediaElementEvent)),
      media_task_runner_(std::move(media_task_runner)),
      client_(client),
      delegate_(delegate),
      vfc_task_runner_(std::move(video_frame_compositor_task_runner)),
      compositor_(std::move(compositor)),
      renderer_factory_selector_(std::move(renderer_factory_selector)) {
  DCHECK(renderer_factory_selector_);
  DCHECK(client_);
  DCHECK(delegate_);
  DCHECK(frame_);

  weak_this_ = weak_factory_.GetWeakPtr();

  // Using base::Unretained(this) is safe because the `pipeline` is owned by
  // `this` and the callback will always be made on the main task runner.
  // Not using BindToCurrentLoop() because CreateRenderer() is a sync call.
  auto native_pipeline = std::make_unique<media::NativePipelineImpl>(
      media_task_runner_, main_task_runner_,
      base::BindRepeating(&WebNativeBridgeImpl::CreateRenderer,
                          base::Unretained(this)));

  native_pipeline_controller_ =
      std::make_unique<media::NativePipelineController>(
          std::move(native_pipeline));
  delegate_id_ = delegate_->AddObserver(this);
}

WebNativeBridgeImpl::~WebNativeBridgeImpl() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG(DEBUG) << "[NativeEmbed] ~WebNativeBridgeImpl.";

  // delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);
  delegate_ = nullptr;

  // The underlying Pipeline must be stopped before it is destroyed.
  //
  // Note: This destruction happens synchronously on the media thread and
  // `compositor_`, must outlive this process. They will be destructed
  // by the DestructionHelper below after trampolining through the media thread.
  native_pipeline_controller_->Stop();

  // Destruct compositor resources in the proper order.
  client_->SetCcLayer(nullptr);

  if (video_layer_) {
    video_layer_->StopUsingProvider();
    video_layer_->ResetLayerRectUpdateCallback();
    LOG(DEBUG) << "[NativeEmbed] ResetLayerRectUpdateCallback.";
  }

  // Handle destruction of things that need to be destructed after the pipeline
  // completes stopping on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DestructionHelper, std::move(main_task_runner_),
                     std::move(vfc_task_runner_), std::move(compositor_),
                     std::move(renderer_factory_selector_)));
}

std::unique_ptr<media::Renderer> WebNativeBridgeImpl::CreateRenderer(
    absl::optional<media::RendererType> renderer_type) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (renderer_type) {
    renderer_factory_selector_->SetBaseRendererType(renderer_type.value());
  }

  return renderer_factory_selector_->GetCurrentFactory()->CreateRenderer(
      media_task_runner_, nullptr, nullptr, compositor_.get(),
      base::NullCallback(), client_->TargetColorSpace());
}

void WebNativeBridgeImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "[NativeEmbed] WebNativeBridgeImpl::StartPipeline.";
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetOnNewProcessedFrameCallback,
                     base::Unretained(compositor_.get()),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &WebNativeBridgeImpl::OnFirstFrame, weak_this_))));

  media::CreateTextureCB create_texture_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&WebNativeBridgeImpl::OnSurfaceCreated, weak_this_));
  media::DestroyTextureCB destroy_texture_cb =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&WebNativeBridgeImpl::OnSurfaceDestroyed, weak_this_));

  native_pipeline_controller_->Start(this, std::move(create_texture_cb),
                                     std::move(destroy_texture_cb));
}

void WebNativeBridgeImpl::OnFirstFrame(base::TimeTicks frame_time) {
  if (client_) {
    client_->Repaint();
  }
}

void WebNativeBridgeImpl::OnSurfaceCreated(media::RectChangedCB rect_changed_cb,
                                           int native_embed_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "[NativeEmbed] WebNativeBridgeImpl::OnSurfaceCreated";

  client_->OnCreateNativeSurface(native_embed_id, rect_changed_cb);
}

void WebNativeBridgeImpl::OnSurfaceDestroyed() {
  LOG(INFO) << "[NativeEmbed] WebNativeBridgeImpl::OnSurfaceDestroyed";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->OnDestroyNativeSurface();
}

void WebNativeBridgeImpl::OnLayerRectChange(const gfx::Rect& rect) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG(DEBUG) << "[NativeEmbed] OnLayerRectChange.";

  client_->OnLayerRectChange(rect);

  layer_rect_ = rect;
}

gfx::Size WebNativeBridgeImpl::NaturalSize() const {
  return layer_rect_.size();
}

void WebNativeBridgeImpl::OnSetLayer() {
  LOG(INFO) << "[NativeEmbed] WebNativeBridgeImpl::OnSetLayer";
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!video_layer_);
  media::WebRectChangedCB rect_change_cb = base::BindRepeating(
      &WebNativeBridgeImpl::OnLayerRectChange, base::Unretained(this));

  video_layer_ = cc::VideoLayer::Create(
      compositor_.get(), media::kNoTransformation, std::move(rect_change_cb));
  client_->SetCcLayer(video_layer_.get());
}

}  // namespace blink
