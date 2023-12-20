/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NATIVE_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NATIVE_DOCUMENT_H_

#include "third_party/blink/renderer/core/html/html_document.h"

namespace blink {

class NativeDocument final : public HTMLDocument {
 public:
  explicit NativeDocument(const DocumentInit&);

 private:
  DocumentParser* CreateParser() override;

  void DefaultEventHandler(Event&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NATIVE_DOCUMENT_H_
