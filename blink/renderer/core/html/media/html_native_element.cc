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

#include "third_party/blink/renderer/core/html/media/html_native_element.h"

#include <algorithm>
#include <limits>

#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/media_error.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_native.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"

namespace blink {

using WeakNativeElementSet = HeapHashSet<WeakMember<HTMLNativeElement>>;
using DocumentElementSetMap =
    HeapHashMap<WeakMember<Document>, Member<WeakNativeElementSet>>;

namespace {

DocumentElementSetMap& DocumentToElementSetMap() {
  DEFINE_STATIC_LOCAL(Persistent<DocumentElementSetMap>, map,
                      (MakeGarbageCollected<DocumentElementSetMap>()));
  return *map;
}

void AddElementToDocumentMap(HTMLNativeElement* element, Document* document) {
  DocumentElementSetMap& map = DocumentToElementSetMap();
  WeakNativeElementSet* set = nullptr;
  auto it = map.find(document);
  if (it == map.end()) {
    set = MakeGarbageCollected<WeakNativeElementSet>();
    map.insert(document, set);
  } else {
    set = it->value;
  }
  set->insert(element);
}

void RemoveElementFromDocumentMap(HTMLNativeElement* element,
                                  Document* document) {
  DocumentElementSetMap& map = DocumentToElementSetMap();
  auto it = map.find(document);
  DCHECK(it != map.end());
  WeakNativeElementSet* set = it->value;
  set->erase(element);
  if (set->empty())
    map.erase(it);
}

std::ostream& operator<<(std::ostream& stream,
                         HTMLNativeElement const& media_element) {
  return stream << static_cast<void const*>(&media_element);
}

}  // anonymous namespace

HTMLNativeElement::HTMLNativeElement(Document& document)
    : HTMLElement(html_names::kNativeTag, document),
      ActiveScriptWrappable<HTMLNativeElement>({}),
      ExecutionContextLifecycleStateObserver(GetExecutionContext()),
      load_timer_(document.GetTaskRunner(TaskType::kInternalMedia),
                  this,
                  &HTMLNativeElement::LoadTimerFired),
      async_event_queue_(
          MakeGarbageCollected<EventQueue>(GetExecutionContext(),
                                           TaskType::kMediaElementEvent)),
      network_state_(kNetworkEmpty),
      cc_layer_(nullptr),
      pending_action_flags_(0),
      should_delay_load_event_(false),
      have_fired_loaded_data_(false),
      sent_stalled_event_(false),
      processing_preference_change_(false),
      lazy_load_intersection_observer_(nullptr) {
  ResetMojoState();

  if (document.LocalOwner()) {
    localOwner_ = document.LocalOwner();
    native_type_ = document.LocalOwner()->NativeType();
    native_source_ = document.LocalOwner()->NativeSource();
    embed_element_id_ = document.LocalOwner()->IdAttribute();
    LOG(INFO)
        << "[NativeEmbed] ConstructHTMLNativeElement native type = "
        << native_type_ << ", native source = " << native_source_ << ", embed element id = " << embed_element_id_;
  }

  SetHasCustomStyleCallbacks();
  AddElementToDocumentMap(this, &document);
}

HTMLNativeElement::~HTMLNativeElement() {
  DVLOG(1) << "~HTMLNativeElement(" << *this << ")";
}

HTMLFrameOwnerElement* HTMLNativeElement::GetLocalOwner() {
  return localOwner_;
}

void HTMLNativeElement::Dispose() {
  ClearResourceWithoutLocking();
}

void HTMLNativeElement::DidMoveToNewDocument(Document& old_document) {
  DVLOG(3) << "didMoveToNewDocument(" << *this << ")";

  load_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  if (should_delay_load_event_) {
    GetDocument().IncrementLoadEventDelayCount();
    // Note: Keeping the load event delay count increment on oldDocument that
    // was added when should_delay_load_event_ was set so that destruction of
    // web_media_player_ can not cause load event dispatching in oldDocument.
  } else {
    // Incrementing the load event delay count so that destruction of
    // web_media_player_ can not cause load event dispatching in oldDocument.
    old_document.IncrementLoadEventDelayCount();
  }

  RemoveElementFromDocumentMap(this, &old_document);
  AddElementToDocumentMap(this, &GetDocument());
  SetExecutionContext(GetExecutionContext());

  // FIXME: This is a temporary fix to prevent this object from causing the
  // MediaPlayer to dereference LocalFrame and FrameLoader pointers from the
  // previous document. This restarts the load, as if the src attribute had been
  // set.  A proper fix would provide a mechanism to allow this object to
  // refresh the MediaPlayer's LocalFrame and FrameLoader references on document
  // changes so that playback can be resumed properly.
  // TODO(liberato): Consider checking that the new document's opener is the old
  // document: GetDocument().GetFrame()->Opener() == old_document.GetFrame().
  auto new_origin = GetDocument().TopFrameOrigin();
  auto old_origin = old_document.TopFrameOrigin();
  const bool reuse =
      new_origin &&
      old_origin && old_origin->IsSameOriginWith(new_origin.get());
  if (!reuse) {
    // Don't worry about notifications from any previous document if we're not
    // re-using the player.
    if (opener_context_observer_)
      opener_context_observer_->SetContextLifecycleNotifier(nullptr);
    AttachToNewFrame();
  } else {
    opener_document_ = old_document;
    if (!opener_context_observer_) {
      opener_context_observer_ =
          MakeGarbageCollected<OpenerContextObserver>(this);
    }
    opener_context_observer_->SetContextLifecycleNotifier(
        opener_document_->GetExecutionContext());
  }

  // Decrement the load event delay count on oldDocument now that
  // web_media_player_ has been destroyed and there is no risk of dispatching a
  // load event from within the destructor.
  old_document.DecrementLoadEventDelayCount();

  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLNativeElement::AttachToNewFrame() {
  opener_document_ = nullptr;
  // Do not ask it to stop notifying us -- if this is a callback from the
  // listener, then it's ExecutionContext has been destroyed and it's not
  // allowed to unregister.
  opener_context_observer_ = nullptr;
  // Reset mojo state that is coupled to |old_document|'s execution context.
  // NOTE: |native_bridge_host_remote_| is also coupled to |old_document|'s
  // frame.
  ResetMojoState();
  InvokeLoadAlgorithm();
}

void HTMLNativeElement::ResetMojoState() {
  native_bridge_host_remote_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemote<media::mojom::blink::NativeBridgeHost>>>(
      GetExecutionContext());
  if (native_bridge_observer_remote_set_)
    native_bridge_observer_remote_set_->Value().Clear();
  native_bridge_observer_remote_set_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemoteSet<media::mojom::blink::NativeBridgeObserver>>>(
      GetExecutionContext());
}

bool HTMLNativeElement::SupportsFocus() const {
  // If no controls specified, we should still be able to focus the element if
  // it has tabIndex.
  return HTMLElement::SupportsFocus();
}

bool HTMLNativeElement::IsMouseFocusable() const {
  return !IsFullscreen() && SupportsFocus();
}

void HTMLNativeElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);
}

