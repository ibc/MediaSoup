// automatically generated by the FlatBuffers compiler, do not modify

import * as flatbuffers from 'flatbuffers';

import { DumpResponse, DumpResponseT } from '../../fbs/consumer/dump-response';
import { Dump, DumpT } from '../../fbs/rtp-stream/dump';


export class PipeConsumerDump {
  bb: flatbuffers.ByteBuffer|null = null;
  bb_pos = 0;
  __init(i:number, bb:flatbuffers.ByteBuffer):PipeConsumerDump {
  this.bb_pos = i;
  this.bb = bb;
  return this;
}

static getRootAsPipeConsumerDump(bb:flatbuffers.ByteBuffer, obj?:PipeConsumerDump):PipeConsumerDump {
  return (obj || new PipeConsumerDump()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
}

static getSizePrefixedRootAsPipeConsumerDump(bb:flatbuffers.ByteBuffer, obj?:PipeConsumerDump):PipeConsumerDump {
  bb.setPosition(bb.position() + flatbuffers.SIZE_PREFIX_LENGTH);
  return (obj || new PipeConsumerDump()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
}

base(obj?:DumpResponse):DumpResponse|null {
  const offset = this.bb!.__offset(this.bb_pos, 4);
  return offset ? (obj || new DumpResponse()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
}

rtpStream(index: number, obj?:Dump):Dump|null {
  const offset = this.bb!.__offset(this.bb_pos, 6);
  return offset ? (obj || new Dump()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
}

rtpStreamLength():number {
  const offset = this.bb!.__offset(this.bb_pos, 6);
  return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
}

static startPipeConsumerDump(builder:flatbuffers.Builder) {
  builder.startObject(2);
}

static addBase(builder:flatbuffers.Builder, baseOffset:flatbuffers.Offset) {
  builder.addFieldOffset(0, baseOffset, 0);
}

static addRtpStream(builder:flatbuffers.Builder, rtpStreamOffset:flatbuffers.Offset) {
  builder.addFieldOffset(1, rtpStreamOffset, 0);
}

static createRtpStreamVector(builder:flatbuffers.Builder, data:flatbuffers.Offset[]):flatbuffers.Offset {
  builder.startVector(4, data.length, 4);
  for (let i = data.length - 1; i >= 0; i--) {
    builder.addOffset(data[i]!);
  }
  return builder.endVector();
}

static startRtpStreamVector(builder:flatbuffers.Builder, numElems:number) {
  builder.startVector(4, numElems, 4);
}

static endPipeConsumerDump(builder:flatbuffers.Builder):flatbuffers.Offset {
  const offset = builder.endObject();
  builder.requiredField(offset, 4) // base
  builder.requiredField(offset, 6) // rtp_stream
  return offset;
}

static createPipeConsumerDump(builder:flatbuffers.Builder, baseOffset:flatbuffers.Offset, rtpStreamOffset:flatbuffers.Offset):flatbuffers.Offset {
  PipeConsumerDump.startPipeConsumerDump(builder);
  PipeConsumerDump.addBase(builder, baseOffset);
  PipeConsumerDump.addRtpStream(builder, rtpStreamOffset);
  return PipeConsumerDump.endPipeConsumerDump(builder);
}

unpack(): PipeConsumerDumpT {
  return new PipeConsumerDumpT(
    (this.base() !== null ? this.base()!.unpack() : null),
    this.bb!.createObjList(this.rtpStream.bind(this), this.rtpStreamLength())
  );
}


unpackTo(_o: PipeConsumerDumpT): void {
  _o.base = (this.base() !== null ? this.base()!.unpack() : null);
  _o.rtpStream = this.bb!.createObjList(this.rtpStream.bind(this), this.rtpStreamLength());
}
}

export class PipeConsumerDumpT {
constructor(
  public base: DumpResponseT|null = null,
  public rtpStream: (DumpT)[] = []
){}


pack(builder:flatbuffers.Builder): flatbuffers.Offset {
  const base = (this.base !== null ? this.base!.pack(builder) : 0);
  const rtpStream = PipeConsumerDump.createRtpStreamVector(builder, builder.createObjectOffsetList(this.rtpStream));

  return PipeConsumerDump.createPipeConsumerDump(builder,
    base,
    rtpStream
  );
}
}
