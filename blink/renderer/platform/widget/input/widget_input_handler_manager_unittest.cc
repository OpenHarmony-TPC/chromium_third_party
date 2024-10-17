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

#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include <utility>
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.cc"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/mojo_embedder/software_compositor_registry_ohos.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/compositor_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/software_compositor_proxy_ohos.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_impl.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cc/raster/task.h"
#define protected public
#include "cc/trees/layer_tree_host_impl.h"
#undef protected
#include "third_party/skia/include/effects/SkHighContrastFilter.h"

namespace blink {
class SoftwareCompositorProxyRegistryOhos;
namespace {
std::shared_ptr<SoftwareCompositorProxyRegistryOhos> g_softwareCompositor;
WidgetBase* widget_;
auto weak_ptr_factory_ = base::WeakPtrFactory<WidgetBase>(widget_);
auto widget = weak_ptr_factory_.GetWeakPtr();
base::WeakPtr<mojom::blink::FrameWidgetInputHandler> frame_widget_input_handler;
scoped_refptr<scheduler::WidgetScheduler> widget_scheduler;
}  // namespace

class SoftwareCompositorProxyRegistryOhosTest : public testing::Test {
 public:
  static void SetUpTestCase(void);
  static void TearDownTestCase(void);
  void SetUp();
  void TearDown();
};
void SoftwareCompositorProxyRegistryOhosTest::SetUpTestCase(void) {}
void SoftwareCompositorProxyRegistryOhosTest::TearDownTestCase(void) {}
void SoftwareCompositorProxyRegistryOhosTest::SetUp(void) {
  scoped_refptr<base::SingleThreadTaskRunner>
      compositor_thread_default_task_runner_;
  g_softwareCompositor = std::make_shared<SoftwareCompositorProxyRegistryOhos>(
      compositor_thread_default_task_runner_);
  ASSERT_NE(g_softwareCompositor, nullptr);
}
void SoftwareCompositorProxyRegistryOhosTest::TearDown(void) {
  g_softwareCompositor = nullptr;
}

class LayerTreeHostImplClientMock : public cc::LayerTreeHostImplClient {
 public:
  LayerTreeHostImplClientMock() = default;
  ~LayerTreeHostImplClientMock() override = default;

  void DidLoseLayerTreeFrameSinkOnImplThread();
  void SetBeginFrameSource(viz::BeginFrameSource* source);
  void DidReceiveCompositorFrameAckOnImplThread();
  void OnCanDrawStateChanged(bool can_draw);
  void NotifyReadyToActivate();
  bool IsReadyToActivate();
  void NotifyReadyToDraw();
  void SetNeedsRedrawOnImplThread();
  void SetNeedsOneBeginImplFrameOnImplThread();
  void SetNeedsCommitOnImplThread();
  void SetNeedsPrepareTilesOnImplThread();
  void SetVideoNeedsBeginFrames(bool needs_begin_frames);
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame);
  bool IsInsideDraw();
  void RenewTreePriority();
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay);
  void DidActivateSyncTree();
  void WillPrepareTiles();
  void DidPrepareTiles();
  void DidCompletePageScaleAnimationOnImplThread();
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw);
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation);
  void NotifyImageDecodeRequestFinished();
  void NotifyTransitionRequestFinished(uint32_t sequence_id);
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      cc::PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
      const viz::FrameTimingDetails& details);
  void NotifyAnimationWorkletStateChange(
      cc::AnimationWorkletMutationState state,
      cc::ElementListType tree_type);
  void NotifyPaintWorkletStateChange(
      cc::SchedulerStateMachine::PaintWorkletState state);
  void NotifyThroughputTrackerResults(cc::CustomTrackerResults results);
  void DidObserveFirstScrollDelay(base::TimeDelta first_scroll_delay,
                                  base::TimeTicks first_scroll_timestamp);
  bool IsInSynchronousComposite() const;
  void FrameSinksToThrottleUpdated(const base::flat_set<viz::FrameSinkId>& ids);
  void ClearHistory();
  size_t CommitDurationSampleCountForTesting() const;
};
void LayerTreeHostImplClientMock::DidLoseLayerTreeFrameSinkOnImplThread() {}
void LayerTreeHostImplClientMock::SetBeginFrameSource(
    viz::BeginFrameSource* source) {}
