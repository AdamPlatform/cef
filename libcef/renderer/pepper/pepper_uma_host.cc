// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_uma_host.h"

#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/renderer/content_renderer_client.h"

#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // defined(ENABLE_EXTENSIONS)

namespace {

const char* const kPredefinedAllowedUMAOrigins[] = {
    "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see http://crbug.com/317833
    "4EB74897CB187C7633357C2FE832E0AD6A44883A",  // see http://crbug.com/317833
    "9E527CDA9D7C50844E8A5DB964A54A640AE48F98",  // see http://crbug.com/521189
    "DF52618D0B040D8A054D8348D2E84DDEEE5974E7",  // see http://crbug.com/521189
    "269D721F163E587BC53C6F83553BF9CE2BB143CD",  // see http://crbug.com/521189
    "6B55A5329E3F1F30F6032BDB20B2EB4378DBF767",  // see http://crbug.com/521189
    "C449A798C495E6CF7D6AF10162113D564E67AD12",  // see http://crbug.com/521189
    "01E9FFA9A8F3C18271FE91BEE46207F3B81755CC",  // see http://crbug.com/521189
    "97B23E01B2AA064E8332EE43A7A85C628AADC3F2",  // see http://crbug.com/521189
};

const char* const kWhitelistedHistogramPrefixes[] = {
    "22F67DA2061FFC4DC9A4974036348D9C38C22919",  // see http://crbug.com/390221
    "3FEA4650221C5E6C39CF5C5C9F464FF74EAB6CE1",  // see http://crbug.com/521189
};

const char* const kWhitelistedPluginBaseNames[] = {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
    kWidevineCdmAdapterFileName,  // see http://crbug.com/368743
                                  // and http://crbug.com/410630
#endif
    CefContentClient::kPDFPluginPath,
};

std::string HashPrefix(const std::string& histogram) {
  const std::string id_hash =
      base::SHA1HashString(histogram.substr(0, histogram.find('.')));
  DCHECK_EQ(id_hash.length(), base::kSHA1Length);
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

}  // namespace

PepperUMAHost::PepperUMAHost(content::RendererPpapiHost* host,
                             PP_Instance instance,
                             PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      document_url_(host->GetDocumentURL(instance)),
      is_plugin_in_process_(host->IsRunningInProcess()) {
  if (host->GetPluginInstance(instance)) {
    plugin_base_name_ =
        host->GetPluginInstance(instance)->GetModulePath().BaseName();
  }

  for (size_t i = 0; i < arraysize(kPredefinedAllowedUMAOrigins); ++i)
    allowed_origins_.insert(kPredefinedAllowedUMAOrigins[i]);
  for (size_t i = 0; i < arraysize(kWhitelistedHistogramPrefixes); ++i)
    allowed_histogram_prefixes_.insert(kWhitelistedHistogramPrefixes[i]);
  for (size_t i = 0; i < arraysize(kWhitelistedPluginBaseNames); ++i)
    allowed_plugin_base_names_.insert(kWhitelistedPluginBaseNames[i]);
}

PepperUMAHost::~PepperUMAHost() {}

int32_t PepperUMAHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperUMAHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramCustomTimes,
                                      OnHistogramCustomTimes)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramCustomCounts,
                                      OnHistogramCustomCounts)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramEnumeration,
                                      OnHistogramEnumeration)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_UMA_IsCrashReportingEnabled, OnIsCrashReportingEnabled)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperUMAHost::IsPluginWhitelisted() {
#if defined(ENABLE_EXTENSIONS)
  return CefContentRendererClient::IsExtensionOrSharedModuleWhitelisted(
      document_url_, allowed_origins_);
#else
  return false;
#endif
}

bool PepperUMAHost::IsHistogramAllowed(const std::string& histogram) {
  if (is_plugin_in_process_ && histogram.find("NaCl.") == 0) {
    return true;
  }

  if (IsPluginWhitelisted() &&
      ContainsKey(allowed_histogram_prefixes_, HashPrefix(histogram))) {
    return true;
  }

  if (ContainsKey(allowed_plugin_base_names_,
                  plugin_base_name_.MaybeAsASCII())) {
    return true;
  }

  LOG(ERROR) << "Host or histogram name is not allowed to use the UMA API.";
  return false;
}

#define RETURN_IF_BAD_ARGS(_min, _max, _buckets) \
  do {                                           \
    if (_min >= _max || _buckets <= 1)           \
      return PP_ERROR_BADARGUMENT;               \
  } while (0)

int32_t PepperUMAHost::OnHistogramCustomTimes(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int64_t sample,
    int64_t min,
    int64_t max,
    uint32_t bucket_count) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(min, max, bucket_count);

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name,
      base::TimeDelta::FromMilliseconds(min),
      base::TimeDelta::FromMilliseconds(max),
      bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter)
    counter->AddTime(base::TimeDelta::FromMilliseconds(sample));
  return PP_OK;
}

int32_t PepperUMAHost::OnHistogramCustomCounts(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int32_t sample,
    int32_t min,
    int32_t max,
    uint32_t bucket_count) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(min, max, bucket_count);

  base::HistogramBase* counter = base::Histogram::FactoryGet(
      name,
      min,
      max,
      bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter)
    counter->Add(sample);
  return PP_OK;
}

int32_t PepperUMAHost::OnHistogramEnumeration(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int32_t sample,
    int32_t boundary_value) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(0, boundary_value, boundary_value + 1);

  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      name,
      1,
      boundary_value,
      boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter)
    counter->Add(sample);
  return PP_OK;
}

int32_t PepperUMAHost::OnIsCrashReportingEnabled(
    ppapi::host::HostMessageContext* context) {
  if (!IsPluginWhitelisted())
    return PP_ERROR_NOACCESS;
  // TODO(extensions): Chrome uses ChromeViewHostMsg_IsCrashReportingEnabled to
  // retrieve this value. Explore whether it's useful for CEF clients when crash
  // reporting is enabled.
  bool enabled = false;
  if (enabled)
    return PP_OK;
  return PP_ERROR_FAILED;
}