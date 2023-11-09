// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/ohos/font_unique_name_lookup_ohos.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#include "base/logging.h"

namespace blink {

FontUniqueNameLookupOhos::~FontUniqueNameLookupOhos() = default;

sk_sp<SkTypeface> FontUniqueNameLookupOhos::MatchUniqueName(
    const String& font_unique_name) {
  if (!IsFontUniqueNameLookupReadyForSyncLookup())
    return nullptr;
  absl::optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().c_str());
  if (!match_result)
    return nullptr;
  return SkTypeface::MakeFromFile(match_result->font_path.c_str(),
                                  match_result->ttc_index);
}

void FontUniqueNameLookupOhos::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureServiceConnected();
}

bool FontUniqueNameLookupOhos::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (!RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled())
    return true;

  EnsureServiceConnected();

  if (font_table_matcher_.get())
    return true;

  if (sync_available_.has_value())
    return sync_available_.value();

  bool sync_available_from_mojo = false;
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  ohos_font_lookup_service_->GetUniqueNameLookupTableIfAvailable(
      &sync_available_from_mojo, &shared_memory_region);
  sync_available_ = sync_available_from_mojo;

  if (*sync_available_) {
    DCHECK_EQ(pending_callbacks_.size(), 0u);
    ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
  }

  return *sync_available_;
}

void FontUniqueNameLookupOhos::PrepareFontUniqueNameLookup(
    NotifyFontUniqueNameLookupReady callback) {
  DCHECK(!font_table_matcher_.get());
  DCHECK(RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled());

  pending_callbacks_.push_back(std::move(callback));

  if (pending_callbacks_.size() > 1)
    return;

  EnsureServiceConnected();

  ohos_font_lookup_service_->GetUniqueNameLookupTable(base::BindOnce(
      &FontUniqueNameLookupOhos::ReceiveReadOnlySharedMemoryRegion,
      base::Unretained(this)));
}

void FontUniqueNameLookupOhos::EnsureServiceConnected() {
  if (!ohos_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        ohos_font_lookup_service_.BindNewPipeAndPassReceiver());
  }
}

void FontUniqueNameLookupOhos::ReceiveReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion shared_memory_region) {
  font_table_matcher_ =
      std::make_unique<FontTableMatcher>(shared_memory_region.Map());
  while (!pending_callbacks_.IsEmpty()) {
    NotifyFontUniqueNameLookupReady callback = pending_callbacks_.TakeFirst();
    std::move(callback).Run();
  }
}

}  // namespace blink
