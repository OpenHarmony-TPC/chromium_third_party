/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_cache.h"

#if BUILDFLAG(IS_OHOS)
#include <string>
#include <vector>
#endif

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

#if BUILDFLAG(IS_OHOS)
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "net/http/http_status_code.h"
#endif

namespace blink {

// A helper method for coverting a MemoryCache::TypeStatistic to a
// WebCacheResourceTypeStat.
static void ToResourceTypeStat(const MemoryCache::TypeStatistic& from,
                               WebCacheResourceTypeStat& to) {
  to.count = from.count;
  to.size = from.size;
  to.decoded_size = from.decoded_size;
}

void WebCache::SetCapacity(size_t capacity) {
  MemoryCache* cache = MemoryCache::Get();
  if (cache)
    cache->SetCapacity(static_cast<unsigned>(capacity));
}

void WebCache::Clear() {
  MemoryCache* cache = MemoryCache::Get();
  if (cache)
    cache->EvictResources();
}

void WebCache::GetResourceTypeStats(WebCacheResourceTypeStats* result) {
  MemoryCache* cache = MemoryCache::Get();
  if (cache) {
    MemoryCache::Statistics stats = cache->GetStatistics();
    ToResourceTypeStat(stats.images, result->images);
    ToResourceTypeStat(stats.css_style_sheets, result->css_style_sheets);
    ToResourceTypeStat(stats.scripts, result->scripts);
    ToResourceTypeStat(stats.xsl_style_sheets, result->xsl_style_sheets);
    ToResourceTypeStat(stats.fonts, result->fonts);
    ToResourceTypeStat(stats.other, result->other);
  } else {
    memset(result, 0, sizeof(WebCacheResourceTypeStats));
  }
}

#if BUILDFLAG(IS_OHOS)
void WebCache::AddResourceToCache(const std::string& url,
                                  const std::string& origin,
                                  const std::vector<uint8_t>& resource,
                                  const base::flat_map<std::string, std::string>& response_headers,
                                  const uint64_t type) {
  MemoryCache* cache = MemoryCache::Get();
  if (!cache) {
    LOG(DEBUG) << "Cannot get MemoryCache.";
    return;
  }

  OfflineResourceType resource_type = static_cast<OfflineResourceType>(type);
  Resource* resource_obj;

  KURL kurl((String(url)));
  KURL origin_url((String(origin)));

  ResourceResponse response = GetResourceResponse(kurl, response_headers);

  AtomicString mime_type = response.MimeType();
  if (resource_type == OfflineResourceType::MODULE_JS &&
      !MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type)) {
    LOG(ERROR) << "Not supported MIMEType for module Javascript: " << mime_type.GetString();
    return;
  }

  switch (resource_type) {
    case OfflineResourceType::IMAGE:
      resource_obj = ImageResource::CreateForOfflineResource(kurl, origin_url);
      break;
    case OfflineResourceType::CSS:
      resource_obj = CSSStyleSheetResource::CreateForOfflineResource(kurl, origin_url);
      break;
    case OfflineResourceType::CLASSIC_JS:
    case OfflineResourceType::MODULE_JS:
      resource_obj = ScriptResource::CreateForOfflineResource(
          kurl, origin_url, response, resource_type == OfflineResourceType::MODULE_JS);
      break;
    default:
      LOG(DEBUG) << "Type is not supported to be added into MemoryCache.";
      return;
  }

  resource_obj->AppendData((char*)resource.data(), resource.size());
  resource_obj->ResponseReceived(response);
  resource_obj->Finish(base::TimeTicks(), nullptr);
  resource_obj->SetKeepAliveOn();

  cache->Add(resource_obj);
  return;
}

ResourceResponse WebCache::GetResourceResponse(const KURL& kurl,
                                               const base::flat_map<std::string, std::string>& response_headers) {
  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);

  for (auto header : response_headers) {
    response.SetHttpHeaderField(AtomicString(header.first.c_str()), AtomicString(header.second.c_str()));
  }

  AtomicString cache_control = response.HttpHeaderField(AtomicString("Cache-Control"));
  AtomicString expires = response.HttpHeaderField(AtomicString("Expires"));
  if ((cache_control.empty() && expires.empty()) ||
      response.CacheControlContainsNoCache() ||
      response.CacheControlContainsNoStore()) {
    response.SetHttpHeaderField(AtomicString("Cache-Control"), AtomicString("max-age=86400"));
  }

  AtomicString content_type = response.HttpContentType();
  if (!content_type.empty()) {
    Vector<String> result;
    content_type.GetString().Split(";", result);
    response.SetMimeType(AtomicString(result[0]));
  }

  response.SetHttpHeaderField(AtomicString("Age"), AtomicString("0"));
  response.SetHttpStatusCode(net::HTTP_OK);

  return response;
}
#endif
}  // namespace blink
