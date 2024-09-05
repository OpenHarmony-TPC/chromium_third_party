// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller.h"

#include <math.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "cc/input/input_handler.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_exponential.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {
std::shared_ptr<ElasticOverscrollController> g_overScrollController;

class ElasticOverscrollControllerMock : public ElasticOverscrollController {
  public:
    ElasticOverscrollControllerMock(cc::ScrollElasticityHelper* helper) :
      ElasticOverscrollController(helper) {}

    void DidEnterMomentumAnimatedState() {}

    gfx::Vector2d StretchAmountForTimeDelta(
      const base::TimeDelta& delta) const {
        gfx::Vector2d point;
        return point;
      }

    gfx::Vector2d StretchAmountForAccumulatedOverscroll(
      const gfx::Vector2dF& accumulated_overscroll) const {
        gfx::Vector2d point;
        return point;
      }

    gfx::Vector2d AccumulatedOverscrollForStretchAmount(
      const gfx::Vector2dF& stretch_amount) const {
        gfx::Vector2d point;
        return point;
      }
};

class ElasticOverscrollControllerTest : public testing::Test {
 public:
  static void SetUpTestCase(void);
  static void TearDownTestCase(void);
  void SetUp();
  void TearDown();
};
void ElasticOverscrollControllerTest::SetUpTestCase(void) {}

void ElasticOverscrollControllerTest::TearDownTestCase(void) {}

void ElasticOverscrollControllerTest::SetUp(void) {
  g_overScrollController =
    std::make_shared<ElasticOverscrollControllerMock>(nullptr);
}

void ElasticOverscrollControllerTest::TearDown(void) {
  g_overScrollController = nullptr;
}

TEST_F(ElasticOverscrollControllerTest, GetOverScrollOffset) {
  #if defined(OHOS_GET_SCROLL_OFFSET)
  gfx::Vector2dF overScrollOffset;
  overScrollOffset.set_x(1.0f);
  overScrollOffset.set_y(1.0f);

  overScrollOffset = g_overScrollController->GetOverScrollOffset();
  EXPECT_EQ(overScrollOffset.x(), 0);
  EXPECT_EQ(overScrollOffset.y(), 0);
  #endif
}
}  // namespace blink
