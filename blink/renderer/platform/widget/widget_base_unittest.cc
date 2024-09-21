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

#include <memory>
#include <tuple>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#undef private

#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/renderer/platform/widget/widget_base.cc"

#if BUILDFLAG(IS_OHOS)
#include "base/process/process_handle.h"
#include "base/task/post_job.h"
#include "base/task/thread_pool/job_task_source.h"
#include "base/task/thread_pool/thread_group_impl.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/worker_thread.h"
#include "content/child/child_thread_impl.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/res_sched_client_adapter.h"
#endif

namespace blink {
class MockMessageReceiverWithResponder
    : public mojo::MessageReceiverWithResponder {
 public:
  MOCK_METHOD(bool,
              AcceptWithResponder,
              (mojo::Message*, std::unique_ptr<MessageReceiver>),
              (override));
  MOCK_METHOD(bool, Accept, (mojo::Message*), (override));
};

class MockWidget : public mojom::blink::Widget {
 public:
  MOCK_METHOD(void, ForceRedraw, (ForceRedrawCallback), (override));
  MOCK_METHOD(
      void,
      GetWidgetInputHandler,
      (mojo::PendingReceiver<blink::mojom::blink::WidgetInputHandler>,
       mojo::PendingRemote<blink::mojom::blink::WidgetInputHandlerHost>),
      (override));
  MOCK_METHOD(void,
              UpdateVisualProperties,
              (const blink::VisualProperties&),
              (override));
  MOCK_METHOD(void,
              UpdateScreenRects,
              (const gfx::Rect&, const gfx::Rect&, UpdateScreenRectsCallback),
              (override));
  MOCK_METHOD(void, WasHidden, (), (override));
  MOCK_METHOD(void,
              WasShown,
              (bool, blink::mojom::blink::RecordContentToVisibleTimeRequestPtr),
              (override));
  MOCK_METHOD(void,
              RequestSuccessfulPresentationTimeForNextFrame,
              (blink::mojom::blink::RecordContentToVisibleTimeRequestPtr),
              (override));
  MOCK_METHOD(void, CancelSuccessfulPresentationTimeRequest, (), (override));
};

class WidgetBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    options_ = std::make_unique<WidgetBase>(
        client, std::move(widget_host), std::move(widget), nullptr, hidden,
        never_composited, is_embedded, is_for_scalable_page);
  }
  void TearDown() override { options_.reset(); }
  WidgetBaseClient* client;
  CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase> widget_host;
  CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget;
  bool hidden;
  bool never_composited;
  bool is_embedded;
  bool is_for_scalable_page;
  std::unique_ptr<WidgetBase> options_;
};

TEST_F(WidgetBaseTest, SetZoomLevel) {
  float magnify_delta = 0.0;
  const gfx::Point anchor;
  options_->SetZoomLevel(magnify_delta, anchor);
  EXPECT_FALSE(options_->widget_input_handler_manager_);
}

TEST_F(WidgetBaseTest, SetOverscrollMode) {
  int mode = 0;
  options_->SetOverscrollMode(mode);
  EXPECT_EQ(options_->widget_input_handler_manager_, nullptr);
}

TEST_F(WidgetBaseTest, GetOverScrollOffset) {
  gfx::Vector2dF overscroll_offset;
  overscroll_offset.set_x(0.0f);
  overscroll_offset.set_y(0.0f);
  auto result = options_->GetOverScrollOffset();
  EXPECT_EQ(overscroll_offset, result);
}

TEST_F(WidgetBaseTest, TouchHitTest) {
  WebPointerEvent event;
  size_t i = 0;
  options_->TouchHitTest(event, i);
  auto result = options_->client_->FrameWidget();
  EXPECT_FALSE(result);
}

TEST_F(WidgetBaseTest, NativeHitTestResult) {
  bool isNative = false;
  size_t fingerId = 0;
  int layerId = 0;
  options_->NativeHitTestResult(isNative, fingerId, layerId);
  EXPECT_FALSE(options_->widget_input_handler_manager_);
}

