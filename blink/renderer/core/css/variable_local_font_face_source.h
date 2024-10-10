// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VARIABLE_LOCAL_FONT_FACE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VARIABLE_LOCAL_FONT_FACE_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTypeface;

namespace blink {

class CSSFontFace;
class FontPlatformData;
class FontSelector;

class VariableLocalFontFaceSource final : public CSSFontFaceSource,
                                          public GarbageCollectedMixin {
 public:
  VariableLocalFontFaceSource(CSSFontFace*, FontSelector*, const String& font_name);
  ~VariableLocalFontFaceSource() override;

  static bool ShouldHandleLocalFont(const String& font_name);

  // Returns whether this font can be immediately retrieved using a non-blocking
  // font lookup, or whether it may need to be retrieved asynchronously,
  // behaving similar to a RemoteFontFaceSource. This is needed on Windows 7 and
  // 8 where the font lookup map needs to be built first.
  bool IsLocalNonBlocking() const override;
  bool IsLocalFontAvailable(const FontDescription&) const override;
  bool IsLoaded() const override;
  bool IsLoading() const override;
  bool IsValid() const override;

  void BeginLoadIfNeeded() override;

  void Trace(Visitor* visitor) const override;

 protected:
  scoped_refptr<SimpleFontData> CreateLoadingFallbackFontData(
      const FontDescription&);

 private:
  scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override;

  FontPlatformData GetFontPlatformData(
      const FontDescription& font_description,
      const FontSelectionCapabilities& selection_capabilities);

  Member<CSSFontFace> face_;
  Member<FontSelector> font_selector_;

  sk_sp<SkTypeface> base_typeface_;

  AtomicString font_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VARIABLE_LOCAL_FONT_FACE_SOURCE_H_
