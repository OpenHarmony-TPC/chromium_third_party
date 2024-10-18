// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/variable_local_font_face_source.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_custom_font_data.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

const char* kVariableLocalFontName = "Mobile Data Prompt Font";
const char* kAlternateFontFamily = "HarmonyOS Sans SC";

constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kSlntTag = SkSetFourByteTag('s', 'l', 'n', 't');
constexpr SkFourByteTag kWdthTag = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

absl::optional<SkFontParameters::Variation::Axis>
RetrieveVariationDesignParametersByTag(sk_sp<SkTypeface> base_typeface,
                                       SkFourByteTag tag) {
  int axes_count = base_typeface->getVariationDesignParameters(nullptr, 0);
  if (axes_count <= 0) {
    return absl::nullopt;
  }
  Vector<SkFontParameters::Variation::Axis> axes;
  axes.resize(axes_count);
  int axes_read =
      base_typeface->getVariationDesignParameters(axes.data(), axes_count);
  if (axes_read <= 0) {
    return absl::nullopt;
  }
  for (auto& axis : axes) {
    if (axis.tag == tag) {
      return axis;
    }
  }
  return absl::nullopt;
}

} // namespace

namespace blink {

VariableLocalFontFaceSource::VariableLocalFontFaceSource(
    CSSFontFace* css_font_face,
    FontSelector* font_selector,
    const String& font_name)
    : face_(css_font_face),
      font_selector_(font_selector),
      font_name_(kAlternateFontFamily) {}

VariableLocalFontFaceSource::~VariableLocalFontFaceSource() {}

// static
bool VariableLocalFontFaceSource::ShouldHandleLocalFont(
    const String& font_name) {
  return font_name == kVariableLocalFontName;
}

bool VariableLocalFontFaceSource::IsLocalNonBlocking() const {
  return true;
}

bool VariableLocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) const {
  return true;
}

scoped_refptr<SimpleFontData>
VariableLocalFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  FontCachePurgePreventer font_cache_purge_preventer;
  scoped_refptr<SimpleFontData> temporary_font =
      FontCache::Get().GetLastResortFallbackFont(font_description,
                                                 kDoNotRetain);
  if (!temporary_font) {
    return nullptr;
  }
  scoped_refptr<CSSCustomFontData> css_font_data =
      CSSCustomFontData::Create(this, CSSCustomFontData::kVisibleFallback);
  return SimpleFontData::Create(temporary_font->PlatformData(), css_font_data);
}

scoped_refptr<SimpleFontData> VariableLocalFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  if (!IsValid()) {
    return nullptr;
  }

  if (IsValid() && IsLoading()) {
    scoped_refptr<SimpleFontData> fallback_font_data =
        CreateLoadingFallbackFontData(font_description);
    return fallback_font_data;
  }

  if (!base_typeface_) {
    FontDescription unstyled_description(font_description);
    unstyled_description.SetStretch(NormalWidthValue());
    unstyled_description.SetStyle(NormalSlopeValue());
    unstyled_description.SetWeight(NormalWeightValue());
    scoped_refptr<SimpleFontData> font_data = FontCache::Get().GetFontData(
        unstyled_description, font_name_);

    if (!font_data) {
      return nullptr;
    }
    if (!font_data->PlatformData().Typeface()) {
      return nullptr;
    }
    base_typeface_ = sk_ref_sp(font_data->PlatformData().Typeface());
  }

  return SimpleFontData::Create(
            GetFontPlatformData(font_description, font_selection_capabilities),
            CustomFontData::Create());
}

void VariableLocalFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded()) {
    return;
  }

  face_->DidBeginLoad();
  if (face_->FontLoaded(this)) {
    font_selector_->FontFaceInvalidated(
        FontInvalidationReason::kGeneralInvalidation);
  }
}

bool VariableLocalFontFaceSource::IsLoaded() const {
  return IsLocalNonBlocking();
}

bool VariableLocalFontFaceSource::IsLoading() const {
  return !IsLocalNonBlocking();
}

bool VariableLocalFontFaceSource::IsValid() const {
  return IsLoading() || IsLocalFontAvailable(FontDescription());
}

