"use strict";
// automatically generated by the FlatBuffers compiler, do not modify
Object.defineProperty(exports, "__esModule", { value: true });
exports.PipeTransportOptionsT = exports.PipeTransportOptions = void 0;
const flatbuffers = require("flatbuffers");
const base_transport_options_1 = require("../../fbs/transport/base-transport-options");
const transport_listen_ip_1 = require("../../fbs/transport/transport-listen-ip");
class PipeTransportOptions {
    bb = null;
    bb_pos = 0;
    __init(i, bb) {
        this.bb_pos = i;
        this.bb = bb;
        return this;
    }
    static getRootAsPipeTransportOptions(bb, obj) {
        return (obj || new PipeTransportOptions()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
    }
    static getSizePrefixedRootAsPipeTransportOptions(bb, obj) {
        bb.setPosition(bb.position() + flatbuffers.SIZE_PREFIX_LENGTH);
        return (obj || new PipeTransportOptions()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
    }
    base(obj) {
        const offset = this.bb.__offset(this.bb_pos, 4);
        return offset ? (obj || new base_transport_options_1.BaseTransportOptions()).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
    }
    listenIp(obj) {
        const offset = this.bb.__offset(this.bb_pos, 6);
        return offset ? (obj || new transport_listen_ip_1.TransportListenIp()).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
    }
    port() {
        const offset = this.bb.__offset(this.bb_pos, 8);
        return offset ? this.bb.readUint16(this.bb_pos + offset) : 0;
    }
    enableRtx() {
        const offset = this.bb.__offset(this.bb_pos, 10);
        return offset ? !!this.bb.readInt8(this.bb_pos + offset) : false;
    }
    enableSrtp() {
        const offset = this.bb.__offset(this.bb_pos, 12);
        return offset ? !!this.bb.readInt8(this.bb_pos + offset) : false;
    }
    static startPipeTransportOptions(builder) {
        builder.startObject(5);
    }
    static addBase(builder, baseOffset) {
        builder.addFieldOffset(0, baseOffset, 0);
    }
    static addListenIp(builder, listenIpOffset) {
        builder.addFieldOffset(1, listenIpOffset, 0);
    }
    static addPort(builder, port) {
        builder.addFieldInt16(2, port, 0);
    }
    static addEnableRtx(builder, enableRtx) {
        builder.addFieldInt8(3, +enableRtx, +false);
    }
    static addEnableSrtp(builder, enableSrtp) {
        builder.addFieldInt8(4, +enableSrtp, +false);
    }
    static endPipeTransportOptions(builder) {
        const offset = builder.endObject();
        builder.requiredField(offset, 6); // listen_ip
        return offset;
    }
    unpack() {
        return new PipeTransportOptionsT((this.base() !== null ? this.base().unpack() : null), (this.listenIp() !== null ? this.listenIp().unpack() : null), this.port(), this.enableRtx(), this.enableSrtp());
    }
    unpackTo(_o) {
        _o.base = (this.base() !== null ? this.base().unpack() : null);
        _o.listenIp = (this.listenIp() !== null ? this.listenIp().unpack() : null);
        _o.port = this.port();
        _o.enableRtx = this.enableRtx();
        _o.enableSrtp = this.enableSrtp();
    }
}
exports.PipeTransportOptions = PipeTransportOptions;
class PipeTransportOptionsT {
    base;
    listenIp;
    port;
    enableRtx;
    enableSrtp;
    constructor(base = null, listenIp = null, port = 0, enableRtx = false, enableSrtp = false) {
        this.base = base;
        this.listenIp = listenIp;
        this.port = port;
        this.enableRtx = enableRtx;
        this.enableSrtp = enableSrtp;
    }
    pack(builder) {
        const base = (this.base !== null ? this.base.pack(builder) : 0);
        const listenIp = (this.listenIp !== null ? this.listenIp.pack(builder) : 0);
        PipeTransportOptions.startPipeTransportOptions(builder);
        PipeTransportOptions.addBase(builder, base);
        PipeTransportOptions.addListenIp(builder, listenIp);
        PipeTransportOptions.addPort(builder, this.port);
        PipeTransportOptions.addEnableRtx(builder, this.enableRtx);
        PipeTransportOptions.addEnableSrtp(builder, this.enableSrtp);
        return PipeTransportOptions.endPipeTransportOptions(builder);
    }
}
exports.PipeTransportOptionsT = PipeTransportOptionsT;
