/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define MS_CLASS "webrtc::AlrDetector"
// #define MS_LOG_DEV_LEVEL 3

#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "rtc_base/numerics/safe_conversions.h"

#include "DepLibUV.hpp"
#include "Logger.hpp"

#include <absl/memory/memory.h>
#include <cstdint>
#include <cstdio>

namespace webrtc {

namespace {
absl::optional<AlrExperimentSettings> GetExperimentSettings(
    const WebRtcKeyValueConfig* key_value_config) {
  // RTC_CHECK(AlrExperimentSettings::MaxOneFieldTrialEnabled(*key_value_config));
  absl::optional<AlrExperimentSettings> experiment_settings =
      AlrExperimentSettings::CreateFromFieldTrial(
          *key_value_config,
          AlrExperimentSettings::kScreenshareProbingBweExperimentName);
  if (!experiment_settings) {
    experiment_settings = AlrExperimentSettings::CreateFromFieldTrial(
        *key_value_config,
        AlrExperimentSettings::kStrictPacingAndProbingExperimentName);
  }
  return experiment_settings;
}
}  //  namespace

AlrDetector::AlrDetector(const WebRtcKeyValueConfig* key_value_config)
    : AlrDetector(key_value_config,
                  GetExperimentSettings(key_value_config)) {}

AlrDetector::AlrDetector(
    const WebRtcKeyValueConfig* key_value_config,
    absl::optional<AlrExperimentSettings> experiment_settings)
    : bandwidth_usage_ratio_(
          "bw_usage",
          experiment_settings
              ? experiment_settings->alr_bandwidth_usage_percent / 100.0
              : kDefaultBandwidthUsageRatio),
      start_budget_level_ratio_(
          "start",
          experiment_settings
              ? experiment_settings->alr_start_budget_level_percent / 100.0
              : kDefaultStartBudgetLevelRatio),
      stop_budget_level_ratio_(
          "stop",
          experiment_settings
              ? experiment_settings->alr_stop_budget_level_percent / 100.0
              : kDefaultStopBudgetLevelRatio),
      alr_timeout_(
          "alr_timeout",
          experiment_settings
              ? experiment_settings->alr_timeout
              : kDefaultAlrTimeout),
      alr_budget_(0, true) {
  ParseFieldTrial({&bandwidth_usage_ratio_, &start_budget_level_ratio_,
                   &stop_budget_level_ratio_},
                  key_value_config->Lookup("WebRTC-AlrDetectorParameters"));
}

AlrDetector::~AlrDetector() {}

void AlrDetector::OnBytesSent(size_t bytes_sent, int64_t send_time_ms) {
  if (!last_send_time_ms_.has_value()) {
    last_send_time_ms_ = send_time_ms;
    // Since the duration for sending the bytes is unknown, return without
    // updating alr state.
    return;
  }
  int64_t delta_time_ms = send_time_ms - *last_send_time_ms_;
  last_send_time_ms_ = send_time_ms;

  alr_budget_.UseBudget(bytes_sent);
  alr_budget_.IncreaseBudget(delta_time_ms);
  bool state_changed = false;
  if (alr_budget_.budget_ratio() > start_budget_level_ratio_ &&
      !alr_started_time_ms_) {
    alr_started_time_ms_.emplace(DepLibUV::GetTimeMsInt64());
    state_changed = true;
  } else if (alr_budget_.budget_ratio() < stop_budget_level_ratio_ &&
             alr_started_time_ms_) {
    state_changed = true;
    alr_started_time_ms_.reset();
  }

  if (state_changed)
    MS_DEBUG_DEV("state changed");
}

void AlrDetector::SetEstimatedBitrate(int bitrate_bps) {
  //RTC_DCHECK(bitrate_bps);
  int target_rate_kbps =
      static_cast<double>(bitrate_bps) * bandwidth_usage_ratio_ / 1000;
  alr_budget_.set_target_rate_kbps(target_rate_kbps);
}

absl::optional<int64_t> AlrDetector::GetApplicationLimitedRegionStartTime()
    const {
  return alr_started_time_ms_;
}

absl::optional<int64_t> AlrDetector::GetApplicationLimitedRegionStartTime(
    int64_t at_time_ms) {
  if (!alr_started_time_ms_ && last_send_time_ms_.has_value()) {
    int64_t delta_time_ms = at_time_ms - *last_send_time_ms_;
    // If ALR is stopped and we haven't sent any packets for a while, force start.
    if (delta_time_ms > alr_timeout_) {
      MS_WARN_TAG(bwe, "large delta_time_ms: %" PRIi64 ", forcing alr state change",
        delta_time_ms);
      alr_started_time_ms_.emplace(at_time_ms);
    }
  }
  return alr_started_time_ms_;
}

}  // namespace webrtc
