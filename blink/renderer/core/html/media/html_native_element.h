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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_NATIVE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_NATIVE_ELEMENT_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/mojo/mojom/native_bridge.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
#include "third_party/blink/public/platform/web_native_client.h"

namespace cc {
class Layer;
}

namespace blink {

class Event;
class EventQueue;
class ExceptionState;
class MediaError;
class ScriptState;
class WebNativeBridge;

class CORE_EXPORT HTMLNativeElement
    : public HTMLElement,
      public Supplementable<HTMLNativeElement>,
      public ActiveScriptWrappable<HTMLNativeElement>,
      public ExecutionContextLifecycleStateObserver,
      private WebNativeClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(HTMLNativeElement, Dispose);

 public:
  explicit HTMLNativeElement(Document&);
  ~HTMLNativeElement() override;

  // TODO: Returne the acture size.
  unsigned videoWidth() const { return 100; }
  unsigned videoHeight() const { return 100; }

  void Trace(Visitor*) const override;

  cc::Layer* CcLayer() const;

  enum DelayedActionType {
    kLoadMediaResource = 1 << 0
  };
  // error state
  MediaError* error() const;

  enum NetworkState {
    kNetworkEmpty,
    kNetworkIdle,
    kNetworkLoading,
    kNetworkNoSource
  };
  NetworkState getNetworkState() const;

  void load();

  // EventTarget function.
  // Both Node (via HTMLElement) and ExecutionContextLifecycleStateObserver
  // define this method, which causes an ambiguity error at compile time. This
  // class's constructor ensures that both implementations return document, so
  // return the result of one of them here.
  using HTMLElement::GetExecutionContext;

  bool IsFullscreen() const;

  // ScriptWrappable functions.
  bool HasPendingActivity() const override;

  void ScheduleEvent(Event*);

  LocalFrame* LocalFrameForNative();

  void LoadForWebNative(LocalFrame* frame);

  WebNativeBridge* GetWebNativeBridge() { return web_native_bridge_.get(); }

  // WebNativeClient implementation.
  void OnCreateNativeSurface(int native_embed_id) final;
  void OnDestroyNativeSurface() final;
  void Repaint() final;
  void SizeChanged(const gfx::Size& size) final;
  void SetCcLayer(cc::Layer*) final;
  int GetNativeEmbedId();
 protected:
  // Assert the correct order of the children in shadow dom when DCHECK is on.
  static void AssertShadowRootChildren(ShadowRoot&);

  void Dispose();

  // Returns a constant reference to the HeapMojoAssociatedRemoteSet holding all
  // the bound remotes for the media::mojom::blink::MediaPlayerObserver
  // interface. Needed to allow sending messages directly from
  // HTMLNativeElement's subclasses.
  const HeapMojoAssociatedRemoteSet<media::mojom::blink::NativeBridgeObserver>&
  GetNativeBridgeObserverRemoteSet() {
    return native_bridge_observer_remote_set_->Value();
  }

  void ParseAttribute(const AttributeModificationParams&) override;
  void FinishParsingChildren() final;
  bool IsURLAttribute(const Attribute&) const override;
  void AttachLayoutTree(AttachContext&) override;
  void ParserDidSetAttributes() override;
  void CloneNonAttributePropertiesFrom(const Element&,
                                       CloneChildrenFlag) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void DidMoveToNewDocument(Document& old_document) override;

  void UpdateLayoutObject();

 private:
  bool HasPendingActivityInternal() const;

  bool AlwaysCreateUserAgentShadowRoot() const final { return true; }
  bool AreAuthorShadowsAllowed() const final { return false; }

  bool SupportsFocus() const final;
  bool IsMouseFocusable() const final;
  bool LayoutObjectIsNeeded(const DisplayStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  void DidNotifySubtreeInsertionsToDocument() override {}
  void DidRecalcStyle(const StyleRecalcChange) final;

  bool CanStartSelection() const override { return false; }

  // ExecutionContextLifecycleStateObserver functions.
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override {}
  void ContextDestroyed() override;

  // Returns a reference to the mojo remote for the NativeBridgeHost interface,
  // requesting it first from the BrowserInterfaceBroker if needed. It is an
  // error to call this method before having access to the document's frame.
  media::mojom::blink::NativeBridgeHost& GetNativeBridgeHostRemote();

  void LoadTimerFired(TimerBase*);
  void StopPeriodicTimers();

  // FIXME: Rename to scheduleNamedEvent for clarity.
  void ScheduleEvent(const AtomicString& event_name);

  // loading
  void InvokeLoadAlgorithm();
  void LoadResource();
  void ScheduleLoadResource();
  void ClearNativeResource();
  void ClearResourceWithoutLocking();
  void NoneSupported(const String&);
  void MediaEngineError(MediaError*);
  void CancelPendingEventsAndCallbacks();

  void SetShouldDelayLoadEvent(bool);

  void SetNetworkState(NetworkState, bool update_media_controls = true);

  void SetError(MediaError* error);

  void ResetMojoState();

  String GetTypeAttribute();

  String GetSrcAttribute();

  String GetIdAttribute();

  String GetTagName();

  ParamMap GetParamList();

  // Adds a new NativeBridgeObserver remote that will be notified about native
  // bridge events and returns a receiver that an observer implementation can
  // bind to.
  mojo::PendingAssociatedReceiver<media::mojom::blink::NativeBridgeObserver>
  AddNativeBridgeObserverAndPassReceiver();

  // Timers used to schedule one-shot tasks with no delay.
  HeapTaskRunnerTimer<HTMLNativeElement> load_timer_;

  Member<EventQueue> async_event_queue_;

  NetworkState network_state_;

  // To prevent potential regression when extended by the MSE API, do not set
  // |error_| outside of constructor and SetError().
  Member<MediaError> error_;

  cc::Layer* cc_layer_;

  typedef unsigned PendingActionFlags;
  PendingActionFlags pending_action_flags_;

  // FIXME: HTMLNativeElement has way too many state bits.
  bool should_delay_load_event_ : 1;
  bool have_fired_loaded_data_ : 1;

  // data has not been loaded since sending a "stalled" event
  bool sent_stalled_event_ : 1;

  bool processing_preference_change_ : 1;

  // Notify HTMLNativeElement when a document's ExecutionContext is destroyed.
  // It allows us to disconnect from a previous document's frame if we were
  // using it to support our WebMediaPlayer rather than our current frame.
  class OpenerContextObserver final
      : public GarbageCollected<OpenerContextObserver>,
        public ContextLifecycleObserver {
   public:
    // Notify `element` when our context is destroyed.
    explicit OpenerContextObserver(HTMLNativeElement* element);
    ~OpenerContextObserver() final;

    void Trace(Visitor* visitor) const final;

   protected:
    void ContextDestroyed() final;

    Member<HTMLNativeElement> element_;
  };

  // Clean up things that are tied to any previous frame, including the player
  // and mojo interfaces, when we switch to a new frame.
  void AttachToNewFrame();

  Member<Document> opener_document_;
  Member<OpenerContextObserver> opener_context_observer_;

  std::unique_ptr<WebNativeBridge> web_native_bridge_;
  int native_embed_id_;

  friend class Internals;

  Member<IntersectionObserver> lazy_load_intersection_observer_;

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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_NATIVE_ELEMENT_H_