// See `FontCustomPlatformData::GetFontPlatformData`
FontPlatformData VariableLocalFontFaceSource::GetFontPlatformData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& selection_capabilities) {
  DCHECK(base_typeface_);
  sk_sp<SkTypeface> return_typeface = base_typeface_;

  FontFormatCheck::VariableFontSubType font_sub_type =
      FontFormatCheck::ProbeVariableFont(base_typeface_);
  bool bold = font_description.IsSyntheticBold() &&
      font_description.SyntheticBoldAllowed();
  bool synthetic_bold = bold;
  bool italic = font_description.IsSyntheticItalic() &&
      font_description.SyntheticItalicAllowed();
  bool synthetic_italic = italic;
  const FontSelectionRequest& selection_request =
            font_description.GetFontSelectionRequest();
  if (font_sub_type ==
          FontFormatCheck::VariableFontSubType::kVariableTrueType ||
      font_sub_type == FontFormatCheck::VariableFontSubType::kVariableCFF2) {
    Vector<SkFontArguments::VariationPosition::Coordinate, 0> variation;

    SkFontArguments::VariationPosition::Coordinate weight_coordinate = {
        kWghtTag, SkFloatToScalar(selection_capabilities.weight.clampToRange(
                      selection_request.weight))};
    absl::optional<SkFontParameters::Variation::Axis> wght_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kWghtTag);
    if (selection_capabilities.weight.IsRangeSetFromAuto() && wght_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange wght_range = {
          FontSelectionValue(wght_parameters->min),
          FontSelectionValue(wght_parameters->max)};
      weight_coordinate = {
          kWghtTag,
          SkFloatToScalar(wght_range.clampToRange(selection_request.weight))};
      synthetic_bold = bold && wght_range.maximum < BoldThreshold() &&
                       selection_request.weight >= BoldThreshold();
    }

    SkFontArguments::VariationPosition::Coordinate width_coordinate = {
        kWdthTag, SkFloatToScalar(selection_capabilities.width.clampToRange(
                      selection_request.width))};
    absl::optional<SkFontParameters::Variation::Axis> wdth_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kWdthTag);
    if (selection_capabilities.width.IsRangeSetFromAuto() && wdth_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange wdth_range = {
          FontSelectionValue(wdth_parameters->min),
          FontSelectionValue(wdth_parameters->max)};
      width_coordinate = {
          kWdthTag,
          SkFloatToScalar(wdth_range.clampToRange(selection_request.width))};
    }
    SkFontArguments::VariationPosition::Coordinate slant_coordinate = {
        kSlntTag, SkFloatToScalar(-selection_capabilities.slope.clampToRange(
                      selection_request.slope))};
    absl::optional<SkFontParameters::Variation::Axis> slnt_parameters =
        RetrieveVariationDesignParametersByTag(base_typeface_, kSlntTag);
    if (selection_capabilities.slope.IsRangeSetFromAuto() && slnt_parameters) {
      DCHECK(RuntimeEnabledFeatures::CSSFontFaceAutoVariableRangeEnabled());
      FontSelectionRange slnt_range = {
          FontSelectionValue(slnt_parameters->min),
          FontSelectionValue(slnt_parameters->max)};
      slant_coordinate = {
          kSlntTag,
          SkFloatToScalar(slnt_range.clampToRange(-selection_request.slope))};
      synthetic_italic = italic && slnt_range.maximum < ItalicSlopeValue() &&
                         selection_request.slope >= ItalicSlopeValue();
    }

    variation.push_back(weight_coordinate);
    variation.push_back(width_coordinate);
    variation.push_back(slant_coordinate);

    const FontVariationSettings* variation_settings =
              font_description.VariationSettings();
    bool explicit_opsz_configured = false;
    if (variation_settings && variation_settings->size() < UINT16_MAX) {
      variation.reserve(variation_settings->size() + variation.size());
      for (const auto& setting : *variation_settings) {
        if (setting.Tag() == kOpszTag) {
          explicit_opsz_configured = true;
        }
        SkFontArguments::VariationPosition::Coordinate setting_coordinate =
            {setting.Tag(), SkFloatToScalar(setting.Value())};
        variation.push_back(setting_coordinate);
      }
    }

    if (!explicit_opsz_configured) {
      if (font_description.FontOpticalSizing() == kAutoOpticalSizing) {
        SkFontArguments::VariationPosition::Coordinate opsz_coordinate = {
            kOpszTag, SkFloatToScalar(font_description.AdjustedSpecifiedSize())};
        variation.push_back(opsz_coordinate);
      } else if (font_description.FontOpticalSizing() == kNoneOpticalSizing) {
        // Explicitly set default value to avoid automatic application of
        // optical sizing as it seems to happen on SkTypeface on Mac.
        absl::optional<SkFontParameters::Variation::Axis> opsz_parameters =
            RetrieveVariationDesignParametersByTag(return_typeface, kOpszTag);
        if (opsz_parameters) {
          float opszDefault = opsz_parameters->def;
          SkFontArguments::VariationPosition::Coordinate opsz_coordinate = {
              kOpszTag, SkFloatToScalar(opszDefault)};
          variation.push_back(opsz_coordinate);
        }
      }
    }

    SkFontArguments font_args;
    font_args.setVariationDesignPosition(
        {variation.data(), static_cast<int>(variation.size())});
    sk_sp<SkTypeface> sk_variation_font(base_typeface_->makeClone(font_args));

    if (sk_variation_font) {
      return_typeface = sk_variation_font;
    } else {
      SkString family_name;
      base_typeface_->getFamilyName(&family_name);
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << family_name.c_str();
    }
  }

  const FontPalette* palette = font_description.GetFontPalette();
  if (palette && !palette->IsNormalPalette()) {
    SkFontArguments font_args;
    SkFontArguments::Palette sk_palette{0, nullptr, 0};

    absl::optional<uint16_t> palette_index = absl::nullopt;

    if (palette->GetPaletteNameKind() == FontPalette::kLightPalette ||
        palette->GetPaletteNameKind() == FontPalette::kDarkPalette) {
      OpenTypeCpalLookup::PaletteUse palette_use =
          palette->GetPaletteNameKind() == FontPalette::kLightPalette
              ? OpenTypeCpalLookup::kUsableWithLightBackground
              : OpenTypeCpalLookup::kUsableWithDarkBackground;
      palette_index =
          OpenTypeCpalLookup::FirstThemedPalette(base_typeface_, palette_use);
    } else if (palette->IsCustomPalette()) {
      FontPalette::BasePaletteValue base_palette_index =
          palette->GetBasePalette();

      switch (base_palette_index.type) {
        case FontPalette::kNoBasePalette: {
          palette_index = 0;
          break;
        }
        case FontPalette::kDarkBasePalette: {
          OpenTypeCpalLookup::PaletteUse palette_use =
              OpenTypeCpalLookup::kUsableWithDarkBackground;
          palette_index = OpenTypeCpalLookup::FirstThemedPalette(base_typeface_,
                                                                 palette_use);
          break;
        }
        case FontPalette::kLightBasePalette: {
          OpenTypeCpalLookup::PaletteUse palette_use =
              OpenTypeCpalLookup::kUsableWithLightBackground;
          palette_index = OpenTypeCpalLookup::FirstThemedPalette(base_typeface_,
                                                                 palette_use);
          break;
        }
        case FontPalette::kIndexBasePalette: {
          palette_index = base_palette_index.index;
          break;
        }
      }
    }

    if (palette_index.has_value()) {
      sk_palette.index = *palette_index;

      auto* color_overrides = palette->GetColorOverrides();
      if (color_overrides && color_overrides->size()) {
        sk_palette.overrides =
            reinterpret_cast<const SkFontArguments::Palette::Override*>(
                color_overrides->data());
        sk_palette.overrideCount = color_overrides->size();
      }

      font_args.setPalette(sk_palette);
    }

    sk_sp<SkTypeface> palette_typeface(return_typeface->makeClone(font_args));
    if (palette_typeface) {
      return_typeface = palette_typeface;
    }
  }

  return FontPlatformData(std::move(return_typeface), std::string(),
                          font_description.EffectiveFontSize(),
                          synthetic_bold && !base_typeface_->isBold(),
                          synthetic_italic && !base_typeface_->isItalic(),
                          font_description.TextRendering(),
                          font_description.GetFontVariantAlternates()
                              ? font_description.GetFontVariantAlternates()
                                    ->GetResolvedFontFeatures()
                              : ResolvedFontFeatures(),
                          font_description.Orientation());
}

void VariableLocalFontFaceSource::Trace(Visitor* visitor) const {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  CSSFontFaceSource::Trace(visitor);
}
}  // namespace blink
