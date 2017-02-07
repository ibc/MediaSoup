'use strict';

let supportedRtpCapabilities =
{
	codecs :
	[
		{
			kind         : 'audio',
			name         : 'audio/opus',
			clockRate    : 48000,
			numChannels  : 2,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/PCMU',
			clockRate    : 8000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/PCMA',
			clockRate    : 8000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/ISAC',
			clockRate    : 32000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/ISAC',
			clockRate    : 16000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/G722',
			clockRate    : 8000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/CN',
			clockRate    : 32000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/CN',
			clockRate    : 16000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/CN',
			clockRate    : 8000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/CN',
			clockRate    : 32000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/telephone-event',
			clockRate    : 48000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/telephone-event',
			clockRate    : 32000,
			rtcpFeedback : []
		},

		{
			kind         : 'audio',
			name         : 'audio/telephone-event',
			clockRate    : 16000,
			rtcpFeedback : []
		},
		{
			kind         : 'audio',
			name         : 'audio/telephone-event',
			clockRate    : 8000,
			rtcpFeedback : []
		},
		{
			kind         : 'video',
			name         : 'video/VP8',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    }, // Locally consumed.
				{ type: 'nack', parameter: 'pli'  }, // Bypassed.
				{ type: 'nack', parameter: 'sli'  }, // Bypassed.
				{ type: 'nack', parameter: 'rpsi' }, // Bypassed.
				{ type: 'nack', parameter: 'app'  }, // Bypassed.
				{ type: 'ccm',  parameter: 'fir'  }, // Bypassed.
				{ type: 'ack',  parameter: 'rpsi' }, // Bypassed.
				{ type: 'ack',  parameter: 'app'  }, // Bypassed.
				{ type: 'transport-cc'            }  // Bypassed.
			]
		},
		{
			kind         : 'video',
			name         : 'video/VP9',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    },
				{ type: 'nack', parameter: 'pli'  },
				{ type: 'nack', parameter: 'sli'  },
				{ type: 'nack', parameter: 'rpsi' },
				{ type: 'nack', parameter: 'app'  },
				{ type: 'ccm',  parameter: 'fir'  },
				{ type: 'ack',  parameter: 'rpsi' },
				{ type: 'ack',  parameter: 'app'  },
				{ type: 'transport-cc'            }
			]
		},
		{
			kind         : 'video',
			name         : 'video/H264',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    },
				{ type: 'nack', parameter: 'pli'  },
				{ type: 'nack', parameter: 'sli'  },
				{ type: 'nack', parameter: 'rpsi' },
				{ type: 'nack', parameter: 'app'  },
				{ type: 'ccm',  parameter: 'fir'  },
				{ type: 'ack',  parameter: 'rpsi' },
				{ type: 'ack',  parameter: 'app'  },
				{ type: 'transport-cc'            }
			],
			parameters :
			{
				packetizationMode : 0
			}
		},
		{
			kind         : 'video',
			name         : 'video/H264',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    },
				{ type: 'nack', parameter: 'pli'  },
				{ type: 'nack', parameter: 'sli'  },
				{ type: 'nack', parameter: 'rpsi' },
				{ type: 'nack', parameter: 'app'  },
				{ type: 'ccm',  parameter: 'fir'  },
				{ type: 'ack',  parameter: 'rpsi' },
				{ type: 'ack',  parameter: 'app'  },
				{ type: 'transport-cc'            }
			],
			parameters :
			{
				packetizationMode : 1
			}
		},
		{
			kind         : 'video',
			name         : 'video/H265',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    },
				{ type: 'nack', parameter: 'pli'  },
				{ type: 'nack', parameter: 'sli'  },
				{ type: 'nack', parameter: 'rpsi' },
				{ type: 'nack', parameter: 'app'  },
				{ type: 'ccm',  parameter: 'fir'  },
				{ type: 'ack',  parameter: 'rpsi' },
				{ type: 'ack',  parameter: 'app'  },
				{ type: 'transport-cc'            }
			],
			parameters :
			{
				packetizationMode : 0
			}
		},
		{
			kind         : 'video',
			name         : 'video/H265',
			clockRate    : 90000,
			rtcpFeedback :
			[
				{ type: 'nack'                    },
				{ type: 'nack', parameter: 'pli'  },
				{ type: 'nack', parameter: 'sli'  },
				{ type: 'nack', parameter: 'rpsi' },
				{ type: 'nack', parameter: 'app'  },
				{ type: 'ccm',  parameter: 'fir'  },
				{ type: 'ack',  parameter: 'rpsi' },
				{ type: 'ack',  parameter: 'app'  },
				{ type: 'transport-cc'            }
			],
			parameters :
			{
				packetizationMode : 1
			}
		}
	],
	headerExtensions :
	[
		{
			kind             : 'audio',
			uri              : 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
			preferredId      : 1,
			preferredEncrypt : false
		},
		{
			kind             : 'video',
			uri              : 'urn:ietf:params:rtp-hdrext:toffset',
			preferredId      : 2,
			preferredEncrypt : false
		},
		{
			kind             : '',
			uri              : 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
			preferredId      : 3,
			preferredEncrypt : false
		},
		{
			kind             : 'video',
			uri              : 'urn:3gpp:video-orientation',
			preferredId      : 4,
			preferredEncrypt : false
		},
		{
			kind             : '',
			uri              : 'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id',
			preferredId      : 5,
			preferredEncrypt : false
		}
		// {
		// 	kind             : 'video',
		// 	uri              : 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01',
		// 	preferredId      : 6,
		// 	preferredEncrypt : false
		// },
		// {
		// 	kind             : 'video',
		// 	uri              : 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
		// 	preferredId      : 7,
		// 	preferredEncrypt : false
		// }
	],
	fecMechanisms : []
};

module.exports = supportedRtpCapabilities;
