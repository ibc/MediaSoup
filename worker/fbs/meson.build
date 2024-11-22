flatbuffers_schemas = [
  'activeSpeakerObserver.fbs',
  'audioLevelObserver.fbs',
  'common.fbs',
  'consumer.fbs',
  'dataConsumer.fbs',
  'dataProducer.fbs',
  'directTransport.fbs',
  'liburing.fbs',
  'log.fbs',
  'message.fbs',
  'notification.fbs',
  'pipeTransport.fbs',
  'plainTransport.fbs',
  'producer.fbs',
  'request.fbs',
  'response.fbs',
  'router.fbs',
  'rtpObserver.fbs',
  'rtpPacket.fbs',
  'rtpParameters.fbs',
  'rtpStream.fbs',
  'rtxStream.fbs',
  'sctpAssociation.fbs',
  'sctpParameters.fbs',
  'srtpParameters.fbs',
  'transport.fbs',
  'webRtcServer.fbs',
  'webRtcTransport.fbs',
  'worker.fbs',
]

# Directory from which worker code will include the header files.
flatbuffers_cpp_out_dir = 'FBS'

flatc = find_program('flatc')
flatbuffers_generator = custom_target('flatbuffers-generator',
  output: flatbuffers_cpp_out_dir,
  input: flatbuffers_schemas,
  command : [
    flatc,
    '--cpp',
    '--cpp-field-case-style', 'lower',
    '--reflect-names',
    '--scoped-enums',
    '--filename-suffix', '',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  build_by_default: true,
)

flatbuffers_generator_dep = declare_dependency(
  include_directories: '.',
)
