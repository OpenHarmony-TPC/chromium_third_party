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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NATIVE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NATIVE_LOADER_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/mojo/mojom/native_bridge.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_native_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote_set.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class Layer;
}

namespace blink {

class WebNativeBridge;
class HTMLPlugInElement;

class CORE_EXPORT NativeLoader
    : public GarbageCollected<NativeLoader>,
      public ExecutionContextLifecycleStateObserver,
      private WebNativeClient {
  USING_PRE_FINALIZER(NativeLoader, Dispose);

 public:
  explicit NativeLoader(HTMLPlugInElement* plugin_element);
  ~NativeLoader() override;

  void Trace(Visitor*) const override;

  cc::Layer* CcLayer() const;

  void ScheduleLoadResource();

  LocalFrame* CurrentFrame();

  WebNativeBridge* GetWebNativeBridge() { return web_native_bridge_.get(); }

  int GetNativeEmbedId() { return native_embed_id_; }

  HTMLPlugInElement* current_plugin_element() { return plugin_element_.Get(); }

  void Dispose();

  // WebNativeClient implementation.
  void OnCreateNativeSurface(int native_embed_id,
                             RectChangeCB rect_changed_cb) final;
  void OnLayerRectChange(const gfx::Rect& rect) final;
  void OnLayerRectVisibilityChange(bool visibility) final;
  void OnDestroyNativeSurface() final;
  void Repaint() final;
  void SetCcLayer(cc::Layer*) final;

  virtual String DebugName() const = 0;
 protected:
  // Assert the correct order of the children in shadow dom when DCHECK is on.
  static void AssertShadowRootChildren(ShadowRoot&);

  // Returns a constant reference to the HeapMojoAssociatedRemoteSet holding all
  // the bound remotes for the media::mojom::blink::MediaPlayerObserver
  // interface. Needed to allow sending messages directly from
  // NativeLoader's subclasses.
  const HeapMojoAssociatedRemoteSet<media::mojom::blink::NativeBridgeObserver>&
  GetNativeBridgeObserverRemoteSet() {
    return native_bridge_observer_remote_set_->Value();
  }

  void UpdateLayoutObject();

 private:
  // ExecutionContextLifecycleStateObserver functions.
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override {}
  void ContextDestroyed() override;

  // Returns a reference to the mojo remote for the NativeBridgeHost interface,
  // requesting it first from the BrowserInterfaceBroker if needed. It is an
  // error to call this method before having access to the document's frame.
  media::mojom::blink::NativeBridgeHost& GetNativeBridgeHostRemote();

  void LoadResource(LocalFrame* frame);
  void ClearNativeResource();

  void ResetMojoState();

  gfx::Rect TransformRect(gfx::Rect rect);

  String GetTypeAttribute() const;
  String GetSrcAttribute() const;
  String GetIdAttribute() const;
  String GetTagName() const;
  ParamMap GetParamList() const;

  // Adds a new NativeBridgeObserver remote that will be notified about native
  // bridge events and returns a receiver that an observer implementation can
  // bind to.
  mojo::PendingAssociatedReceiver<media::mojom::blink::NativeBridgeObserver>
  AddNativeBridgeObserverAndPassReceiver();

  cc::Layer* cc_layer_;

  Member<HTMLPlugInElement> plugin_element_;

  gfx::Rect bounding_rect_;

  RectChangeCB bounding_rect_changed_cb_;

  // Clean up things that are tied to any previous frame, including the player
  // and mojo interfaces, when we switch to a new frame.
  void AttachToNewFrame();

  std::unique_ptr<WebNativeBridge> web_native_bridge_;
  int native_embed_id_;
  bool visibility_ = false;
  bool first_update_visibility = false;
  bool cc_layer_update_ = false;

  Member<DisallowNewWrapper<
      HeapMojoAssociatedRemote<media::mojom::blink::NativeBridgeHost>>>
      native_bridge_host_remote_;

  // Multiple objects outside of the renderer process can register as observers,
  // so we need to store the remotes in a set here.
  Member<DisallowNewWrapper<
      HeapMojoAssociatedRemoteSet<media::mojom::blink::NativeBridgeObserver>>>
      native_bridge_observer_remote_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NATIVE_LOADER_H_
