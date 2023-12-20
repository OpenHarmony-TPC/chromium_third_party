/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include "third_party/blink/renderer/platform/web_native_bridge_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/video_layer.h"
#include "media/base/native_pipeline_impl.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/media/video_frame_compositor.h"
#include "third_party/blink/public/platform/web_native_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/gfx/geometry/size.h"

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
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces,
    const scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>&
        video_frame_compositor_task_runner)
    : frame_(frame),
      main_task_runner_(frame->GetTaskRunner(TaskType::kMediaElementEvent)),
      media_task_runner_(media_task_runner),
      client_(client),
      delegate_(delegate),
      vfc_task_runner_(video_frame_compositor_task_runner),
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

  OnSurfaceDestroyed();

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

  if (video_layer_)
    video_layer_->StopUsingProvider();

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
  media::DestroyTextureCB destroy_texture_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&WebNativeBridgeImpl::OnSurfaceDestroyed, weak_this_));

  native_pipeline_controller_->Start(this, std::move(create_texture_cb),
                                     std::move(destroy_texture_cb));
}

void WebNativeBridgeImpl::OnFirstFrame(base::TimeTicks frame_time) {
  if (client_) {
    client_->Repaint();
  }
}

void WebNativeBridgeImpl::OnSurfaceCreated(
    media::TextureSizeChangedCB texture_size_changed_cb,
    int native_embed_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  LOG(INFO) << "[NativeEmbed] WebNativeBridgeImpl::OnSurfaceCreated.";
  // A null callback indicates that the decoder is going away.
  if (texture_size_changed_cb.is_null()) {
    texture_size_changed_cb_.Reset();
    return;
  }

  texture_size_changed_cb_ = std::move(texture_size_changed_cb);
  client_->OnCreateNativeSurface(native_embed_id);
}

void WebNativeBridgeImpl::OnSurfaceDestroyed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->OnDestroyNativeSurface();
}

void WebNativeBridgeImpl::OnTextureSizeChange(const gfx::Size& size) {
  surface_texture_size_ = size;
  if (!texture_size_changed_cb_.is_null()) {
    std::move(texture_size_changed_cb_).Run(size);
  }
}

gfx::Size WebNativeBridgeImpl::NaturalSize() const {
  return surface_texture_size_;
}

void WebNativeBridgeImpl::OnSetLayer() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  DCHECK(!video_layer_);
  video_layer_ =
      cc::VideoLayer::Create(compositor_.get(), media::kNoTransformation);
  video_layer_->SetContentsOpaque(opaque_);
  client_->SetCcLayer(video_layer_.get());
}

}  // namespace blink
