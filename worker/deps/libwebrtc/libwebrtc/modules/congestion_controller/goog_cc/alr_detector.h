/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_

#include "api/transport/webrtc_key_value_config.h"
#include "modules/pacing/interval_budget.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/experiments/field_trial_units.h"

#include <absl/types/optional.h>
#include <stddef.h>
#include <stdint.h>


namespace webrtc {

// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.
class AlrDetector {
 public:
  explicit AlrDetector(const WebRtcKeyValueConfig* key_value_config);
  ~AlrDetector();

  void OnBytesSent(size_t bytes_sent, int64_t send_time_ms);

  // Set current estimated bandwidth.
  void SetEstimatedBitrate(int bitrate_bps);

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  absl::optional<int64_t> GetApplicationLimitedRegionStartTime() const;
  absl::optional<int64_t> GetApplicationLimitedRegionStartTime(int64_t at_time_ms);

  void UpdateBudgetWithElapsedTime(int64_t delta_time_ms);
  void UpdateBudgetWithBytesSent(size_t bytes_sent);

 private:
  // Sent traffic ratio as a function of network capacity used to determine
  // application-limited region. ALR region start when bandwidth usage drops
  // below kAlrStartUsageRatio and ends when it raises above
  // kAlrEndUsageRatio. NOTE: This is intentionally conservative at the moment
  // until BW adjustments of application limited region is fine tuned.
  static constexpr double kDefaultBandwidthUsageRatio = 0.65;
  static constexpr double kDefaultStartBudgetLevelRatio = 0.80;
  static constexpr double kDefaultStopBudgetLevelRatio = 0.50;
  static constexpr int    kDefaultAlrTimeout = 3000;

  AlrDetector(const WebRtcKeyValueConfig* key_value_config,
              absl::optional<AlrExperimentSettings> experiment_settings);

  friend class GoogCcStatePrinter;
  FieldTrialParameter<double>  bandwidth_usage_ratio_;
  FieldTrialParameter<double>  start_budget_level_ratio_;
  FieldTrialParameter<double>  stop_budget_level_ratio_;
  FieldTrialParameter<int>     alr_timeout_;

  absl::optional<int64_t> last_send_time_ms_;

  IntervalBudget alr_budget_;
  absl::optional<int64_t> alr_started_time_ms_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
