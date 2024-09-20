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

#include <algorithm>
#include <limits>

#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "media/mojo/mojom/native_bridge.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/layout_native.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/native_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

float PageConstraintInitalScale(const Document& document) {
  float scale = 1.0;
  if (auto* page = document.GetPage()) {
    scale = page->GetPageScaleConstraintsSet().FinalConstraints().initial_scale;
  }
  return scale;
}

}  // anonymous namespace

NativeLoader::NativeLoader(HTMLPlugInElement* plugin_element)
    : ExecutionContextLifecycleStateObserver(GetExecutionContext()),
      cc_layer_(nullptr),
      plugin_element_(plugin_element) {
  LOG(INFO) << "[NativeEmbed] NativeLoader::NativeLoader element "
            << plugin_element_->ToString();
  ResetMojoState();

  CurrentFrame()->GetFrameScheduler()->RegisterStickyFeature(
    SchedulingPolicy::Feature::kEnableCacheNativeEmbed, {SchedulingPolicy::DisableBackForwardCache()});
}

NativeLoader::~NativeLoader() {
  LOG(INFO) << "[NativeEmbed] NativeLoader::~NativeLoader ";
}

void NativeLoader::Dispose() {
  LOG(INFO) << "[NativeEmbed] NativeLoader::Dispose " << this;
  ClearNativeResource();
}

void NativeLoader::AttachToNewFrame() {
  // Reset mojo state that is coupled to |old_document|'s execution context.
  // NOTE: |native_bridge_host_remote_| is also coupled to |old_document|'s
  // frame.
  ResetMojoState();
  ScheduleLoadResource();
}

void NativeLoader::ResetMojoState() {
  native_bridge_host_remote_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemote<media::mojom::blink::NativeBridgeHost>>>(
      GetExecutionContext());
  if (native_bridge_observer_remote_set_) {
    native_bridge_observer_remote_set_->Value().Clear();
  }
  native_bridge_observer_remote_set_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemoteSet<media::mojom::blink::NativeBridgeObserver>>>(
      GetExecutionContext());
}

String NativeLoader::GetTypeAttribute() const {
  return plugin_element_->TypeAttribute();
}

String NativeLoader::GetSrcAttribute() const {
  return plugin_element_->SrcAttribute();
}

String NativeLoader::GetIdAttribute() const {
  return plugin_element_->IdAttribute();
}

String NativeLoader::GetTagName() const {
  return plugin_element_->tagName();
}

ParamMap NativeLoader::GetParamList() const {
  return plugin_element_->ParamList();
}

LocalFrame* NativeLoader::CurrentFrame() {
  return plugin_element_->GetDocument().GetFrame();
}

void NativeLoader::ScheduleLoadResource() {
  LOG(INFO) << "NativeEmbed NativeLoader::ScheduleLoadResource";
  LoadResource(CurrentFrame());
}

void NativeLoader::LoadResource(LocalFrame* frame) {
  LOG(INFO) << "NativeEmbed NativeLoader::LoadResource";
  DCHECK(frame);
  web_native_bridge_ = frame->Client()->CreateWebNativeBridge(*this, this);
  if (!web_native_bridge_) {
    return;
  }

  GetNativeBridgeHostRemote().OnNativeBridgeAdded(
      AddNativeBridgeObserverAndPassReceiver(),
      web_native_bridge_->GetDelegateId());

  web_native_bridge_->StartPipeline();
}

void NativeLoader::OnCreateNativeSurface(int native_embed_id,
                                         RectChangeCB rect_changed_cb) {
  LOG(INFO) << "[NativeEmbed] NativeLoader::OnCreateNativeSurface";
  if (!native_bridge_observer_remote_set_ || !cc_layer_) {
    return;
  }

  if (bounding_rect_.IsEmpty()) {
    plugin_element_->GetDocument().UpdateStyleAndLayoutForNode(
        plugin_element_, DocumentUpdateReason::kPlugin);
    bounding_rect_ = plugin_element_->PixelSnappedBoundingBox();    
    if (auto* layout_object = plugin_element_->GetLayoutObject()) {
      if (const auto& replaced = To<LayoutReplaced>(layout_object)) {
        bounding_rect_.set_size(ToPixelSnappedRect(replaced->ReplacedContentRect()).size());
      }
    }
    if (bounding_rect_.IsEmpty()) {
      first_update_rect_ = false;
    }
  }
  LOG(INFO) << "NativeEmbed NativeLoader::OnCreateNativeSurface:"
            << bounding_rect_.ToString();
            
  native_embed_id_ = native_embed_id;
  bounding_rect_changed_cb_ = rect_changed_cb;
  cc_layer_->SetNativeEmbedId(native_embed_id_);

  auto embed_info = media::mojom::blink::NativeEmbedInfo::New();
  auto* frame = CurrentFrame();
  if (frame && frame->View()) {
    auto bounds_to_viewport =
        frame->View()->FrameToViewport(bounding_rect_);
    // We will use the position relative to visual viewport.
    bounding_rect_.set_origin(bounds_to_viewport.origin());
    embed_info->rect = bounds_to_viewport;
    if (!bounding_rect_changed_cb_.is_null()) {
      bounding_rect_changed_cb_.Run(bounds_to_viewport);
    }
  }

  embed_info->embed_id = native_embed_id_;
  embed_info->type = GetTypeAttribute().IsNull() ? "" : GetTypeAttribute();
  embed_info->element_id = GetIdAttribute().IsNull() ? "" : GetIdAttribute();
  embed_info->source = GetSrcAttribute().IsNull() ? "" : GetSrcAttribute();
  embed_info->tag = GetTagName().IsNull() ? "" : GetTagName();
  if (!GetParamList().empty()) {
    embed_info->params = GetParamList();
  }

  for (auto& observer : native_bridge_observer_remote_set_->Value()) {
    // TODO: We actually only have one observer now so just using std::move
    // here.
    observer->OnCreateNativeSurface(std::move(embed_info));
  }

  if (first_update_visibility) {
    OnLayerRectVisibilityChange(visibility_);
  }
}