void HTMLNativeElement::ParserDidSetAttributes() {
  HTMLElement::ParserDidSetAttributes();
}

// This method is being used as a way to know that cloneNode finished cloning
// attribute as there is no callback notifying about the end of a cloning
// operation. Indeed, it is required per spec to set the muted state based on
// the content attribute when the object is created.
void HTMLNativeElement::CloneNonAttributePropertiesFrom(const Element& other,
                                                       CloneChildrenFlag flag) {
  HTMLElement::CloneNonAttributePropertiesFrom(other, flag);
}

void HTMLNativeElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
}

bool HTMLNativeElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLNativeElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutNative>(this);
}

Node::InsertionNotificationRequest HTMLNativeElement::InsertedInto(
    ContainerNode& insertion_point) {
  DVLOG(3) << "insertedInto(" << *this << ", " << insertion_point << ")";

  HTMLElement::InsertedInto(insertion_point);
  if (insertion_point.isConnected()) {
    if ((!FastGetAttribute(html_names::kSrcAttr).empty()) &&
        network_state_ == kNetworkEmpty) {
      InvokeLoadAlgorithm();
    }
  }

  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLNativeElement::RemovedFrom(ContainerNode& insertion_point) {
  DVLOG(3) << "removedFrom(" << *this << ", " << insertion_point << ")";
  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLNativeElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  UpdateLayoutObject();
}

void HTMLNativeElement::DidRecalcStyle(const StyleRecalcChange change) {
  if (!change.ReattachLayoutTree())
    UpdateLayoutObject();
}

void HTMLNativeElement::ScheduleLoadResource() {
  // Schedule the timer to try the next <source> element WITHOUT resetting state
  // ala invokeLoadAlgorithm.
  pending_action_flags_ |= kLoadMediaResource;
  load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void HTMLNativeElement::ScheduleEvent(const AtomicString& event_name) {
  Event* event = Event::CreateCancelable(event_name);
  event->SetTarget(this);
  ScheduleEvent(event);
}

void HTMLNativeElement::ScheduleEvent(Event* event) {
  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

void HTMLNativeElement::LoadTimerFired(TimerBase*) {
  DVLOG(3) << "LoadTimerFired(" << *this << ")";
  if (pending_action_flags_ & kLoadMediaResource) {
    // 6 - If the media element has an assigned media provider object, then let
    //     mode be object.

    // 7 - Set the media element's networkState to NETWORK_LOADING.
    SetNetworkState(kNetworkLoading);

    // 8 - Queue a task to fire a simple event named loadstart at the media
    // element.
    ScheduleEvent(event_type_names::kLoadstart);

    // 9 - Run the appropriate steps...
    LoadResource();
  }

  pending_action_flags_ = 0;
}

MediaError* HTMLNativeElement::error() const {
  return error_;
}

HTMLNativeElement::NetworkState HTMLNativeElement::getNetworkState() const {
  return network_state_;
}

void HTMLNativeElement::load() {
  DVLOG(1) << "load(" << *this << ")";

  InvokeLoadAlgorithm();
}

void HTMLNativeElement::InvokeLoadAlgorithm() {
  DVLOG(3) << "invokeLoadAlgorithm(" << *this << ")";

  // Perform the cleanup required for the resource load algorithm to run.
  StopPeriodicTimers();
  load_timer_.Stop();
  // FIXME: Figure out appropriate place to reset LoadTextTrackResource if
  // necessary and set pending_action_flags_ to 0 here.
  pending_action_flags_ &= ~kLoadMediaResource;
  sent_stalled_event_ = false;
  have_fired_loaded_data_ = false;

  CancelPendingEventsAndCallbacks();

  if (network_state_ == kNetworkLoading || network_state_ == kNetworkIdle)
    ScheduleEvent(event_type_names::kAbort);

  // If the element's networkState is not set to NETWORK_EMPTY, then
  // run these substeps
  if (network_state_ != kNetworkEmpty) {
    // Queue a task to fire a simple event named emptied at the media
    // element.
    ScheduleEvent(event_type_names::kEmptied);

    // If a fetching process is in progress for the media element, the
    // user agent should stop it.
    SetNetworkState(kNetworkEmpty);
  }

  SetError(nullptr);

  SetNetworkState(kNetworkNoSource);

  SetShouldDelayLoadEvent(false);

  ScheduleLoadResource();
}

void HTMLNativeElement::LoadResource() {
  DCHECK(IsMainThread());
  // The resource fetch algorithm
  SetNetworkState(kNetworkLoading);

  DCHECK(!error_);

  if (auto* frame = LocalFrameForNative()) {
    LoadForWebNative(frame);
  }
}

LocalFrame* HTMLNativeElement::LocalFrameForNative() {
  return opener_document_ ? opener_document_->GetFrame()
                          : GetDocument().GetFrame();
}

void HTMLNativeElement::LoadForWebNative(LocalFrame* frame) {
  LOG(INFO) << "[NativeEmbed] HTMLNativeElement::LoadForWebNative";
  web_native_bridge_ = frame->Client()->CreateWebNativeBridge(*this, this);
  if (!web_native_bridge_)
    return;

  GetNativeBridgeHostRemote().OnNativeBridgeAdded(AddNativeBridgeObserverAndPassReceiver(),
      web_native_bridge_->GetDelegateId());

  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();

  web_native_bridge_->StartPipeline();
}

void HTMLNativeElement::NoneSupported(const String& input_message) {
  DVLOG(3) << "NoneSupported(" << *this << ", message='" << input_message
           << "')";

  StopPeriodicTimers();

  const String& message = input_message;

  SetError(MakeGarbageCollected<MediaError>(
      MediaError::kMediaErrSrcNotSupported, message));

  SetNetworkState(kNetworkNoSource);

  ScheduleEvent(event_type_names::kError);

  SetShouldDelayLoadEvent(false);

  UpdateLayoutObject();
}

void HTMLNativeElement::MediaEngineError(MediaError* err) {
  DVLOG(3) << "mediaEngineError(" << *this << ", "
           << static_cast<int>(err->code()) << ")  message:"
           << static_cast<AtomicString>(err->message()).Ascii();

  StopPeriodicTimers();

  SetError(err);

  ScheduleEvent(event_type_names::kError);

  SetNetworkState(kNetworkIdle);

  SetShouldDelayLoadEvent(false);
}

void HTMLNativeElement::CancelPendingEventsAndCallbacks() {
  DVLOG(3) << "cancelPendingEventsAndCallbacks(" << *this << ")";
  async_event_queue_->CancelAllEvents();
}

void HTMLNativeElement::UpdateLayoutObject() {
  if (GetLayoutObject())
    GetLayoutObject()->UpdateFromElement();
}

void HTMLNativeElement::OnCreateNativeSurface(int native_embed_id) {
  LOG(INFO) << "[NativeEmbed] HTMLNativeElement::OnCreateNativeSurface.";
  auto bounding_size = PixelSnappedBoundingBox().size();
  if (auto* layout_view = GetDocument().GetLayoutView()) {
    bounding_size = layout_view->GetLayoutSize();
  }
  native_embed_id_ = native_embed_id;
  bounding_size = gfx::ScaleToCeiledSize(bounding_size, 1 / GetDocument().DevicePixelRatio());
  for (auto& observer : native_bridge_observer_remote_set_->Value()) {
    observer->UpdateElementId(embed_element_id_);
    observer->UpdateElementSource(native_source_);
    observer->OnCreateNativeSurface(
        native_embed_id, bounding_size, native_type_);
  }

  web_native_bridge_->OnTextureSizeChange(bounding_size);
}

void HTMLNativeElement::
    ClearResourceWithoutLocking() {
  if (web_native_bridge_) {
    web_native_bridge_.reset();
    native_bridge_observer_remote_set_->Value().Clear();
  }
}

void HTMLNativeElement::OnDestroyNativeSurface() {
  if (!native_bridge_observer_remote_set_)
    return;

  for (auto& observer : native_bridge_observer_remote_set_->Value()) {
    observer->OnDestroyNativeSurface();
  }
}

void HTMLNativeElement::Repaint() {
  if (cc_layer_)
    cc_layer_->SetNeedsDisplay();

  UpdateLayoutObject();
  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

void HTMLNativeElement::SizeChanged(const gfx::Size& size) {
  ScheduleEvent(event_type_names::kResize);
  LOG(INFO) << "[NativeEmbed] HTMLNativeElement::SizeChanged size " << size.ToString();
  UpdateLayoutObject();
  if (web_native_bridge_) {
    auto bounding_size = gfx::ScaleToCeiledSize(size, 1 / GetDocument().DevicePixelRatio());
    web_native_bridge_->OnTextureSizeChange(bounding_size);

    for (auto& observer : native_bridge_observer_remote_set_->Value()) {
      observer->OnEmbedSizeChange(bounding_size);
    }
  }
}

void HTMLNativeElement::SetCcLayer(cc::Layer* cc_layer) {
  if (cc_layer == cc_layer_)
    return;

  SetNeedsCompositingUpdate();
  cc_layer_ = cc_layer;
}

void HTMLNativeElement::StopPeriodicTimers() {
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->disconnect();
    lazy_load_intersection_observer_ = nullptr;
  }
}

void HTMLNativeElement::ClearNativeResource() {
  StopPeriodicTimers();
  load_timer_.Stop();

  pending_action_flags_ = 0;

  ClearResourceWithoutLocking();

  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

void HTMLNativeElement::ContextDestroyed() {
  DVLOG(3) << "contextDestroyed(" << static_cast<void*>(this) << ")";

  // Close the async event queue so that no events are enqueued.
  CancelPendingEventsAndCallbacks();

  // Clear everything in the Media Element
  ClearNativeResource();
  SetNetworkState(kNetworkEmpty);
  SetShouldDelayLoadEvent(false);

  UpdateLayoutObject();

  StopPeriodicTimers();
}

bool HTMLNativeElement::HasPendingActivity() const {
  const auto result = HasPendingActivityInternal();
  // TODO(dalecurtis): Replace c-style casts in followup patch.
  DVLOG(3) << "HasPendingActivity(" << *this << ") = " << result;
  return result;
}

bool HTMLNativeElement::HasPendingActivityInternal() const {
  // The delaying-the-load-event flag is set by resource selection algorithm
  // when looking for a resource to load, before networkState has reached to
  // kNetworkLoading.
  if (should_delay_load_event_)
    return true;

  // Wait for any pending events to be fired.
  if (async_event_queue_->HasPendingEvents())
    return true;

  return false;
}

bool HTMLNativeElement::IsFullscreen() const {
  return Fullscreen::IsFullscreenElement(*this);
}

cc::Layer* HTMLNativeElement::CcLayer() const {
  return cc_layer_;
}

bool HTMLNativeElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLNativeElement::SetShouldDelayLoadEvent(bool should_delay) {
  if (should_delay_load_event_ == should_delay)
    return;

  should_delay_load_event_ = should_delay;
  if (should_delay)
    GetDocument().IncrementLoadEventDelayCount();
  else
    GetDocument().DecrementLoadEventDelayCount();
}

void HTMLNativeElement::Trace(Visitor* visitor) const {
  visitor->Trace(load_timer_);
  visitor->Trace(async_event_queue_);
  visitor->Trace(error_);
  visitor->Trace(lazy_load_intersection_observer_);
  visitor->Trace(native_bridge_host_remote_);
  visitor->Trace(native_bridge_observer_remote_set_);
  visitor->Trace(opener_document_);
  visitor->Trace(opener_context_observer_);
  Supplementable<HTMLNativeElement>::Trace(visitor);
  HTMLElement::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void HTMLNativeElement::SetNetworkState(NetworkState state,
                                       bool update_media_controls) {
  if (network_state_ == state)
    return;

  network_state_ = state;
}

media::mojom::blink::NativeBridgeHost&
HTMLNativeElement::GetNativeBridgeHostRemote() {
  // It is an error to call this before having access to the document's frame.
  DCHECK(GetDocument().GetFrame());
  if (!native_bridge_host_remote_->Value().is_bound()) {
    GetDocument()
        .GetFrame()
        ->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(
            native_bridge_host_remote_->Value().BindNewEndpointAndPassReceiver(
                GetDocument().GetTaskRunner(TaskType::kInternalMedia)));
  }
  return *native_bridge_host_remote_->Value().get();
}

mojo::PendingAssociatedReceiver<media::mojom::blink::NativeBridgeObserver>
HTMLNativeElement::AddNativeBridgeObserverAndPassReceiver() {
  mojo::PendingAssociatedRemote<media::mojom::blink::NativeBridgeObserver>
      observer;
  auto observer_receiver = observer.InitWithNewEndpointAndPassReceiver();
  native_bridge_observer_remote_set_->Value().Add(
      std::move(observer),
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  return observer_receiver;
}

void HTMLNativeElement::SetError(MediaError* error) {
  error_ = error;

  if (!error)
    return;
}

HTMLNativeElement::OpenerContextObserver::OpenerContextObserver(
    HTMLNativeElement* element)
    : element_(element) {}

HTMLNativeElement::OpenerContextObserver::~OpenerContextObserver() = default;

void HTMLNativeElement::OpenerContextObserver::Trace(Visitor* visitor) const {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(element_);
}

void HTMLNativeElement::OpenerContextObserver::ContextDestroyed() {
  element_->AttachToNewFrame();
}

int HTMLNativeElement::GetNativeEmbedId() {
  return native_embed_id_;
}

}  // namespace blink