void LayerTreeHostImplClientMock::DidReceiveCompositorFrameAckOnImplThread() {}
void LayerTreeHostImplClientMock::OnCanDrawStateChanged(bool can_draw) {}
void LayerTreeHostImplClientMock::NotifyReadyToActivate() {}
bool LayerTreeHostImplClientMock::IsReadyToActivate() {
  return true;
}
void LayerTreeHostImplClientMock::NotifyReadyToDraw() {}
void LayerTreeHostImplClientMock::SetNeedsRedrawOnImplThread() {}
void LayerTreeHostImplClientMock::SetNeedsOneBeginImplFrameOnImplThread() {}
void LayerTreeHostImplClientMock::SetNeedsCommitOnImplThread() {}
void LayerTreeHostImplClientMock::SetNeedsPrepareTilesOnImplThread() {}
void LayerTreeHostImplClientMock::SetVideoNeedsBeginFrames(
    bool needs_begin_frames) {}
void LayerTreeHostImplClientMock::SetDeferBeginMainFrameFromImpl(
    bool defer_begin_main_frame) {}
bool LayerTreeHostImplClientMock::IsInsideDraw() {
  return true;
}
void LayerTreeHostImplClientMock::RenewTreePriority() {}
void LayerTreeHostImplClientMock::PostDelayedAnimationTaskOnImplThread(
    base::OnceClosure task,
    base::TimeDelta delay) {}
void LayerTreeHostImplClientMock::DidActivateSyncTree() {}
void LayerTreeHostImplClientMock::WillPrepareTiles() {}
void LayerTreeHostImplClientMock::DidPrepareTiles() {}
void LayerTreeHostImplClientMock::DidCompletePageScaleAnimationOnImplThread() {}
void LayerTreeHostImplClientMock::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {}
void LayerTreeHostImplClientMock::NeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {}
void LayerTreeHostImplClientMock::NotifyImageDecodeRequestFinished() {}
void LayerTreeHostImplClientMock::NotifyTransitionRequestFinished(
    uint32_t sequence_id) {}
void LayerTreeHostImplClientMock::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    cc::PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
    const viz::FrameTimingDetails& details) {}
void LayerTreeHostImplClientMock::NotifyAnimationWorkletStateChange(
    cc::AnimationWorkletMutationState state,
    cc::ElementListType tree_type) {}
void LayerTreeHostImplClientMock::NotifyPaintWorkletStateChange(
    cc::SchedulerStateMachine::PaintWorkletState state) {}
void LayerTreeHostImplClientMock::NotifyThroughputTrackerResults(
    cc::CustomTrackerResults results) {}
void LayerTreeHostImplClientMock::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {}
bool LayerTreeHostImplClientMock::IsInSynchronousComposite() const {
  return true;
}
void LayerTreeHostImplClientMock::FrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {}
void LayerTreeHostImplClientMock::ClearHistory() {}
size_t LayerTreeHostImplClientMock::CommitDurationSampleCountForTesting()
    const {
  return 0;
}

class TaskGraphRunnerMock : public cc::TaskGraphRunner {
 public:
  TaskGraphRunnerMock() = default;
  ~TaskGraphRunnerMock() override = default;

  cc::NamespaceToken GenerateNamespaceToken();
  void ScheduleTasks(cc::NamespaceToken token, cc::TaskGraph* graph);
  void WaitForTasksToFinishRunning(cc::NamespaceToken token);
  void CollectCompletedTasks(cc::NamespaceToken token,
                             cc::Task::Vector* completed_tasks);
};
cc::NamespaceToken TaskGraphRunnerMock::GenerateNamespaceToken() {
  auto token = std::make_unique<cc::NamespaceToken>();
  return *token;
}
void TaskGraphRunnerMock::ScheduleTasks(cc::NamespaceToken token,
                                        cc::TaskGraph* graph) {}
void TaskGraphRunnerMock::WaitForTasksToFinishRunning(
    cc::NamespaceToken token) {}
