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

#include "third_party/blink/renderer/core/paint/native_painter.h"

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/media/html_native_element.h"
#include "third_party/blink/renderer/core/layout/layout_native.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"

namespace blink {

void NativePainter::PaintReplaced(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage)
    return;

  WebNativeBridge* surface_texture_wrapper =
      layout_native_.NativeElement()->GetWebNativeBridge();

  if (!surface_texture_wrapper)
    return;

  PhysicalRect replaced_rect = layout_native_.ReplacedContentRect();
  replaced_rect.Move(paint_offset);
  gfx::Rect snapped_replaced_rect = ToPixelSnappedRect(replaced_rect);

  if (snapped_replaced_rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_native_, paint_info.phase))
    return;

  GraphicsContext& context = paint_info.context;
  // Here we're not painting the video but rather preparing the layer for the
  // compositor to submit video frames. But the compositor will do all the work
  // related to the video moving forward. Therefore we mark the FCP here.
  context.GetPaintController().SetImagePainted();
  PhysicalRect content_box_rect = layout_native_.PhysicalContentBoxRect();
  content_box_rect.Move(paint_offset);

  if (layout_native_.GetDocument().GetPaintPreviewState() !=
      Document::kNotPaintingPreview) {
    // Create a canvas and draw a URL rect to it for the paint preview.
    BoxDrawingRecorder recorder(context, layout_native_, paint_info.phase,
                                paint_offset);
    context.SetURLForRect(layout_native_.GetDocument().Url(),
                          snapped_replaced_rect);
  }

  // Video frames are only painted in software for printing or capturing node
  // images via web APIs.
  bool force_software_video_paint =
      paint_info.ShouldOmitCompositingInfo();

  bool paint_with_foreign_layer = paint_info.phase == PaintPhase::kForeground &&
                                  !force_software_video_paint;
  if (paint_with_foreign_layer) {
    if (cc::Layer* layer = layout_native_.NativeElement()->CcLayer()) {
      layer->SetBounds(snapped_replaced_rect.size());
      layer->SetIsDrawable(true);
      layer->SetHitTestable(true);
      RecordForeignLayer(context, layout_native_,
                         DisplayItem::kForeignLayerVideo, layer,
                         snapped_replaced_rect.origin());
      return;
    }
  }

  BoxDrawingRecorder recorder(context, layout_native_, paint_info.phase,
                              paint_offset);

  if (!force_software_video_paint) {
    // This will display the poster image, if one is present, and otherwise
    // paint nothing.
    ImagePainter(layout_native_)
        .PaintIntoRect(context, replaced_rect, content_box_rect);
  }
}

}  // namespace blink
