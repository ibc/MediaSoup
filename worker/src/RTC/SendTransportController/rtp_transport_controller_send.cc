/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "RTC/SendTransportController/rtp_transport_controller_send.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "RTC/SendTransportController/network_types.h"
#include "RTC/SendTransportController/data_rate.h"
#include "RTC/SendTransportController/time_delta.h"
#include "RTC/SendTransportController/timestamp.h"
// #include "rtc_base/rate_limiter.h"
#include "RTC/SendTransportController/field_trial.h"
#include "RTC/SendTransportController/congestion_controller/goog_cc_network_control.h"

#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

#define MS_CLASS "RtpTransportControllerSend"

namespace webrtc {
namespace {
static const size_t kMaxOverheadBytes = 500;

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         int start_bitrate_bps) {
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(DepLibUV::GetTime());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  if (start_bitrate_bps > 0)
    msg.starting_rate = DataRate::bps(start_bitrate_bps);
  return msg;
}

TargetRateConstraints ConvertConstraints(const BitrateConstraints& contraints) {
  return ConvertConstraints(contraints.min_bitrate_bps,
                            contraints.max_bitrate_bps,
                            contraints.start_bitrate_bps);
}
}  // namespace

RtpTransportControllerSend::RtpTransportControllerSend(
    NetworkStatePredictorFactoryInterface* predictor_factory,
    NetworkControllerFactoryInterface* controller_factory,
    const BitrateConstraints& bitrate_config)
    : pacer_(&packet_router_),
      observer_(nullptr),
      controller_factory_override_(controller_factory),
      // from: api/transport/goog_cc_factory.cc.
      // const int64_t kUpdateIntervalMs = 25;
      process_interval_(TimeDelta::ms(25)),
      last_report_block_time_(Timestamp::ms(DepLibUV::GetTime())),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(false) {
  initial_config_.constraints = ConvertConstraints(bitrate_config);
  initial_config_.key_value_config = &trial_based_config_;
  // RTC_DCHECK(bitrate_config.start_bitrate_bps > 0);

  pacer_.SetPacingRates(bitrate_config.start_bitrate_bps, 0);
}

RtpTransportControllerSend::~RtpTransportControllerSend() {
}

void RtpTransportControllerSend::UpdateControlState() {
  absl::optional<TargetTransferRate> update = control_handler_->GetUpdate();
  if (!update)
    return;
  // We won't create control_handler_ until we have an observers.
  // RTC_DCHECK(observer_ != nullptr);
  observer_->OnTargetTransferRate(*update);
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &this->packet_router_;
}

NetworkStateEstimateObserver*
RtpTransportControllerSend::network_state_estimate_observer() {
  return this;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return this;
}

PacedSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

// jmillan: not used.
void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps,
    int max_total_bitrate_bps) {
  UpdateStreamsConfig();
}

// jmillan: not used.
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  UpdateStreamsConfig();
}

void RtpTransportControllerSend::RegisterTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
    // RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
    observer_->OnStartRateUpdate(*initial_config_.constraints.starting_rate);
    MaybeCreateControllers();
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  MS_DEBUG_TAG(bwe, "SignalNetworkState %s",
                   (network_available ? "Up" : "Down"));

  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(DepLibUV::GetTime());
  msg.network_available = network_available;

  if (network_available_ == msg.network_available)
    return;
  network_available_ = msg.network_available;
  if (network_available_) {
    pacer_.Resume();
  } else {
    pacer_.Pause();
  }
  pacer_.UpdateOutstandingData(0);

  control_handler_->SetNetworkAvailability(network_available_);
  PostUpdates(controller_->OnNetworkAvailability(msg));
  UpdateControlState();
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return this;
}
// jmillan: not used.
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  UpdateStreamsConfig();
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  absl::optional<SentPacket> packet_msg =
      transport_feedback_adapter_.ProcessSentPacket(sent_packet);
  if (packet_msg)
    PostUpdates(controller_->OnSentPacket(*packet_msg));
  pacer_.UpdateOutstandingData(
      transport_feedback_adapter_.GetOutstandingData().bytes());
}

void RtpTransportControllerSend::OnTransportOverheadChanged(
    size_t transport_overhead_bytes_per_packet) {
  if (transport_overhead_bytes_per_packet >= kMaxOverheadBytes) {
    MS_ERROR("Transport overhead exceeds: %zu", kMaxOverheadBytes);
    return;
  }
}

void RtpTransportControllerSend::OnReceivedEstimatedBitrate(uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(DepLibUV::GetTime());
  msg.bandwidth = DataRate::bps(bitrate);

  PostUpdates(controller_->OnRemoteBitrateReport(msg));
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReport(
    const ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);

  RoundTripTimeUpdate report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(rtt_ms);
  report.smoothed = false;
  if (!report.round_trip_time.IsZero())
    PostUpdates(controller_->OnRoundTripTimeUpdate(report));
}

void RtpTransportControllerSend::OnAddPacket(
    const RtpPacketSendInfo& packet_info) {
  transport_feedback_adapter_.AddPacket(
      packet_info,
      send_side_bwe_with_overhead_ ? transport_overhead_bytes_per_packet_.load()
                                   : 0,
      Timestamp::ms(DepLibUV::GetTime()));
}

