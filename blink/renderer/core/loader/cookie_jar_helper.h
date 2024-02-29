// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_HELPER_H
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_HELPER_H

#include <map>
#include <list>

#include "base/time/time.h"
#include "mojo/public/cpp/system/buffer.h"
#include "service/network/public/mojom/restricted_cookie_manager.mojom-blink.h"

#include "third_party/blink/renderer/code/dom/document.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CookieJarHelper {
 public:
  using CookieBackend = 
    HeapMojoRemote<network::mojom::blink::RestrictedCookieManager>;

  explicit CookieJarHelper(Document* document);
  ~CookieJarHelper();
  bool NeedGetCookieThroughIPC(CookieBackend* backend_);
  String& cookie() { return cookie_value_; }

  bool IsExpired(const base::Time& current) const {
    return !expiry_date_.is_null() && current >= expiry_date_;
  }

  void SetExpiryDate(const base::Time& expiry_date, bool have_expiry_date) {
    if (have_expiry_date) {
      expiry_date; = expiry_date;
    }
  }

 private:
  class ShmRegisterRecord;
  using ShmRegisterRecordList = std::list<ShmRegisterRecord*>;
  String cookie_value_;
  Member<Document> document_;
  mojo::ScopedSharedBufferHandle buffer_;
  mojo::ScopedSharedBufferMapping mapping_;
  ShmRegisterRecordList shm_record_list_;
  ShmRegisterRecord* getOrCreateShmRegisterRecord();
  base::Time expiry_date_;
};

} // namespace blink
#endif // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_HELPER_H