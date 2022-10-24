"use strict";
// automatically generated by the FlatBuffers compiler, do not modify
Object.defineProperty(exports, "__esModule", { value: true });
exports.WebRtcTransportListenIndividualT = exports.WebRtcTransportListenIndividual = void 0;
const flatbuffers = require("flatbuffers");
const transport_listen_ip_1 = require("../../fbs/transport/transport-listen-ip");
class WebRtcTransportListenIndividual {
    bb = null;
    bb_pos = 0;
    __init(i, bb) {
        this.bb_pos = i;
        this.bb = bb;
        return this;
    }
    static getRootAsWebRtcTransportListenIndividual(bb, obj) {
        return (obj || new WebRtcTransportListenIndividual()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
    }
    static getSizePrefixedRootAsWebRtcTransportListenIndividual(bb, obj) {
        bb.setPosition(bb.position() + flatbuffers.SIZE_PREFIX_LENGTH);
        return (obj || new WebRtcTransportListenIndividual()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
    }
    listenIps(index, obj) {
        const offset = this.bb.__offset(this.bb_pos, 4);
        return offset ? (obj || new transport_listen_ip_1.TransportListenIp()).__init(this.bb.__indirect(this.bb.__vector(this.bb_pos + offset) + index * 4), this.bb) : null;
    }
    listenIpsLength() {
        const offset = this.bb.__offset(this.bb_pos, 4);
        return offset ? this.bb.__vector_len(this.bb_pos + offset) : 0;
    }
    port() {
        const offset = this.bb.__offset(this.bb_pos, 6);
        return offset ? this.bb.readUint16(this.bb_pos + offset) : 0;
    }
    static startWebRtcTransportListenIndividual(builder) {
        builder.startObject(2);
    }
    static addListenIps(builder, listenIpsOffset) {
        builder.addFieldOffset(0, listenIpsOffset, 0);
    }
    static createListenIpsVector(builder, data) {
        builder.startVector(4, data.length, 4);
        for (let i = data.length - 1; i >= 0; i--) {
            builder.addOffset(data[i]);
        }
        return builder.endVector();
    }
    static startListenIpsVector(builder, numElems) {
        builder.startVector(4, numElems, 4);
    }
    static addPort(builder, port) {
        builder.addFieldInt16(1, port, 0);
    }
    static endWebRtcTransportListenIndividual(builder) {
        const offset = builder.endObject();
        builder.requiredField(offset, 4); // listen_ips
        return offset;
    }
    static createWebRtcTransportListenIndividual(builder, listenIpsOffset, port) {
        WebRtcTransportListenIndividual.startWebRtcTransportListenIndividual(builder);
        WebRtcTransportListenIndividual.addListenIps(builder, listenIpsOffset);
        WebRtcTransportListenIndividual.addPort(builder, port);
        return WebRtcTransportListenIndividual.endWebRtcTransportListenIndividual(builder);
    }
    unpack() {
        return new WebRtcTransportListenIndividualT(this.bb.createObjList(this.listenIps.bind(this), this.listenIpsLength()), this.port());
    }
    unpackTo(_o) {
        _o.listenIps = this.bb.createObjList(this.listenIps.bind(this), this.listenIpsLength());
        _o.port = this.port();
    }
}
exports.WebRtcTransportListenIndividual = WebRtcTransportListenIndividual;
class WebRtcTransportListenIndividualT {
    listenIps;
    port;
    constructor(listenIps = [], port = 0) {
        this.listenIps = listenIps;
        this.port = port;
    }
    pack(builder) {
        const listenIps = WebRtcTransportListenIndividual.createListenIpsVector(builder, builder.createObjectOffsetList(this.listenIps));
        return WebRtcTransportListenIndividual.createWebRtcTransportListenIndividual(builder, listenIps, this.port);
    }
}
exports.WebRtcTransportListenIndividualT = WebRtcTransportListenIndividualT;