void NativeLoader::OnLayerRectVisibilityChange(bool visibility) {
  visibility_ = visibility;
  first_update_visibility = true;
  if(native_embed_id_ != -1) {
    for (auto& observer : native_bridge_observer_remote_set_->Value()) {
      observer->OnLayerRectVisibilityChange(visibility_, native_embed_id_);
    }
  }
}

void NativeLoader::OnLayerRectChange(const gfx::Rect& rect) {
  if (first_update_rect_) {
    first_update_rect_ = false;
    return;
  }

  if (bounding_rect_.ApproximatelyEqual(rect, 1) ||
      !native_bridge_observer_remote_set_) {
    return;
  }
 
  if (bounding_rect_.size() != rect.size()) {
    bounding_rect_ = rect;
    if (!bounding_rect_changed_cb_.is_null()) {
      bounding_rect_changed_cb_.Run(gfx::ScaleToEnclosingRect(
          bounding_rect_, PageConstraintInitalScale(plugin_element_->GetDocument())));
    }
  } else {
    bounding_rect_.set_origin(rect.origin());
  }
  LOG(INFO) << "NativeEmbed NativeLoader::OnLayerRectChange:"
            << bounding_rect_.ToString();
  for (auto& observer : native_bridge_observer_remote_set_->Value()) {
    observer->OnEmbedRectChange(gfx::Rect(
        bounding_rect_.origin(),
        gfx::ScaleToCeiledSize(
            bounding_rect_.size(),
            PageConstraintInitalScale(plugin_element_->GetDocument()))));
  }
}

void NativeLoader::OnDestroyNativeSurface() {
  LOG(INFO) << "[NativeEmbed] NativeLoader::OnDestroyNativeSurface";
  bounding_rect_changed_cb_.Reset();
  if (!native_bridge_observer_remote_set_) {
    return;
  }

  for (auto& observer : native_bridge_observer_remote_set_->Value()) {
    observer->OnDestroyNativeSurface();
  }
}

void NativeLoader::Repaint() {
  if (cc_layer_) {
    cc_layer_->SetNeedsDisplay();
  }

  if (auto* layout_object = plugin_element_->GetLayoutObject()) {
    layout_object->SetShouldDoFullPaintInvalidation();
  }
}

void NativeLoader::SetCcLayer(cc::Layer* cc_layer) {
  LOG(INFO) << "[NativeEmbed] NativeLoader::SetCcLayer";
  if (cc_layer == cc_layer_) {
    return;
  }

  plugin_element_->SetNeedsCompositingUpdate();
  cc_layer_ = cc_layer;
  if (cc_layer_) {
    LOG(DEBUG) << "[NativeEmbed] set native flag SetNativeType:"
               << GetTypeAttribute();
    cc_layer_->SetMayContainNative(true);
    cc_layer_->SetNeedsPushProperties();
  }
}

void NativeLoader::ClearNativeResource() {
  LOG(INFO) << "NativeEmbed NativeLoader::ClearNativeResource";

  if (web_native_bridge_) {
    web_native_bridge_.reset();
    native_bridge_observer_remote_set_->Value().Clear();
  }

  if (auto* layout_object = plugin_element_->GetLayoutObject()) {
    layout_object->SetShouldDoFullPaintInvalidation();
  }
}

void NativeLoader::ContextDestroyed() {
  DVLOG(3) << "contextDestroyed(" << static_cast<void*>(this) << ")";
  LOG(INFO) << "[NativeEmbed] NativeLoader::ContextDestroyed";
  // Clear everything in the Media Element
  ClearNativeResource();
}

cc::Layer* NativeLoader::CcLayer() const {
  return cc_layer_;
}

void NativeLoader::Trace(Visitor* visitor) const {
  visitor->Trace(native_bridge_host_remote_);
  visitor->Trace(native_bridge_observer_remote_set_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

media::mojom::blink::NativeBridgeHost&
NativeLoader::GetNativeBridgeHostRemote() {
  // It is an error to call this before having access to the document's frame.
  auto* frame = CurrentFrame();
  DCHECK(frame);
  if (!native_bridge_host_remote_->Value().is_bound()) {
    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        native_bridge_host_remote_->Value().BindNewEndpointAndPassReceiver(
            plugin_element_->GetDocument().GetTaskRunner(
                TaskType::kInternalMedia)));
  }
  return *native_bridge_host_remote_->Value().get();
}

mojo::PendingAssociatedReceiver<media::mojom::blink::NativeBridgeObserver>
NativeLoader::AddNativeBridgeObserverAndPassReceiver() {
  mojo::PendingAssociatedRemote<media::mojom::blink::NativeBridgeObserver>
      observer;
  auto observer_receiver = observer.InitWithNewEndpointAndPassReceiver();
  native_bridge_observer_remote_set_->Value().Add(
      std::move(observer),
      plugin_element_->GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  return observer_receiver;
}

}  // namespace blink
