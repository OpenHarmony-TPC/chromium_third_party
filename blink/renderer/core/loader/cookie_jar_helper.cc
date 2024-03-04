// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cookie_jar_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class CookieJarHelper::ShmRegisterRecord {
 public:
  ShmRegisterRecord(
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      bool registed = false)
      : url_(url),
        site_for_cookies_(site_for_cookies),
        top_frame_origin_(top_frame_origin),
        registed_(registed) {}

  const KURL url() const { return url_; }
  const net::SiteForCookies site_for_cookies() const {
    return site_for_cookies_;
  }
  const scoped_refptr<const SecurityOrigin> top_frame_origin() const {
    return top_frame_origin_;
  }
  bool& registed() { return registed_; }

 private:
  const KURL url_;
  const net::SiteForCookies site_for_cookies_;
  const scoped_refptr<const SecurityOrigin> top_frame_origin_;
  bool registed_;
};

CookieJarHelper::CookieJarHelper(Document* document)
    : document_(document) {}

CookieJarHelper::~CookieJarHelper() {
  if (buffer_.is_valid()) {
    buffer_.reset();
  }
  for (auto item : shm_record_list_) {
    delete item;
  }
}

bool CookieJarHelper::IPCNeeded(CookieBackend* backend) {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty()) {
    return true;
  }

  bool& registed = getOrCreateShmRegisterRecord()->registed();
  if (registed) {
    bool* cookie_changed(static_cast<bool*>(mapping_.get()));
    if (*cookie_changed == true || IsExpired(base::Time::Now())) {
      *cookie_changed = false;
      return true;
    } else {
      return false;
    }
  }

  buffer_ = mojo::SharedBufferHandle::Create(sizeof(bool));
  if (buffer_.is_valid()) {
    mapping_ = buffer_->Map(sizeof(bool));
    auto handle = 
        buffer_->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);
    backend->get()->RegisterCookieChangeObserver(
        cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
        document_->GetExecutionContext()->HasStorageAccess(), std::move(handle), &registed);
  }
  return true;
}

CookieJarHelper::ShmRegisterRecord*
CookieJarHelper::getOrCreateShmRegisterRecord() {
  for (auto item : shm_record_list_) {
    if (item->url() == document_->CookieURL() &&
        item->site_for_cookies().IsEquivalent(document_->SiteForCookies()) &&
        item->top_frame_origin() == document_->TopFrameOrigin()) {
          return item;
        }
  }
  CookieJarHelper::ShmRegisterRecord* record = 
      new CookieJarHelper::ShmRegisterRecord(
          document_->CookieURL(), document_->SiteForCookies(),
          document_->TopFrameOrigin());
  shm_record_list_.emplace_back(record);
  return record;
}
} // namespace blink