void TaskGraphRunnerMock::CollectCompletedTasks(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {}

class RasterDarkModeFilterMock : public cc::RasterDarkModeFilter {
 public:
  RasterDarkModeFilterMock() = default;
  ~RasterDarkModeFilterMock() override = default;

  sk_sp<SkColorFilter> ApplyToImage(const SkPixmap& pixmap,
                                    const SkIRect& src) const override;
};
sk_sp<SkColorFilter> RasterDarkModeFilterMock::ApplyToImage(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  SkHighContrastConfig config;
  auto filter = SkHighContrastFilter::Make(config);
  return filter;
}

class LayerTreeHostSchedulingClientMock
    : public cc::LayerTreeHostSchedulingClient {
 public:
  LayerTreeHostSchedulingClientMock() = default;
  virtual ~LayerTreeHostSchedulingClientMock() = default;
  void DidRunBeginMainFrame();
};
void LayerTreeHostSchedulingClientMock::DidRunBeginMainFrame() {
  return;
}

class CompositorDelegateForInputMock : public cc::CompositorDelegateForInput {
 public:
  CompositorDelegateForInputMock() = default;
  ~CompositorDelegateForInputMock() override = default;
  void BindToInputHandler(
      std::unique_ptr<cc::InputDelegateForCompositor> delegate);
  cc::ScrollTree& GetScrollTree() const;
  bool HasAnimatedScrollbars() const;
  void SetNeedsCommit();
  void SetNeedsFullViewportRedraw();
  void SetDeferBeginMainFrame(bool defer_begin_main_frame) const;
  void DidUpdateScrollAnimationCurve();
  void AccumulateScrollDeltaForTracing(const gfx::Vector2dF& delta);
  void DidStartPinchZoom();
  void DidUpdatePinchZoom();
  void DidEndPinchZoom();
  void DidStartScroll();
  void DidEndScroll();
  void DidMouseLeave();
  bool IsInHighLatencyMode() const;
  void WillScrollContent(cc::ElementId element_id);
  void DidScrollContent(cc::ElementId element_id, bool animated);
  float DeviceScaleFactor() const;
  float PageScaleFactor() const;
  gfx::Size VisualDeviceViewportSize() const;
  const cc::LayerTreeSettings& GetSettings() const;
  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate);
  bool HasScrollLinkedAnimation(cc::ElementId for_scroller) const;
  cc::LayerTreeHostImpl& GetImplDeprecated();
  const cc::LayerTreeHostImpl& GetImplDeprecated() const;
};
void CompositorDelegateForInputMock::BindToInputHandler(
    std::unique_ptr<cc::InputDelegateForCompositor> delegate) {}
cc::ScrollTree& CompositorDelegateForInputMock::GetScrollTree() const {
  auto scrollTree = std::make_shared<cc::ScrollTree>();
  return *scrollTree;
}
bool CompositorDelegateForInputMock::HasAnimatedScrollbars() const {
  return false;
}
void CompositorDelegateForInputMock::SetNeedsCommit() {}
void CompositorDelegateForInputMock::SetNeedsFullViewportRedraw() {}
void CompositorDelegateForInputMock::SetDeferBeginMainFrame(
    bool defer_begin_main_frame) const {}
void CompositorDelegateForInputMock::DidUpdateScrollAnimationCurve() {}
void CompositorDelegateForInputMock::AccumulateScrollDeltaForTracing(
    const gfx::Vector2dF& delta) {}
void CompositorDelegateForInputMock::DidStartPinchZoom() {}
void CompositorDelegateForInputMock::DidUpdatePinchZoom() {}
void CompositorDelegateForInputMock::DidEndPinchZoom() {}
void CompositorDelegateForInputMock::DidStartScroll() {}
void CompositorDelegateForInputMock::DidEndScroll() {}
void CompositorDelegateForInputMock::DidMouseLeave() {}
bool CompositorDelegateForInputMock::IsInHighLatencyMode() const {
  return false;
}
void CompositorDelegateForInputMock::WillScrollContent(
    cc::ElementId element_id) {}
void CompositorDelegateForInputMock::DidScrollContent(cc::ElementId element_id,
                                                      bool animated) {}
