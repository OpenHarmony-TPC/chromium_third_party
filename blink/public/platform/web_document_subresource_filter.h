// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"

#ifdef OHOS_ARKWEB_ADBLOCK
#include "url/origin.h"
#endif  // OHOS_ARKWEB_ADBLOCK

namespace blink {

class WebURL;

class WebDocumentSubresourceFilter {
 public:
  // This builder class is created on the main thread and passed to a worker
  // thread to create the subresource filter for the worker thread.
  class Builder {
   public:
    virtual ~Builder() = default;
    virtual std::unique_ptr<WebDocumentSubresourceFilter> Build() = 0;
  };

  enum LoadPolicy { kAllow, kDisallow, kWouldDisallow };

  virtual ~WebDocumentSubresourceFilter() = default;
  virtual LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                                   mojom::RequestContextType) = 0;

#ifdef OHOS_ARKWEB_ADBLOCK
  virtual void ClearStatistics() = 0;

  virtual std::unique_ptr<std::string> GetElementHidingSelectors(
      const WebURL& document_url,
      bool need_common_selectors) = 0;

  virtual bool HasGenericHideTypeOption(
      const WebURL& document_url,
      const url::Origin& parent_document_origin) = 0;

  virtual bool HasElemHideTypeOption(
      const WebURL& document_url,
      const url::Origin& parent_document_origin) = 0;

  virtual bool HasDocumentTypeOption(
      const WebURL& document_url,
      const url::Origin& parent_document_origin) = 0;

  virtual void DidMatchCssRule(const WebURL& document_url,
                               const std::string& dom_path,
                               //  unsigned rule_line_num = 0,
                               bool is_for_report = false) = 0;

  virtual void SetDidFinishLoad(bool did_load_finished) = 0;

  virtual bool GetDidFinishLoad() = 0;

  virtual std::unique_ptr<std::vector<std::string>> GetUserDomPathSelectors(
      const blink::WebURL& document_url,
      bool need_generic_selectors) = 0;

  virtual void set_activation_state(bool enabled) = 0;
#endif

  virtual LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL&) = 0;
  virtual LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) = 0;

  // Report that a resource loaded by the document (not a preload) was
  // disallowed.
  virtual void ReportDisallowedLoad() = 0;

  // Returns true if disallowed resource loads should be logged to the devtools
  // console.
  virtual bool ShouldLogToConsole() = 0;

  // Report that the resource request corresponding to |request_id|  was tagged
  // as an ad.
  virtual void ReportAdRequestId(int request_id) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
