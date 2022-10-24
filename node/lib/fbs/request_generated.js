"use strict";
// automatically generated by the FlatBuffers compiler, do not modify
Object.defineProperty(exports, "__esModule", { value: true });
exports.RtpCodecParameters = exports.RtcpParametersT = exports.RtcpParameters = exports.RtcpFeedbackT = exports.RtcpFeedback = exports.ParameterT = exports.Parameter = exports.MediaKind = exports.IntegerArrayT = exports.IntegerArray = exports.IntegerT = exports.Integer = exports.DoubleT = exports.Double = exports.BooleanT = exports.Boolean = exports.RouterDumpT = exports.RouterDump = exports.CreateWebRtcTransportRequestT = exports.CreateWebRtcTransportRequest = exports.CreatePlainTransportRequestT = exports.CreatePlainTransportRequest = exports.CreatePipeTransportRequestT = exports.CreatePipeTransportRequest = exports.RequestT = exports.Request = exports.Method = exports.unionListToBody = exports.unionToBody = exports.Body = exports.PlainTransportOptionsT = exports.PlainTransportOptions = exports.PlainTransportDumpT = exports.PlainTransportDump = exports.PipeTransportOptionsT = exports.PipeTransportOptions = exports.ConsumerScoreT = exports.ConsumerScore = exports.ConsumerLayersT = exports.ConsumerLayers = exports.Uint32StringT = exports.Uint32String = exports.Uint16StringT = exports.Uint16String = exports.StringUint8T = exports.StringUint8 = exports.StringStringArrayT = exports.StringStringArray = exports.StringStringT = exports.StringString = void 0;
exports.unionListToTransportDumpData = exports.unionToTransportDumpData = exports.TransportDumpData = exports.TransportDumpT = exports.TransportDump = exports.SrtpParametersT = exports.SrtpParameters = exports.SctpListenerT = exports.SctpListener = exports.RtpListenerT = exports.RtpListener = exports.PipeTransportDumpT = exports.PipeTransportDump = exports.IceParametersT = exports.IceParameters = exports.IceCandidateT = exports.IceCandidate = exports.FingerprintT = exports.Fingerprint = exports.DtlsParametersT = exports.DtlsParameters = exports.DirectTransportDumpT = exports.DirectTransportDump = exports.ConsumeResponseT = exports.ConsumeResponse = exports.ConsumeRequestT = exports.ConsumeRequest = exports.BaseTransportOptionsT = exports.BaseTransportOptions = exports.BaseTransportDumpT = exports.BaseTransportDump = exports.SctpParametersT = exports.SctpParameters = exports.NumSctpStreamsT = exports.NumSctpStreams = exports.unionListToValue = exports.unionToValue = exports.Value = exports.Type = exports.StringT = exports.String = exports.RtxT = exports.Rtx = exports.RtpParametersT = exports.RtpParameters = exports.RtpHeaderExtensionParametersT = exports.RtpHeaderExtensionParameters = exports.RtpEncodingParametersT = exports.RtpEncodingParameters = exports.RtpCodecParametersT = void 0;
exports.WorkerDumpT = exports.WorkerDump = exports.UpdateableSettingsT = exports.UpdateableSettings = exports.ResourceUsageT = exports.ResourceUsage = exports.CreateWebRtcServerRequestT = exports.CreateWebRtcServerRequest = exports.CreateRouterRequestT = exports.CreateRouterRequest = exports.CloseWebRtcServerRequestT = exports.CloseWebRtcServerRequest = exports.CloseRouterRequestT = exports.CloseRouterRequest = exports.ChannelMessageHandlersT = exports.ChannelMessageHandlers = exports.WebRtcTransportOptionsT = exports.WebRtcTransportOptions = exports.WebRtcTransportListenServerT = exports.WebRtcTransportListenServer = exports.WebRtcTransportListenIndividualT = exports.WebRtcTransportListenIndividual = exports.unionListToWebRtcTransportListen = exports.unionToWebRtcTransportListen = exports.WebRtcTransportListen = exports.WebRtcServerListenInfoT = exports.WebRtcServerListenInfo = exports.WebRtcServerDumpT = exports.WebRtcServerDump = exports.TupleHashT = exports.TupleHash = exports.IpPortT = exports.IpPort = exports.IceUserNameFragmentT = exports.IceUserNameFragment = exports.WebRtcTransportDumpT = exports.WebRtcTransportDump = exports.TupleT = exports.Tuple = exports.TransportProtocol = exports.TransportListenIpT = exports.TransportListenIp = void 0;
var string_string_1 = require("./fbs/common/string-string");
Object.defineProperty(exports, "StringString", { enumerable: true, get: function () { return string_string_1.StringString; } });
Object.defineProperty(exports, "StringStringT", { enumerable: true, get: function () { return string_string_1.StringStringT; } });
var string_string_array_1 = require("./fbs/common/string-string-array");
Object.defineProperty(exports, "StringStringArray", { enumerable: true, get: function () { return string_string_array_1.StringStringArray; } });
Object.defineProperty(exports, "StringStringArrayT", { enumerable: true, get: function () { return string_string_array_1.StringStringArrayT; } });
var string_uint8_1 = require("./fbs/common/string-uint8");
Object.defineProperty(exports, "StringUint8", { enumerable: true, get: function () { return string_uint8_1.StringUint8; } });
Object.defineProperty(exports, "StringUint8T", { enumerable: true, get: function () { return string_uint8_1.StringUint8T; } });
var uint16string_1 = require("./fbs/common/uint16string");
Object.defineProperty(exports, "Uint16String", { enumerable: true, get: function () { return uint16string_1.Uint16String; } });
Object.defineProperty(exports, "Uint16StringT", { enumerable: true, get: function () { return uint16string_1.Uint16StringT; } });
var uint32string_1 = require("./fbs/common/uint32string");
Object.defineProperty(exports, "Uint32String", { enumerable: true, get: function () { return uint32string_1.Uint32String; } });
Object.defineProperty(exports, "Uint32StringT", { enumerable: true, get: function () { return uint32string_1.Uint32StringT; } });
var consumer_layers_1 = require("./fbs/consumer/consumer-layers");
Object.defineProperty(exports, "ConsumerLayers", { enumerable: true, get: function () { return consumer_layers_1.ConsumerLayers; } });
Object.defineProperty(exports, "ConsumerLayersT", { enumerable: true, get: function () { return consumer_layers_1.ConsumerLayersT; } });
var consumer_score_1 = require("./fbs/consumer/consumer-score");
Object.defineProperty(exports, "ConsumerScore", { enumerable: true, get: function () { return consumer_score_1.ConsumerScore; } });
Object.defineProperty(exports, "ConsumerScoreT", { enumerable: true, get: function () { return consumer_score_1.ConsumerScoreT; } });
var pipe_transport_options_1 = require("./fbs/pipe-transport/pipe-transport-options");
Object.defineProperty(exports, "PipeTransportOptions", { enumerable: true, get: function () { return pipe_transport_options_1.PipeTransportOptions; } });
Object.defineProperty(exports, "PipeTransportOptionsT", { enumerable: true, get: function () { return pipe_transport_options_1.PipeTransportOptionsT; } });
var plain_transport_dump_1 = require("./fbs/plain-transport/plain-transport-dump");
Object.defineProperty(exports, "PlainTransportDump", { enumerable: true, get: function () { return plain_transport_dump_1.PlainTransportDump; } });
Object.defineProperty(exports, "PlainTransportDumpT", { enumerable: true, get: function () { return plain_transport_dump_1.PlainTransportDumpT; } });
var plain_transport_options_1 = require("./fbs/plain-transport/plain-transport-options");
Object.defineProperty(exports, "PlainTransportOptions", { enumerable: true, get: function () { return plain_transport_options_1.PlainTransportOptions; } });
Object.defineProperty(exports, "PlainTransportOptionsT", { enumerable: true, get: function () { return plain_transport_options_1.PlainTransportOptionsT; } });
var body_1 = require("./fbs/request/body");
Object.defineProperty(exports, "Body", { enumerable: true, get: function () { return body_1.Body; } });
Object.defineProperty(exports, "unionToBody", { enumerable: true, get: function () { return body_1.unionToBody; } });
Object.defineProperty(exports, "unionListToBody", { enumerable: true, get: function () { return body_1.unionListToBody; } });
var method_1 = require("./fbs/request/method");
Object.defineProperty(exports, "Method", { enumerable: true, get: function () { return method_1.Method; } });
var request_1 = require("./fbs/request/request");
Object.defineProperty(exports, "Request", { enumerable: true, get: function () { return request_1.Request; } });
Object.defineProperty(exports, "RequestT", { enumerable: true, get: function () { return request_1.RequestT; } });
var create_pipe_transport_request_1 = require("./fbs/router/create-pipe-transport-request");
Object.defineProperty(exports, "CreatePipeTransportRequest", { enumerable: true, get: function () { return create_pipe_transport_request_1.CreatePipeTransportRequest; } });
Object.defineProperty(exports, "CreatePipeTransportRequestT", { enumerable: true, get: function () { return create_pipe_transport_request_1.CreatePipeTransportRequestT; } });
var create_plain_transport_request_1 = require("./fbs/router/create-plain-transport-request");
Object.defineProperty(exports, "CreatePlainTransportRequest", { enumerable: true, get: function () { return create_plain_transport_request_1.CreatePlainTransportRequest; } });
Object.defineProperty(exports, "CreatePlainTransportRequestT", { enumerable: true, get: function () { return create_plain_transport_request_1.CreatePlainTransportRequestT; } });
var create_web_rtc_transport_request_1 = require("./fbs/router/create-web-rtc-transport-request");
Object.defineProperty(exports, "CreateWebRtcTransportRequest", { enumerable: true, get: function () { return create_web_rtc_transport_request_1.CreateWebRtcTransportRequest; } });
Object.defineProperty(exports, "CreateWebRtcTransportRequestT", { enumerable: true, get: function () { return create_web_rtc_transport_request_1.CreateWebRtcTransportRequestT; } });
var router_dump_1 = require("./fbs/router/router-dump");
Object.defineProperty(exports, "RouterDump", { enumerable: true, get: function () { return router_dump_1.RouterDump; } });
Object.defineProperty(exports, "RouterDumpT", { enumerable: true, get: function () { return router_dump_1.RouterDumpT; } });
var boolean_1 = require("./fbs/rtp-parameters/boolean");
Object.defineProperty(exports, "Boolean", { enumerable: true, get: function () { return boolean_1.Boolean; } });
Object.defineProperty(exports, "BooleanT", { enumerable: true, get: function () { return boolean_1.BooleanT; } });
var double_1 = require("./fbs/rtp-parameters/double");
Object.defineProperty(exports, "Double", { enumerable: true, get: function () { return double_1.Double; } });
Object.defineProperty(exports, "DoubleT", { enumerable: true, get: function () { return double_1.DoubleT; } });
var integer_1 = require("./fbs/rtp-parameters/integer");
Object.defineProperty(exports, "Integer", { enumerable: true, get: function () { return integer_1.Integer; } });
Object.defineProperty(exports, "IntegerT", { enumerable: true, get: function () { return integer_1.IntegerT; } });
var integer_array_1 = require("./fbs/rtp-parameters/integer-array");
Object.defineProperty(exports, "IntegerArray", { enumerable: true, get: function () { return integer_array_1.IntegerArray; } });
Object.defineProperty(exports, "IntegerArrayT", { enumerable: true, get: function () { return integer_array_1.IntegerArrayT; } });
var media_kind_1 = require("./fbs/rtp-parameters/media-kind");
Object.defineProperty(exports, "MediaKind", { enumerable: true, get: function () { return media_kind_1.MediaKind; } });
var parameter_1 = require("./fbs/rtp-parameters/parameter");
Object.defineProperty(exports, "Parameter", { enumerable: true, get: function () { return parameter_1.Parameter; } });
Object.defineProperty(exports, "ParameterT", { enumerable: true, get: function () { return parameter_1.ParameterT; } });
var rtcp_feedback_1 = require("./fbs/rtp-parameters/rtcp-feedback");
Object.defineProperty(exports, "RtcpFeedback", { enumerable: true, get: function () { return rtcp_feedback_1.RtcpFeedback; } });
Object.defineProperty(exports, "RtcpFeedbackT", { enumerable: true, get: function () { return rtcp_feedback_1.RtcpFeedbackT; } });
var rtcp_parameters_1 = require("./fbs/rtp-parameters/rtcp-parameters");
Object.defineProperty(exports, "RtcpParameters", { enumerable: true, get: function () { return rtcp_parameters_1.RtcpParameters; } });
Object.defineProperty(exports, "RtcpParametersT", { enumerable: true, get: function () { return rtcp_parameters_1.RtcpParametersT; } });
var rtp_codec_parameters_1 = require("./fbs/rtp-parameters/rtp-codec-parameters");
Object.defineProperty(exports, "RtpCodecParameters", { enumerable: true, get: function () { return rtp_codec_parameters_1.RtpCodecParameters; } });
Object.defineProperty(exports, "RtpCodecParametersT", { enumerable: true, get: function () { return rtp_codec_parameters_1.RtpCodecParametersT; } });
var rtp_encoding_parameters_1 = require("./fbs/rtp-parameters/rtp-encoding-parameters");
Object.defineProperty(exports, "RtpEncodingParameters", { enumerable: true, get: function () { return rtp_encoding_parameters_1.RtpEncodingParameters; } });
Object.defineProperty(exports, "RtpEncodingParametersT", { enumerable: true, get: function () { return rtp_encoding_parameters_1.RtpEncodingParametersT; } });
var rtp_header_extension_parameters_1 = require("./fbs/rtp-parameters/rtp-header-extension-parameters");
Object.defineProperty(exports, "RtpHeaderExtensionParameters", { enumerable: true, get: function () { return rtp_header_extension_parameters_1.RtpHeaderExtensionParameters; } });
Object.defineProperty(exports, "RtpHeaderExtensionParametersT", { enumerable: true, get: function () { return rtp_header_extension_parameters_1.RtpHeaderExtensionParametersT; } });
var rtp_parameters_1 = require("./fbs/rtp-parameters/rtp-parameters");
Object.defineProperty(exports, "RtpParameters", { enumerable: true, get: function () { return rtp_parameters_1.RtpParameters; } });
Object.defineProperty(exports, "RtpParametersT", { enumerable: true, get: function () { return rtp_parameters_1.RtpParametersT; } });
var rtx_1 = require("./fbs/rtp-parameters/rtx");
Object.defineProperty(exports, "Rtx", { enumerable: true, get: function () { return rtx_1.Rtx; } });
Object.defineProperty(exports, "RtxT", { enumerable: true, get: function () { return rtx_1.RtxT; } });
var string_1 = require("./fbs/rtp-parameters/string");
Object.defineProperty(exports, "String", { enumerable: true, get: function () { return string_1.String; } });
Object.defineProperty(exports, "StringT", { enumerable: true, get: function () { return string_1.StringT; } });
var type_1 = require("./fbs/rtp-parameters/type");
Object.defineProperty(exports, "Type", { enumerable: true, get: function () { return type_1.Type; } });
var value_1 = require("./fbs/rtp-parameters/value");
Object.defineProperty(exports, "Value", { enumerable: true, get: function () { return value_1.Value; } });
Object.defineProperty(exports, "unionToValue", { enumerable: true, get: function () { return value_1.unionToValue; } });
Object.defineProperty(exports, "unionListToValue", { enumerable: true, get: function () { return value_1.unionListToValue; } });
var num_sctp_streams_1 = require("./fbs/sctp-parameters/num-sctp-streams");
Object.defineProperty(exports, "NumSctpStreams", { enumerable: true, get: function () { return num_sctp_streams_1.NumSctpStreams; } });
Object.defineProperty(exports, "NumSctpStreamsT", { enumerable: true, get: function () { return num_sctp_streams_1.NumSctpStreamsT; } });
var sctp_parameters_1 = require("./fbs/sctp-parameters/sctp-parameters");
Object.defineProperty(exports, "SctpParameters", { enumerable: true, get: function () { return sctp_parameters_1.SctpParameters; } });
Object.defineProperty(exports, "SctpParametersT", { enumerable: true, get: function () { return sctp_parameters_1.SctpParametersT; } });
var base_transport_dump_1 = require("./fbs/transport/base-transport-dump");
Object.defineProperty(exports, "BaseTransportDump", { enumerable: true, get: function () { return base_transport_dump_1.BaseTransportDump; } });
Object.defineProperty(exports, "BaseTransportDumpT", { enumerable: true, get: function () { return base_transport_dump_1.BaseTransportDumpT; } });
var base_transport_options_1 = require("./fbs/transport/base-transport-options");
Object.defineProperty(exports, "BaseTransportOptions", { enumerable: true, get: function () { return base_transport_options_1.BaseTransportOptions; } });
Object.defineProperty(exports, "BaseTransportOptionsT", { enumerable: true, get: function () { return base_transport_options_1.BaseTransportOptionsT; } });
var consume_request_1 = require("./fbs/transport/consume-request");
Object.defineProperty(exports, "ConsumeRequest", { enumerable: true, get: function () { return consume_request_1.ConsumeRequest; } });
Object.defineProperty(exports, "ConsumeRequestT", { enumerable: true, get: function () { return consume_request_1.ConsumeRequestT; } });
var consume_response_1 = require("./fbs/transport/consume-response");
Object.defineProperty(exports, "ConsumeResponse", { enumerable: true, get: function () { return consume_response_1.ConsumeResponse; } });
Object.defineProperty(exports, "ConsumeResponseT", { enumerable: true, get: function () { return consume_response_1.ConsumeResponseT; } });
var direct_transport_dump_1 = require("./fbs/transport/direct-transport-dump");
Object.defineProperty(exports, "DirectTransportDump", { enumerable: true, get: function () { return direct_transport_dump_1.DirectTransportDump; } });
Object.defineProperty(exports, "DirectTransportDumpT", { enumerable: true, get: function () { return direct_transport_dump_1.DirectTransportDumpT; } });
var dtls_parameters_1 = require("./fbs/transport/dtls-parameters");
Object.defineProperty(exports, "DtlsParameters", { enumerable: true, get: function () { return dtls_parameters_1.DtlsParameters; } });
Object.defineProperty(exports, "DtlsParametersT", { enumerable: true, get: function () { return dtls_parameters_1.DtlsParametersT; } });
var fingerprint_1 = require("./fbs/transport/fingerprint");
Object.defineProperty(exports, "Fingerprint", { enumerable: true, get: function () { return fingerprint_1.Fingerprint; } });
Object.defineProperty(exports, "FingerprintT", { enumerable: true, get: function () { return fingerprint_1.FingerprintT; } });
var ice_candidate_1 = require("./fbs/transport/ice-candidate");
Object.defineProperty(exports, "IceCandidate", { enumerable: true, get: function () { return ice_candidate_1.IceCandidate; } });
Object.defineProperty(exports, "IceCandidateT", { enumerable: true, get: function () { return ice_candidate_1.IceCandidateT; } });
var ice_parameters_1 = require("./fbs/transport/ice-parameters");
Object.defineProperty(exports, "IceParameters", { enumerable: true, get: function () { return ice_parameters_1.IceParameters; } });
Object.defineProperty(exports, "IceParametersT", { enumerable: true, get: function () { return ice_parameters_1.IceParametersT; } });
var pipe_transport_dump_1 = require("./fbs/transport/pipe-transport-dump");
Object.defineProperty(exports, "PipeTransportDump", { enumerable: true, get: function () { return pipe_transport_dump_1.PipeTransportDump; } });
Object.defineProperty(exports, "PipeTransportDumpT", { enumerable: true, get: function () { return pipe_transport_dump_1.PipeTransportDumpT; } });
var rtp_listener_1 = require("./fbs/transport/rtp-listener");
Object.defineProperty(exports, "RtpListener", { enumerable: true, get: function () { return rtp_listener_1.RtpListener; } });
Object.defineProperty(exports, "RtpListenerT", { enumerable: true, get: function () { return rtp_listener_1.RtpListenerT; } });
var sctp_listener_1 = require("./fbs/transport/sctp-listener");
Object.defineProperty(exports, "SctpListener", { enumerable: true, get: function () { return sctp_listener_1.SctpListener; } });
Object.defineProperty(exports, "SctpListenerT", { enumerable: true, get: function () { return sctp_listener_1.SctpListenerT; } });
var srtp_parameters_1 = require("./fbs/transport/srtp-parameters");
Object.defineProperty(exports, "SrtpParameters", { enumerable: true, get: function () { return srtp_parameters_1.SrtpParameters; } });
Object.defineProperty(exports, "SrtpParametersT", { enumerable: true, get: function () { return srtp_parameters_1.SrtpParametersT; } });
var transport_dump_1 = require("./fbs/transport/transport-dump");
Object.defineProperty(exports, "TransportDump", { enumerable: true, get: function () { return transport_dump_1.TransportDump; } });
Object.defineProperty(exports, "TransportDumpT", { enumerable: true, get: function () { return transport_dump_1.TransportDumpT; } });
var transport_dump_data_1 = require("./fbs/transport/transport-dump-data");
Object.defineProperty(exports, "TransportDumpData", { enumerable: true, get: function () { return transport_dump_data_1.TransportDumpData; } });
Object.defineProperty(exports, "unionToTransportDumpData", { enumerable: true, get: function () { return transport_dump_data_1.unionToTransportDumpData; } });
Object.defineProperty(exports, "unionListToTransportDumpData", { enumerable: true, get: function () { return transport_dump_data_1.unionListToTransportDumpData; } });
var transport_listen_ip_1 = require("./fbs/transport/transport-listen-ip");
Object.defineProperty(exports, "TransportListenIp", { enumerable: true, get: function () { return transport_listen_ip_1.TransportListenIp; } });
Object.defineProperty(exports, "TransportListenIpT", { enumerable: true, get: function () { return transport_listen_ip_1.TransportListenIpT; } });
var transport_protocol_1 = require("./fbs/transport/transport-protocol");
Object.defineProperty(exports, "TransportProtocol", { enumerable: true, get: function () { return transport_protocol_1.TransportProtocol; } });
var tuple_1 = require("./fbs/transport/tuple");
Object.defineProperty(exports, "Tuple", { enumerable: true, get: function () { return tuple_1.Tuple; } });
Object.defineProperty(exports, "TupleT", { enumerable: true, get: function () { return tuple_1.TupleT; } });
var web_rtc_transport_dump_1 = require("./fbs/transport/web-rtc-transport-dump");
Object.defineProperty(exports, "WebRtcTransportDump", { enumerable: true, get: function () { return web_rtc_transport_dump_1.WebRtcTransportDump; } });
Object.defineProperty(exports, "WebRtcTransportDumpT", { enumerable: true, get: function () { return web_rtc_transport_dump_1.WebRtcTransportDumpT; } });
var ice_user_name_fragment_1 = require("./fbs/web-rtc-server/ice-user-name-fragment");
Object.defineProperty(exports, "IceUserNameFragment", { enumerable: true, get: function () { return ice_user_name_fragment_1.IceUserNameFragment; } });
Object.defineProperty(exports, "IceUserNameFragmentT", { enumerable: true, get: function () { return ice_user_name_fragment_1.IceUserNameFragmentT; } });
var ip_port_1 = require("./fbs/web-rtc-server/ip-port");
Object.defineProperty(exports, "IpPort", { enumerable: true, get: function () { return ip_port_1.IpPort; } });
Object.defineProperty(exports, "IpPortT", { enumerable: true, get: function () { return ip_port_1.IpPortT; } });
var tuple_hash_1 = require("./fbs/web-rtc-server/tuple-hash");
Object.defineProperty(exports, "TupleHash", { enumerable: true, get: function () { return tuple_hash_1.TupleHash; } });
Object.defineProperty(exports, "TupleHashT", { enumerable: true, get: function () { return tuple_hash_1.TupleHashT; } });
var web_rtc_server_dump_1 = require("./fbs/web-rtc-server/web-rtc-server-dump");
Object.defineProperty(exports, "WebRtcServerDump", { enumerable: true, get: function () { return web_rtc_server_dump_1.WebRtcServerDump; } });
Object.defineProperty(exports, "WebRtcServerDumpT", { enumerable: true, get: function () { return web_rtc_server_dump_1.WebRtcServerDumpT; } });
var web_rtc_server_listen_info_1 = require("./fbs/web-rtc-server/web-rtc-server-listen-info");
Object.defineProperty(exports, "WebRtcServerListenInfo", { enumerable: true, get: function () { return web_rtc_server_listen_info_1.WebRtcServerListenInfo; } });
Object.defineProperty(exports, "WebRtcServerListenInfoT", { enumerable: true, get: function () { return web_rtc_server_listen_info_1.WebRtcServerListenInfoT; } });
var web_rtc_transport_listen_1 = require("./fbs/web-rtc-transport/web-rtc-transport-listen");
Object.defineProperty(exports, "WebRtcTransportListen", { enumerable: true, get: function () { return web_rtc_transport_listen_1.WebRtcTransportListen; } });
Object.defineProperty(exports, "unionToWebRtcTransportListen", { enumerable: true, get: function () { return web_rtc_transport_listen_1.unionToWebRtcTransportListen; } });
Object.defineProperty(exports, "unionListToWebRtcTransportListen", { enumerable: true, get: function () { return web_rtc_transport_listen_1.unionListToWebRtcTransportListen; } });
var web_rtc_transport_listen_individual_1 = require("./fbs/web-rtc-transport/web-rtc-transport-listen-individual");
Object.defineProperty(exports, "WebRtcTransportListenIndividual", { enumerable: true, get: function () { return web_rtc_transport_listen_individual_1.WebRtcTransportListenIndividual; } });
Object.defineProperty(exports, "WebRtcTransportListenIndividualT", { enumerable: true, get: function () { return web_rtc_transport_listen_individual_1.WebRtcTransportListenIndividualT; } });
var web_rtc_transport_listen_server_1 = require("./fbs/web-rtc-transport/web-rtc-transport-listen-server");
Object.defineProperty(exports, "WebRtcTransportListenServer", { enumerable: true, get: function () { return web_rtc_transport_listen_server_1.WebRtcTransportListenServer; } });
Object.defineProperty(exports, "WebRtcTransportListenServerT", { enumerable: true, get: function () { return web_rtc_transport_listen_server_1.WebRtcTransportListenServerT; } });
var web_rtc_transport_options_1 = require("./fbs/web-rtc-transport/web-rtc-transport-options");
Object.defineProperty(exports, "WebRtcTransportOptions", { enumerable: true, get: function () { return web_rtc_transport_options_1.WebRtcTransportOptions; } });
Object.defineProperty(exports, "WebRtcTransportOptionsT", { enumerable: true, get: function () { return web_rtc_transport_options_1.WebRtcTransportOptionsT; } });
var channel_message_handlers_1 = require("./fbs/worker/channel-message-handlers");
Object.defineProperty(exports, "ChannelMessageHandlers", { enumerable: true, get: function () { return channel_message_handlers_1.ChannelMessageHandlers; } });
Object.defineProperty(exports, "ChannelMessageHandlersT", { enumerable: true, get: function () { return channel_message_handlers_1.ChannelMessageHandlersT; } });
var close_router_request_1 = require("./fbs/worker/close-router-request");
Object.defineProperty(exports, "CloseRouterRequest", { enumerable: true, get: function () { return close_router_request_1.CloseRouterRequest; } });
Object.defineProperty(exports, "CloseRouterRequestT", { enumerable: true, get: function () { return close_router_request_1.CloseRouterRequestT; } });
var close_web_rtc_server_request_1 = require("./fbs/worker/close-web-rtc-server-request");
Object.defineProperty(exports, "CloseWebRtcServerRequest", { enumerable: true, get: function () { return close_web_rtc_server_request_1.CloseWebRtcServerRequest; } });
Object.defineProperty(exports, "CloseWebRtcServerRequestT", { enumerable: true, get: function () { return close_web_rtc_server_request_1.CloseWebRtcServerRequestT; } });
var create_router_request_1 = require("./fbs/worker/create-router-request");
Object.defineProperty(exports, "CreateRouterRequest", { enumerable: true, get: function () { return create_router_request_1.CreateRouterRequest; } });
Object.defineProperty(exports, "CreateRouterRequestT", { enumerable: true, get: function () { return create_router_request_1.CreateRouterRequestT; } });
var create_web_rtc_server_request_1 = require("./fbs/worker/create-web-rtc-server-request");
Object.defineProperty(exports, "CreateWebRtcServerRequest", { enumerable: true, get: function () { return create_web_rtc_server_request_1.CreateWebRtcServerRequest; } });
Object.defineProperty(exports, "CreateWebRtcServerRequestT", { enumerable: true, get: function () { return create_web_rtc_server_request_1.CreateWebRtcServerRequestT; } });
var resource_usage_1 = require("./fbs/worker/resource-usage");
Object.defineProperty(exports, "ResourceUsage", { enumerable: true, get: function () { return resource_usage_1.ResourceUsage; } });
Object.defineProperty(exports, "ResourceUsageT", { enumerable: true, get: function () { return resource_usage_1.ResourceUsageT; } });
var updateable_settings_1 = require("./fbs/worker/updateable-settings");
Object.defineProperty(exports, "UpdateableSettings", { enumerable: true, get: function () { return updateable_settings_1.UpdateableSettings; } });
Object.defineProperty(exports, "UpdateableSettingsT", { enumerable: true, get: function () { return updateable_settings_1.UpdateableSettingsT; } });
var worker_dump_1 = require("./fbs/worker/worker-dump");
Object.defineProperty(exports, "WorkerDump", { enumerable: true, get: function () { return worker_dump_1.WorkerDump; } });
Object.defineProperty(exports, "WorkerDumpT", { enumerable: true, get: function () { return worker_dump_1.WorkerDumpT; } });
