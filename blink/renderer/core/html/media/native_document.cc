/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */


#include "third_party/blink/renderer/core/html/media/native_document.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/raw_data_document_parser.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/core/html/media/html_native_element.h"

namespace blink {

class NativeDocumentParser : public RawDataDocumentParser {
 public:
  explicit NativeDocumentParser(Document* document)
      : RawDataDocumentParser(document) {}

 private:
  void AppendBytes(const char*, size_t) override {}
  void Finish() override;

  void CreateDocumentStructure();

  bool did_build_document_structure_ = false;
};

void NativeDocumentParser::CreateDocumentStructure() {
  LOG(INFO) << "[NativeEmbed] NativeDocumentParser CreateDocumentStructure";
  if (IsStopped())
    return;
  if (did_build_document_structure_)
    return;
  did_build_document_structure_ = true;

  DCHECK(GetDocument());
  auto* root_element = MakeGarbageCollected<HTMLHtmlElement>(*GetDocument());
  GetDocument()->AppendChild(root_element);
  root_element->InsertedByParser();

  // runScriptsAtDocumentElementAvailable can detach the frame.
  if (IsDetached())
    return;

  auto* head = MakeGarbageCollected<HTMLHeadElement>(*GetDocument());
  auto* meta = MakeGarbageCollected<HTMLMetaElement>(*GetDocument(),
                                                     CreateElementFlags());
  meta->setAttribute(html_names::kNameAttr, "viewport");
  meta->setAttribute(html_names::kContentAttr, "width=device-width");
  head->AppendChild(meta);

  auto* element = MakeGarbageCollected<HTMLNativeElement>(*GetDocument());
  element->setAttribute(html_names::kWidthAttr, "100%");
  element->setAttribute(html_names::kHeightAttr, "100%");
  element->load();
  auto* body = MakeGarbageCollected<HTMLBodyElement>(*GetDocument());

  GetDocument()->WillInsertBody();

  body->AppendChild(element);
  root_element->AppendChild(head);
  // DOM insertion events can detach the frame.
  if (IsDetached())
    return;
  root_element->AppendChild(body);
}

void NativeDocumentParser::Finish() {
  CreateDocumentStructure();
  RawDataDocumentParser::Finish();
}

NativeDocument::NativeDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, kMediaDocumentClass) {
  SetCompatibilityMode(kNoQuirksMode);
  LockCompatibilityMode();
}

DocumentParser* NativeDocument::CreateParser() {
  return MakeGarbageCollected<NativeDocumentParser>(this);
}

void NativeDocument::DefaultEventHandler(Event& event) {
  Node* target_node = event.target()->ToNode();
  if (!target_node)
    return;

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeydown && keyboard_event) {
    if (auto* element =
                      Traversal<HTMLNativeElement>::FirstWithin(*target_node)) {
      event.SetDefaultHandled();
    }
  }
}

}  // namespace blink