#if BUILDFLAG(IS_OHOS)
TEST_F(WidgetBaseTest, DidNativeEmbedEvent01) {
  blink::WebInputEvent::Type type =
      blink::WebInputEvent::Type::kGesturePinchTypeFirst;
  std::string embedId = "embedID";
  int32_t id = 0;
  float x = 1.0;
  float y = 1.0;
  std::unique_ptr<MockMessageReceiverWithResponder> mock_receiver =
      std::make_unique<MockMessageReceiverWithResponder>();
  EXPECT_CALL(*mock_receiver, Accept(testing::_))
      .WillOnce(testing::Return(false));
  std::unique_ptr<mojo::MessageReceiverWithResponder> receiver =
      std::move(mock_receiver);
  mojo::internal::AssociatedInterfacePtrState<mojom::blink::WidgetHost>
      widget_host = mojo::internal::AssociatedInterfacePtrState<
          mojom::blink::WidgetHost>();
  widget_host.proxy_ =
      std::make_unique<mojom::blink::WidgetHostProxy>(receiver.get());
  widget_host.proxy_->receiver_ = receiver.get();
  options_->widget_host_.internal_state_.Swap(&widget_host);
  options_->DidNativeEmbedEvent(type, embedId, id, x, y);
  ASSERT_EQ(type, blink::WebInputEvent::Type::kGesturePinchTypeFirst);
}
#endif

#if BUILDFLAG(IS_OHOS)
TEST_F(WidgetBaseTest, DidNativeEmbedEvent02) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kTouchMove;
  std::string embedId = "embedID";
  int32_t id = 0;
  float x = 1.0;
  float y = 1.0;
  std::unique_ptr<MockMessageReceiverWithResponder> mock_receiver =
      std::make_unique<MockMessageReceiverWithResponder>();
  EXPECT_CALL(*mock_receiver, Accept(testing::_))
      .WillOnce(testing::Return(false));
  std::unique_ptr<mojo::MessageReceiverWithResponder> receiver =
      std::move(mock_receiver);
  mojo::internal::AssociatedInterfacePtrState<mojom::blink::WidgetHost>
      widget_host = mojo::internal::AssociatedInterfacePtrState<
          mojom::blink::WidgetHost>();
  widget_host.proxy_ =
      std::make_unique<mojom::blink::WidgetHostProxy>(receiver.get());
  widget_host.proxy_->receiver_ = receiver.get();
  options_->widget_host_.internal_state_.Swap(&widget_host);
  options_->DidNativeEmbedEvent(type, embedId, id, x, y);
  ASSERT_EQ(type, blink::WebInputEvent::Type::kTouchMove);
}
#endif

#if BUILDFLAG(IS_OHOS)
TEST_F(WidgetBaseTest, DidNativeEmbedEvent03) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kTouchStart;
  std::string embedId = "embedID";
  int32_t id = 0;
  float x = 1.0;
  float y = 1.0;
  std::unique_ptr<MockMessageReceiverWithResponder> mock_receiver =
      std::make_unique<MockMessageReceiverWithResponder>();
  EXPECT_CALL(*mock_receiver, Accept(testing::_))
      .WillOnce(testing::Return(false));
  std::unique_ptr<mojo::MessageReceiverWithResponder> receiver =
      std::move(mock_receiver);
  mojo::internal::AssociatedInterfacePtrState<mojom::blink::WidgetHost>
      widget_host = mojo::internal::AssociatedInterfacePtrState<
          mojom::blink::WidgetHost>();
  widget_host.proxy_ =
      std::make_unique<mojom::blink::WidgetHostProxy>(receiver.get());
  widget_host.proxy_->receiver_ = receiver.get();
  options_->widget_host_.internal_state_.Swap(&widget_host);
  options_->DidNativeEmbedEvent(type, embedId, id, x, y);
  ASSERT_EQ(type, blink::WebInputEvent::Type::kTouchStart);
}
#endif

#if BUILDFLAG(IS_OHOS)
TEST_F(WidgetBaseTest, DidNativeEmbedEvent04) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kTouchEnd;
  std::string embedId = "embedID";
  int32_t id = 0;
  float x = 1.0;
  float y = 1.0;
  std::unique_ptr<MockMessageReceiverWithResponder> mock_receiver =
      std::make_unique<MockMessageReceiverWithResponder>();
  EXPECT_CALL(*mock_receiver, Accept(testing::_))
      .WillOnce(testing::Return(false));
  std::unique_ptr<mojo::MessageReceiverWithResponder> receiver =
      std::move(mock_receiver);
  mojo::internal::AssociatedInterfacePtrState<mojom::blink::WidgetHost>
      widget_host = mojo::internal::AssociatedInterfacePtrState<
          mojom::blink::WidgetHost>();
  widget_host.proxy_ =
      std::make_unique<mojom::blink::WidgetHostProxy>(receiver.get());
  widget_host.proxy_->receiver_ = receiver.get();
  options_->widget_host_.internal_state_.Swap(&widget_host);
  options_->DidNativeEmbedEvent(type, embedId, id, x, y);
  ASSERT_EQ(type, blink::WebInputEvent::Type::kTouchEnd);
}
#endif

}  // namespace blink