float CompositorDelegateForInputMock::DeviceScaleFactor() const {
  return 1.0;
}
float CompositorDelegateForInputMock::PageScaleFactor() const {
  return 1.0;
}
gfx::Size CompositorDelegateForInputMock::VisualDeviceViewportSize() const {
  return gfx::Size();
}
const cc::LayerTreeSettings& CompositorDelegateForInputMock::GetSettings()
    const {
  static cc::LayerTreeSettings settings;
  return settings;
}
void CompositorDelegateForInputMock::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {}
bool CompositorDelegateForInputMock::HasScrollLinkedAnimation(
    cc::ElementId for_scroller) const {
  return false;
}
cc::LayerTreeHostImpl& CompositorDelegateForInputMock::GetImplDeprecated() {
  cc::LayerTreeSettings settings;
  auto layer_tree_host_impl = std::make_shared<cc::LayerTreeHostImpl>(
      settings, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 1,
      nullptr, nullptr);
  return *layer_tree_host_impl;
}
const cc::LayerTreeHostImpl& CompositorDelegateForInputMock::GetImplDeprecated()
    const {
  cc::LayerTreeSettings settings;
  auto layer_tree_host_impl = std::make_shared<cc::LayerTreeHostImpl>(
      settings, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 1,
      nullptr, nullptr);
  return *layer_tree_host_impl;
}

class SoftwareCompositorRegistryOhosMock
    : public cc::mojo_embedder::SoftwareCompositorRegistryOhos {
 public:
  SoftwareCompositorRegistryOhosMock() = default;
  ~SoftwareCompositorRegistryOhosMock() = default;
  void RegisterSoftwareRenderer(
      cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer);
  void UnregisterSoftwareRenderer(
      cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer);
};
void SoftwareCompositorRegistryOhosMock::RegisterSoftwareRenderer(
    cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer) {}
void SoftwareCompositorRegistryOhosMock::UnregisterSoftwareRenderer(
    cc::mojo_embedder::SoftwareCompositorRendererOhos* software_renderer) {}

TEST_F(SoftwareCompositorProxyRegistryOhosTest, CreateProxy) {
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams initParams;
  auto sink_ = std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
      nullptr, nullptr, &initParams);

  auto registry_ = std::make_shared<SoftwareCompositorRegistryOhosMock>();
  auto renderer_ =
      std::make_shared<cc::mojo_embedder::SoftwareCompositorRendererOhos>(
          sink_.get(), registry_.get());

  g_softwareCompositor->RegisterSoftwareRenderer(renderer_.get());
  g_softwareCompositor->CreateProxy(nullptr);
  EXPECT_NE(g_softwareCompositor->proxy(), nullptr);

  g_softwareCompositor->RegisterSoftwareRenderer(nullptr);
  g_softwareCompositor->CreateProxy(nullptr);
}

TEST_F(SoftwareCompositorProxyRegistryOhosTest, RegisterSoftwareRenderer) {
  g_softwareCompositor->CreateProxy(nullptr);
  EXPECT_NE(g_softwareCompositor->proxy(), nullptr);
  g_softwareCompositor->RegisterSoftwareRenderer(nullptr);

  g_softwareCompositor->DestroyProxy();
  EXPECT_EQ(g_softwareCompositor->proxy(), nullptr);
  g_softwareCompositor->RegisterSoftwareRenderer(nullptr);
}

TEST_F(SoftwareCompositorProxyRegistryOhosTest, UnregisterSoftwareRenderer) {
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams initParams;
  auto sink_ = std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
      nullptr, nullptr, &initParams);

  auto registry_ = std::make_shared<SoftwareCompositorRegistryOhosMock>();
  auto renderer_ =
      std::make_shared<cc::mojo_embedder::SoftwareCompositorRendererOhos>(
          sink_.get(), registry_.get());

  g_softwareCompositor->RegisterSoftwareRenderer(renderer_.get());
  g_softwareCompositor->UnregisterSoftwareRenderer(renderer_.get());
}

TEST_F(SoftwareCompositorProxyRegistryOhosTest, DestroyProxy) {
  g_softwareCompositor->CreateProxy(nullptr);
  EXPECT_NE(g_softwareCompositor->proxy(), nullptr);

  g_softwareCompositor->DestroyProxy();
}
}  // namespace blink