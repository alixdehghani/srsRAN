/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSRAN_PDU_QUEUE_H
#define SRSRAN_PDU_QUEUE_H

#include "srsran/adt/circular_buffer.h"
#include "srsran/common/block_queue.h"
#include "srsran/common/buffer_pool.h"
#include "srsran/common/timers.h"
#include "srsran/mac/pdu.h"

/* Logical Channel Demultiplexing and MAC CE dissassemble */

namespace srsran {

class pdu_queue
{
public:
  typedef enum { DCH, BCH, MCH } channel_t;
  class process_callback
  {
  public:
    virtual void process_pdu(uint8_t* buff, uint32_t len, channel_t channel) = 0;
  };

  pdu_queue(srslog::basic_logger& logger) : pool(DEFAULT_POOL_SIZE), callback(NULL), logger(logger) {}
  void init(process_callback* callback);

  uint8_t* request(uint32_t len);
  void     deallocate(const uint8_t* pdu);
  void     push(const uint8_t* ptr, uint32_t len, channel_t channel = DCH);

  bool process_pdus();

  void reset();

private:
  const static int DEFAULT_POOL_SIZE = 128;            // Number of PDU buffers in total
  const static int MAX_PDU_LEN       = 150 * 1024 / 8; // ~ 150 Mbps

  typedef struct {
    uint8_t   ptr[MAX_PDU_LEN];
    uint32_t  len;
    channel_t channel;
#ifdef SRSRAN_BUFFER_POOL_LOG_ENABLED
    char debug_name[128];
#endif

  } pdu_t;

  static_blocking_queue<pdu_t*, DEFAULT_POOL_SIZE> pdu_q;
  buffer_pool<pdu_t>                               pool;

  process_callback*     callback;
  srslog::basic_logger& logger;
};

} // namespace srsran

#endif // SRSRAN_PDU_QUEUE_H