void RtpTransportControllerSend::OnTransportFeedback(
    const RTC::RTCP::FeedbackRtpTransportPacket& feedback) {

  absl::optional<TransportPacketsFeedback> feedback_msg =
      transport_feedback_adapter_.ProcessTransportFeedback(
          feedback, Timestamp::ms(DepLibUV::GetTime()));
  if (feedback_msg)
    PostUpdates(controller_->OnTransportPacketsFeedback(*feedback_msg));
  pacer_.UpdateOutstandingData(
      transport_feedback_adapter_.GetOutstandingData().bytes());
}

void RtpTransportControllerSend::OnRemoteNetworkEstimate(
    NetworkStateEstimate estimate) {
  estimate.update_time = Timestamp::ms(DepLibUV::GetTime());
  controller_->OnNetworkStateEstimate(estimate);
}

void RtpTransportControllerSend::OnTimer(Timer* timer)
{
	if (timer == pacer_queue_update_task_periodic_timer)
	{
		UpdateControlState();
	}
	else if (timer == controller_task_periodic_timer)
	{
		UpdateControllerWithTimeInterval();
  }
}

// jmillan: this should be done on transport connected.
void RtpTransportControllerSend::MaybeCreateControllers() {
  // RTC_DCHECK(!controller_);
  // RTC_DCHECK(!control_handler_);

  if (!network_available_ || !observer_)
    return;
  control_handler_ = absl::make_unique<CongestionControlHandler>();

  initial_config_.constraints.at_time =
      Timestamp::ms(DepLibUV::GetTime());

  // jmillan: properly create
  controller_ = controller_factory_override_->Create(initial_config_);
  process_interval_ = controller_factory_override_->GetProcessInterval();

  UpdateControllerWithTimeInterval();
  StartProcessPeriodicTasks();
}

void RtpTransportControllerSend::UpdateInitialConstraints(
    TargetRateConstraints new_contraints) {
  if (!new_contraints.starting_rate)
    new_contraints.starting_rate = initial_config_.constraints.starting_rate;
  // RTC_DCHECK(new_contraints.starting_rate);
  initial_config_.constraints = new_contraints;
}

// jmillan: moved to OnTimer.
void RtpTransportControllerSend::StartProcessPeriodicTasks() {
}

void RtpTransportControllerSend::UpdateControllerWithTimeInterval() {
  // RTC_DCHECK(controller_);
  ProcessInterval msg;
  msg.at_time = Timestamp::ms(DepLibUV::GetTime());

  PostUpdates(controller_->OnProcessInterval(msg));
}

// jmillan. removed.
void RtpTransportControllerSend::UpdateStreamsConfig() {
}

void RtpTransportControllerSend::PostUpdates(NetworkControlUpdate update) {
  if (update.congestion_window) {
    if (update.congestion_window->IsFinite())
      pacer_.SetCongestionWindow(update.congestion_window->bytes());
    else
      pacer_.SetCongestionWindow(PacedSender::kNoCongestionWindow);
  }
  if (update.pacer_config) {
    pacer_.SetPacingRates(update.pacer_config->data_rate().bps(),
                          update.pacer_config->pad_rate().bps());
  }
  for (const auto& probe : update.probe_cluster_configs) {
    int64_t bitrate_bps = probe.target_data_rate.bps();
    pacer_.CreateProbeCluster(bitrate_bps, probe.id);
  }
  if (update.target_rate) {
    control_handler_->SetTargetRate(*update.target_rate);
    UpdateControlState();
  }
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReportBlocks(
    const ReportBlockList& report_blocks,
    int64_t now_ms) {
  if (report_blocks.empty())
    return;

  int total_packets_lost_delta = 0;
  int total_packets_delta = 0;

  // Compute the packet loss from all report blocks.
  for (const RTCPReportBlock& report_block : report_blocks) {
    auto it = last_report_blocks_.find(report_block.source_ssrc);
    if (it != last_report_blocks_.end()) {
      auto number_of_packets = report_block.extended_highest_sequence_number -
                               it->second.extended_highest_sequence_number;
      total_packets_delta += number_of_packets;
      auto lost_delta = report_block.packets_lost - it->second.packets_lost;
      total_packets_lost_delta += lost_delta;
    }
    last_report_blocks_[report_block.source_ssrc] = report_block;
  }
  // Can only compute delta if there has been previous blocks to compare to. If
  // not, total_packets_delta will be unchanged and there's nothing more to do.
  if (!total_packets_delta)
    return;
  int packets_received_delta = total_packets_delta - total_packets_lost_delta;
  // To detect lost packets, at least one packet has to be received. This check
  // is needed to avoid bandwith detection update in
  // VideoSendStreamTest.SuspendBelowMinBitrate

  if (packets_received_delta < 1)
    return;
  Timestamp now = Timestamp::ms(now_ms);
  TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;

  PostUpdates(controller_->OnTransportLossReport(msg));
  last_report_block_time_ = now;
}

}  // namespace webrtc
