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

#ifndef SRSRAN_PUSCH_CFG_H
#define SRSRAN_PUSCH_CFG_H

#include "srsran/phy/fec/softbuffer.h"
#include "srsran/phy/phch/ra.h"
#include "srsran/phy/phch/uci_cfg.h"

typedef struct SRSRAN_API {
  uint32_t I_offset_cqi;
  uint32_t I_offset_ri;
  uint32_t I_offset_ack;
} srsran_uci_offset_cfg_t;

typedef struct {
  enum { SRSRAN_PUSCH_HOP_MODE_INTER_SF = 1, SRSRAN_PUSCH_HOP_MODE_INTRA_SF = 0 } hop_mode;
  uint32_t hopping_offset;
  uint32_t n_sb;
  uint32_t n_rb_ho;
  uint32_t current_tx_nb;
  bool     hopping_enabled;
} srsran_pusch_hopping_cfg_t;

typedef struct SRSRAN_API {

  uint32_t       L_prb;
  uint32_t       n_prb[2];       // rb_start after frequency hopping
  uint32_t       n_prb_tilde[2]; // rb_start after frequency hopping per retx
  uint32_t       freq_hopping;
  uint32_t       nof_re;
  uint32_t       nof_symb;
  srsran_ra_tb_t tb;
  srsran_ra_tb_t last_tb;
  uint32_t       n_dmrs;
  bool           is_rar;

} srsran_pusch_grant_t;

typedef struct SRSRAN_API {

  uint16_t rnti;

  srsran_uci_cfg_t        uci_cfg;
  srsran_uci_offset_cfg_t uci_offset;
  srsran_pusch_grant_t    grant;

  uint32_t max_nof_iterations;
  uint32_t last_O_cqi;
  uint32_t K_segm;
  uint32_t current_tx_nb;
  bool     csi_enable;
  bool     enable_64qam;

  union {
    srsran_softbuffer_tx_t* tx;
    srsran_softbuffer_rx_t* rx;
  } softbuffers;

  bool     meas_time_en;
  uint32_t meas_time_value;

  bool meas_epre_en;
  bool meas_ta_en;
  bool meas_evm_en;

} srsran_pusch_cfg_t;

#endif // SRSRAN_PUSCH_CFG_H