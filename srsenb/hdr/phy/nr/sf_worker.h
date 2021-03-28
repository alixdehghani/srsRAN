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

#ifndef SRSUE_NR_PHCH_WORKER_H
#define SRSUE_NR_PHCH_WORKER_H

#include "cc_worker.h"
#include "srsenb/hdr/phy/phy_common.h"
#include "srsran/common/thread_pool.h"
#include "srsran/srslog/srslog.h"

namespace srsenb {
namespace nr {

/**
 * The sf_worker class handles the PHY processing, UL and DL procedures associated with 1 subframe.
 * It contains multiple cc_worker objects, one for each component carrier which may be executed in
 * one or multiple threads.
 *
 * A sf_worker object is executed by a thread within the thread_pool.
 */

class sf_worker final : public srsran::thread_pool::worker
{
public:
  sf_worker(phy_common* phy_, phy_nr_state* phy_state_, srslog::basic_logger& logger);
  ~sf_worker();

  bool set_carrier_unlocked(uint32_t cc_idx, const srsran_carrier_nr_t* carrier_);

  /* Functions used by main PHY thread */
  cf_t*    get_buffer_rx(uint32_t cc_idx, uint32_t antenna_idx);
  cf_t*    get_buffer_tx(uint32_t cc_idx, uint32_t antenna_idx);
  uint32_t get_buffer_len();
  void     set_tti(uint32_t tti);

private:
  /* Inherited from thread_pool::worker. Function called every subframe to run the DL/UL processing */
  void work_imp() override;

  std::vector<std::unique_ptr<cc_worker> > cc_workers;

  phy_common*           phy       = nullptr;
  phy_nr_state*         phy_state = nullptr;
  srslog::basic_logger& logger;

  // Temporal attributes
  srsran_softbuffer_tx_t softbuffer_tx = {};
  std::vector<uint8_t>   data;
};

} // namespace nr
} // namespace srsenb

#endif // SRSUE_NR_PHCH_WORKER_H