"use strict";
// automatically generated by the FlatBuffers compiler, do not modify
Object.defineProperty(exports, "__esModule", { value: true });
exports.unionListToBody = exports.unionToBody = exports.Body = void 0;
const close_rtp_observer_request_1 = require("../../fbs/router/close-rtp-observer-request");
const close_transport_request_1 = require("../../fbs/router/close-transport-request");
const create_active_speaker_observer_request_1 = require("../../fbs/router/create-active-speaker-observer-request");
const create_audio_level_observer_request_1 = require("../../fbs/router/create-audio-level-observer-request");
const create_direct_transport_request_1 = require("../../fbs/router/create-direct-transport-request");
const create_pipe_transport_request_1 = require("../../fbs/router/create-pipe-transport-request");
const create_plain_transport_request_1 = require("../../fbs/router/create-plain-transport-request");
const create_web_rtc_transport_request_1 = require("../../fbs/router/create-web-rtc-transport-request");
const close_consumer_request_1 = require("../../fbs/transport/close-consumer-request");
const close_data_consumer_request_1 = require("../../fbs/transport/close-data-consumer-request");
const close_data_producer_request_1 = require("../../fbs/transport/close-data-producer-request");
const close_producer_request_1 = require("../../fbs/transport/close-producer-request");
const consume_request_1 = require("../../fbs/transport/consume-request");
const enable_trace_event_request_1 = require("../../fbs/transport/enable-trace-event-request");
const produce_request_1 = require("../../fbs/transport/produce-request");
const set_max_incoming_bitrate_request_1 = require("../../fbs/transport/set-max-incoming-bitrate-request");
const set_max_outgoing_bitrate_request_1 = require("../../fbs/transport/set-max-outgoing-bitrate-request");
const close_router_request_1 = require("../../fbs/worker/close-router-request");
const close_web_rtc_server_request_1 = require("../../fbs/worker/close-web-rtc-server-request");
const create_router_request_1 = require("../../fbs/worker/create-router-request");
const create_web_rtc_server_request_1 = require("../../fbs/worker/create-web-rtc-server-request");
const update_settings_request_1 = require("../../fbs/worker/update-settings-request");
var Body;
(function (Body) {
    Body[Body["NONE"] = 0] = "NONE";
    Body[Body["FBS_Worker_UpdateSettingsRequest"] = 1] = "FBS_Worker_UpdateSettingsRequest";
    Body[Body["FBS_Worker_CreateWebRtcServerRequest"] = 2] = "FBS_Worker_CreateWebRtcServerRequest";
    Body[Body["FBS_Worker_CloseWebRtcServerRequest"] = 3] = "FBS_Worker_CloseWebRtcServerRequest";
    Body[Body["FBS_Worker_CreateRouterRequest"] = 4] = "FBS_Worker_CreateRouterRequest";
    Body[Body["FBS_Worker_CloseRouterRequest"] = 5] = "FBS_Worker_CloseRouterRequest";
    Body[Body["FBS_Router_CreateWebRtcTransportRequest"] = 6] = "FBS_Router_CreateWebRtcTransportRequest";
    Body[Body["FBS_Router_CreatePlainTransportRequest"] = 7] = "FBS_Router_CreatePlainTransportRequest";
    Body[Body["FBS_Router_CreatePipeTransportRequest"] = 8] = "FBS_Router_CreatePipeTransportRequest";
    Body[Body["FBS_Router_CreateDirectTransportRequest"] = 9] = "FBS_Router_CreateDirectTransportRequest";
    Body[Body["FBS_Router_CreateActiveSpeakerObserverRequest"] = 10] = "FBS_Router_CreateActiveSpeakerObserverRequest";
    Body[Body["FBS_Router_CreateAudioLevelObserverRequest"] = 11] = "FBS_Router_CreateAudioLevelObserverRequest";
    Body[Body["FBS_Router_CloseTransportRequest"] = 12] = "FBS_Router_CloseTransportRequest";
    Body[Body["FBS_Router_CloseRtpObserverRequest"] = 13] = "FBS_Router_CloseRtpObserverRequest";
    Body[Body["FBS_Transport_SetMaxIncomingBitrateRequest"] = 14] = "FBS_Transport_SetMaxIncomingBitrateRequest";
    Body[Body["FBS_Transport_SetMaxOutgoingBitrateRequest"] = 15] = "FBS_Transport_SetMaxOutgoingBitrateRequest";
    Body[Body["FBS_Transport_ProduceRequest"] = 16] = "FBS_Transport_ProduceRequest";
    Body[Body["FBS_Transport_ConsumeRequest"] = 17] = "FBS_Transport_ConsumeRequest";
    Body[Body["FBS_Transport_EnableTraceEventRequest"] = 18] = "FBS_Transport_EnableTraceEventRequest";
    Body[Body["FBS_Transport_CloseProducerRequest"] = 19] = "FBS_Transport_CloseProducerRequest";
    Body[Body["FBS_Transport_CloseConsumerRequest"] = 20] = "FBS_Transport_CloseConsumerRequest";
    Body[Body["FBS_Transport_CloseDataProducerRequest"] = 21] = "FBS_Transport_CloseDataProducerRequest";
    Body[Body["FBS_Transport_CloseDataConsumerRequest"] = 22] = "FBS_Transport_CloseDataConsumerRequest";
})(Body = exports.Body || (exports.Body = {}));
function unionToBody(type, accessor) {
    switch (Body[type]) {
        case 'NONE': return null;
        case 'FBS_Worker_UpdateSettingsRequest': return accessor(new update_settings_request_1.UpdateSettingsRequest());
        case 'FBS_Worker_CreateWebRtcServerRequest': return accessor(new create_web_rtc_server_request_1.CreateWebRtcServerRequest());
        case 'FBS_Worker_CloseWebRtcServerRequest': return accessor(new close_web_rtc_server_request_1.CloseWebRtcServerRequest());
        case 'FBS_Worker_CreateRouterRequest': return accessor(new create_router_request_1.CreateRouterRequest());
        case 'FBS_Worker_CloseRouterRequest': return accessor(new close_router_request_1.CloseRouterRequest());
        case 'FBS_Router_CreateWebRtcTransportRequest': return accessor(new create_web_rtc_transport_request_1.CreateWebRtcTransportRequest());
        case 'FBS_Router_CreatePlainTransportRequest': return accessor(new create_plain_transport_request_1.CreatePlainTransportRequest());
        case 'FBS_Router_CreatePipeTransportRequest': return accessor(new create_pipe_transport_request_1.CreatePipeTransportRequest());
        case 'FBS_Router_CreateDirectTransportRequest': return accessor(new create_direct_transport_request_1.CreateDirectTransportRequest());
        case 'FBS_Router_CreateActiveSpeakerObserverRequest': return accessor(new create_active_speaker_observer_request_1.CreateActiveSpeakerObserverRequest());
        case 'FBS_Router_CreateAudioLevelObserverRequest': return accessor(new create_audio_level_observer_request_1.CreateAudioLevelObserverRequest());
        case 'FBS_Router_CloseTransportRequest': return accessor(new close_transport_request_1.CloseTransportRequest());
        case 'FBS_Router_CloseRtpObserverRequest': return accessor(new close_rtp_observer_request_1.CloseRtpObserverRequest());
        case 'FBS_Transport_SetMaxIncomingBitrateRequest': return accessor(new set_max_incoming_bitrate_request_1.SetMaxIncomingBitrateRequest());
        case 'FBS_Transport_SetMaxOutgoingBitrateRequest': return accessor(new set_max_outgoing_bitrate_request_1.SetMaxOutgoingBitrateRequest());
        case 'FBS_Transport_ProduceRequest': return accessor(new produce_request_1.ProduceRequest());
        case 'FBS_Transport_ConsumeRequest': return accessor(new consume_request_1.ConsumeRequest());
        case 'FBS_Transport_EnableTraceEventRequest': return accessor(new enable_trace_event_request_1.EnableTraceEventRequest());
        case 'FBS_Transport_CloseProducerRequest': return accessor(new close_producer_request_1.CloseProducerRequest());
        case 'FBS_Transport_CloseConsumerRequest': return accessor(new close_consumer_request_1.CloseConsumerRequest());
        case 'FBS_Transport_CloseDataProducerRequest': return accessor(new close_data_producer_request_1.CloseDataProducerRequest());
        case 'FBS_Transport_CloseDataConsumerRequest': return accessor(new close_data_consumer_request_1.CloseDataConsumerRequest());
        default: return null;
    }
}
exports.unionToBody = unionToBody;
function unionListToBody(type, accessor, index) {
    switch (Body[type]) {
        case 'NONE': return null;
        case 'FBS_Worker_UpdateSettingsRequest': return accessor(index, new update_settings_request_1.UpdateSettingsRequest());
        case 'FBS_Worker_CreateWebRtcServerRequest': return accessor(index, new create_web_rtc_server_request_1.CreateWebRtcServerRequest());
        case 'FBS_Worker_CloseWebRtcServerRequest': return accessor(index, new close_web_rtc_server_request_1.CloseWebRtcServerRequest());
        case 'FBS_Worker_CreateRouterRequest': return accessor(index, new create_router_request_1.CreateRouterRequest());
        case 'FBS_Worker_CloseRouterRequest': return accessor(index, new close_router_request_1.CloseRouterRequest());
        case 'FBS_Router_CreateWebRtcTransportRequest': return accessor(index, new create_web_rtc_transport_request_1.CreateWebRtcTransportRequest());
        case 'FBS_Router_CreatePlainTransportRequest': return accessor(index, new create_plain_transport_request_1.CreatePlainTransportRequest());
        case 'FBS_Router_CreatePipeTransportRequest': return accessor(index, new create_pipe_transport_request_1.CreatePipeTransportRequest());
        case 'FBS_Router_CreateDirectTransportRequest': return accessor(index, new create_direct_transport_request_1.CreateDirectTransportRequest());
        case 'FBS_Router_CreateActiveSpeakerObserverRequest': return accessor(index, new create_active_speaker_observer_request_1.CreateActiveSpeakerObserverRequest());
        case 'FBS_Router_CreateAudioLevelObserverRequest': return accessor(index, new create_audio_level_observer_request_1.CreateAudioLevelObserverRequest());
        case 'FBS_Router_CloseTransportRequest': return accessor(index, new close_transport_request_1.CloseTransportRequest());
        case 'FBS_Router_CloseRtpObserverRequest': return accessor(index, new close_rtp_observer_request_1.CloseRtpObserverRequest());
        case 'FBS_Transport_SetMaxIncomingBitrateRequest': return accessor(index, new set_max_incoming_bitrate_request_1.SetMaxIncomingBitrateRequest());
        case 'FBS_Transport_SetMaxOutgoingBitrateRequest': return accessor(index, new set_max_outgoing_bitrate_request_1.SetMaxOutgoingBitrateRequest());
        case 'FBS_Transport_ProduceRequest': return accessor(index, new produce_request_1.ProduceRequest());
        case 'FBS_Transport_ConsumeRequest': return accessor(index, new consume_request_1.ConsumeRequest());
        case 'FBS_Transport_EnableTraceEventRequest': return accessor(index, new enable_trace_event_request_1.EnableTraceEventRequest());
        case 'FBS_Transport_CloseProducerRequest': return accessor(index, new close_producer_request_1.CloseProducerRequest());
        case 'FBS_Transport_CloseConsumerRequest': return accessor(index, new close_consumer_request_1.CloseConsumerRequest());
        case 'FBS_Transport_CloseDataProducerRequest': return accessor(index, new close_data_producer_request_1.CloseDataProducerRequest());
        case 'FBS_Transport_CloseDataConsumerRequest': return accessor(index, new close_data_consumer_request_1.CloseDataConsumerRequest());
        default: return null;
    }
}
exports.unionListToBody = unionListToBody;
