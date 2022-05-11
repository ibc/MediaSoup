#ifndef MS_LIVELY_BIN_LOGS_HPP
#define MS_LIVELY_BIN_LOGS_HPP

#include "common.hpp"
#include <cstring>
#include "RTC/RtpStream.hpp"

/*
  BUGBUG: until epoch_len is a "relative timestamp", not a time diff btw samples,
  see that CALL_STATS_BIN_LOG_RECORDS_NUM * CALL_STATS_BIN_LOG_SAMPLING
  fits into uint16_t
*/
#define CALL_STATS_BIN_LOG_RECORDS_NUM 5
#define CALL_STATS_BIN_LOG_SAMPLING    10000
#define UINT32_UNSET                   ((uint32_t)-1)
#define UINT64_UNSET                   ((uint64_t)-1)
#define ZERO_UUID "00000000-0000-0000-0000-000000000000"


namespace Lively
{
  // Data sample
  struct CallStatsSample
  {
    uint16_t epoch_len;             // epoch duration in milliseconds
    uint16_t packets_count;         // RTP packets per epoch
    uint16_t packets_lost;
    uint16_t packets_discarded;
    uint16_t packets_retransmitted;
    uint16_t packets_repaired;
    uint16_t nack_count;            // number of NACK requests
    uint16_t nack_pkt_count;        // number of NACK packets requested
    uint16_t kf_count;              // Requests for key frame, PLI or FIR, see RequestKeyFrame()
    uint16_t rtt;
    uint32_t max_pts;
    uint32_t bytes_count;
  };


  //Header sizes of data written into bin log
  // BUGBUG: currently header size is aligned to 16 bytes so filled and payload are uint32_t.
  // They can be uint8_t if a go parser is careful about reading data from files
  //sizeof timestamp filled payload
  //       8         4      4
  #define PRODUCER_REC_HEADER_SIZE 16
  //sizeof timestamp filled payload consumer_uuid producer_uuid
  //       8          4     4       16            16           
  #define CONSUMER_REC_HEADER_SIZE 48
  
  /* Data record: a header followed array of data samples. This structure is NOT how data is written out
  Headers different for producer and consumer, followed by CallsStatsSample array of CALL_STATS_BIN_LOG_RECORDS_NUM

  typedef struct {
  uint64_t       start_tm;                   // the record start timestamp in milliseconds
  uint32_t       filled;                      // number of filled records in the array below
  uint32_t       payload;                     // payload as in original RTP stream
  char           consumer_id [UUID_BYTE_LEN]; // 
  char           producer_id [UUID_BYTE_LEN]; // 
} stats_consumer_record_header_t;

typedef struct {
  uint64_t       start_tm;                   // the record start timestamp in milliseconds
  uint32_t       filled;                      // number of filled records in the array below
  uint32_t       payload;                     // payload as in original RTP stream
} stats_producer_record_header_t;
  */
  class CallStatsRecord
  {
  public:
    std::string       call_id;                                 // call id: uuid4()
    std::string       object_id;                               // uuid4(), producer or consumer id, depending on source
    std::string       producer_id;                             // uuid4(), undef if source is producer, or consumer's corresponding producer id
    uint64_t          start_tm {UINT64_UNSET};                 // the record start timestamp in milliseconds
    uint8_t           source {0};                              // 0 for producer or 1 for consumer
    uint32_t          payload {0};                             // payload, matches value set in RTP stream
    uint32_t          filled {UINT32_UNSET};                    // number of filled records in the array below
    CallStatsSample   samples[CALL_STATS_BIN_LOG_RECORDS_NUM]; // collection of data samples
  
  public:
    CallStatsRecord(uint64_t objType, uint8_t payload, std::string callId, std::string objId, std::string producerId);

    bool fwriteRecord(std::FILE* fd);

    void resetSamples()
    {
      // Wipe off samples data
      std::memset(samples, 0, sizeof(samples));
      filled = UINT32_UNSET;
      start_tm = UINT64_UNSET;
      memcpy(write_buf, &start_tm, sizeof(uint64_t));
    }
    
  private:
    uint8_t  write_buf[CONSUMER_REC_HEADER_SIZE];      // temp buffer to form record header data for bin log output

    void fillHeader();
    bool uuidToBytes(std::string uuid, uint8_t *out);
    uint8_t* hexStrToBytes(const char* from, int num, uint8_t *to);
  };


  class StatsBinLog;


  // Data collecting management:
  // Collects data samples,
  // forms data records,
  // dumps data out into a file
  class CallStatsRecordCtx
  {
  public:
    CallStatsRecord record;
  
  private:
    class InputStats
    {
    public:
      size_t packetsCount;
      size_t bytesCount;
      uint32_t packetsLost;
      size_t packetsDiscarded;
      size_t packetsRetransmitted;
      size_t packetsRepaired;
      size_t nackCount;
      size_t nackPacketCount;
      size_t kfCount;
      float rtt;
      uint32_t maxPacketTs;
    };

    InputStats last {};
    InputStats curr {};

  public:
    CallStatsRecordCtx(uint64_t objType, uint8_t payload, std::string callId, std::string objId, std::string producerId) : record(objType, payload, callId, objId, producerId) {}

    void AddStatsRecord(StatsBinLog* log, RTC::RtpStream* stream); // either recv or send stream

  private:
    void WriteIfFull(StatsBinLog* log);
  };


  // Binary log presentation
  class StatsBinLog
  {
  public:
    std::string   bin_log_file_path;                               // binary log's full file name: combo of call id, timestamp and "version"
    std::FILE*    fd {0};                                          
    uint64_t      sampling_interval {CALL_STATS_BIN_LOG_SAMPLING}; // frequency of collecting samples, non-configurable
  
  private:
    bool          initialized {false};
    std::string   bin_log_name_template;       // Log name template, use to rotate log, keep same name except for timestamp
    uint64_t      log_start_ts {UINT64_UNSET}; // Timestamp included into log's name; used to rotate logs daily

  public:
    StatsBinLog() = default;

    int LogOpen();
    int OnLogWrite(CallStatsRecordCtx* ctx);
    int LogClose(CallStatsRecordCtx* recordCtx);

    void InitLog(char type, std::string id1, std::string id2); // if type is producer, then log name is a combo of callid, producerid and timestamp
    void DeinitLog(CallStatsRecordCtx* recordCtx);  // Closes log file and deinitializes state variables

  private:
    void UpdateLogName();
  };
} //Lively

#endif // MS_LIVELY_BIN_LOGS_HPP