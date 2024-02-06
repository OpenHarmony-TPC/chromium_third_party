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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_IMPL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/base/renderer_factory_selector.h"
#include "media/filters/native_pipeline_controller.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/media/web_native_delegate.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace cc {
class VideoLayer;
}

namespace gfx {
class Size;
}

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class VideoFrameCompositor;
class WebLocalFrame;
class WebNativeClient;

class BLINK_PLATFORM_EXPORT WebNativeBridgeImpl
    : public WebNativeBridge,
      public media::NativePipeline::Client,
      public WebNativeDelegate::Observer {
 public:
  WebNativeBridgeImpl(
      WebLocalFrame* frame,
      blink::WebNativeClient* client,
      blink::WebNativeDelegate* delegate,
      std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
      std::unique_ptr<VideoFrameCompositor> compositor,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner>
          video_frame_compositor_task_runner);
  WebNativeBridgeImpl(const WebNativeBridgeImpl&) = delete;
  WebNativeBridgeImpl& operator=(const WebNativeBridgeImpl&) = delete;
  ~WebNativeBridgeImpl() override;

  void StartPipeline() override;
  void OnTextureSizeChange(const gfx::Size& size) override;
  int GetDelegateId() override { return delegate_id_; }
  gfx::Size NaturalSize() const override;

  // Creates a Renderer via the |renderer_factory_selector_|. If the
  // |renderer_type| is absl::nullopt, create the base Renderer. Otherwise, set
  // the base type to be |renderer_type| and create a Renderer of that type.
  std::unique_ptr<media::Renderer> CreateRenderer(
      absl::optional<media::RendererType> renderer_type);

  void OnSurfaceCreated(media::TextureSizeChangedCB texture_size_changed_cb,
                        int native_embed_id);

  void OnSurfaceDestroyed();

  // media::NativePipeline::Client overrides.
  void OnSetLayer() override;

  // Called by the compositor the very first time a frame is received.
  void OnFirstFrame(base::TimeTicks frame_time);

  // TODO:WebNativeDelegate::Observer implementation.
  void OnFrameHidden() override {}
  void OnFrameShown() override {}

 private:
  WebLocalFrame* const frame_;
  // Task runner for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // |pipeline_controller_| owns an instance of Pipeline.
  std::unique_ptr<media::NativePipelineController> native_pipeline_controller_;

  media::TextureSizeChangedCB texture_size_changed_cb_;

  WebNativeClient* const client_;

  WebNativeDelegate* delegate_;
  int delegate_id_ = 0;

  // Video rendering members.
  // The |compositor_| runs on the compositor thread, or if
  // kEnableSurfaceLayerForVideo is enabled, the media thread. This task runner
  // posts tasks for the |compositor_| on the correct thread.
  scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner_;
  std::unique_ptr<VideoFrameCompositor>
      compositor_;  // Deleted on |vfc_task_runner_|.

  std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_refptr<cc::VideoLayer> video_layer_;

  // Whether the video is known to be opaque or not.
  bool opaque_ = true;
  gfx::Size surface_texture_size_;

  base::WeakPtr<WebNativeBridgeImpl> weak_this_;
  base::WeakPtrFactory<WebNativeBridgeImpl> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NATIVE_BRIDGE_IMPL_H_
