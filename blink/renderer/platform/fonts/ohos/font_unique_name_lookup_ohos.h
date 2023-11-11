// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LINUX_FONT_UNIQUE_NAME_LOOKUP_OHOS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LINUX_FONT_UNIQUE_NAME_LOOKUP_OHOS_H_

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/mojom/android_font_lookup/android_font_lookup.mojom-blink.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class FontUniqueNameLookupOhos : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupOhos() = default;
  FontUniqueNameLookupOhos(const FontUniqueNameLookupOhos&) = delete;
  FontUniqueNameLookupOhos& operator=(const FontUniqueNameLookupOhos&) = delete;
  ~FontUniqueNameLookupOhos() override;

  bool IsFontUniqueNameLookupReadyForSyncLookup() override;

  void PrepareFontUniqueNameLookup(
      NotifyFontUniqueNameLookupReady callback) override;

  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

  void Init() override;

 private:
  void EnsureServiceConnected();

  void ReceiveReadOnlySharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion shared_memory_region);

  mojo::Remote<mojom::blink::FontUniqueNameLookup> ohos_font_lookup_service_;
  WTF::Deque<NotifyFontUniqueNameLookupReady> pending_callbacks_;
  absl::optional<bool> sync_available_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LINUX_FONT_UNIQUE_NAME_LOOKUP_OHOS_H_
