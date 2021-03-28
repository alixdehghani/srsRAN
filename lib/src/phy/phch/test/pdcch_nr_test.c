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

#include "srsran/common/test_common.h"
#include "srsran/phy/phch/pdcch_nr.h"
#include "srsran/phy/utils/random.h"
#include <getopt.h>

static srsran_carrier_nr_t carrier = {
    0,  // cell_id
    0,  // numerology
    50, // nof_prb
    0,  // start
    1   // max_mimo_layers
};

static uint16_t rnti       = 0x1234;
static bool     fast_sweep = true;

typedef struct {
  uint64_t time_us;
  uint64_t count;
} proc_time_t;

static proc_time_t enc_time[SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR] = {};
static proc_time_t dec_time[SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR] = {};

static int test(srsran_pdcch_nr_t*      tx,
                srsran_pdcch_nr_t*      rx,
                cf_t*                   grid,
                srsran_dmrs_pdcch_ce_t* ce,
                srsran_dci_msg_nr_t*    dci_msg_tx)
{
  // Encode PDCCH
  TESTASSERT(srsran_pdcch_nr_encode(tx, dci_msg_tx, grid) == SRSRAN_SUCCESS);

  enc_time[dci_msg_tx->location.L].time_us += tx->meas_time_us;
  enc_time[dci_msg_tx->location.L].count++;

  // Init Rx MSG
  srsran_pdcch_nr_res_t res        = {};
  srsran_dci_msg_nr_t   dci_msg_rx = *dci_msg_tx;
  srsran_vec_u8_zero(dci_msg_rx.payload, dci_msg_rx.nof_bits);

  // Decode PDCCH
  TESTASSERT(srsran_pdcch_nr_decode(rx, grid, ce, &dci_msg_rx, &res) == SRSRAN_SUCCESS);

  dec_time[dci_msg_tx->location.L].time_us += rx->meas_time_us;
  dec_time[dci_msg_tx->location.L].count++;

  // Assert
  TESTASSERT(res.evm < 0.01f);
  TESTASSERT(res.crc);

  return SRSRAN_SUCCESS;
}

static void usage(char* prog)
{
  printf("Usage: %s [pFv] \n", prog);
  printf("\t-p Number of carrier PRB [Default %d]\n", carrier.nof_prb);
  printf("\t-F Fast CORESET frequency resource sweeping [Default %s]\n", fast_sweep ? "Enabled" : "Disabled");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

static int parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "pFv")) != -1) {
    switch (opt) {
      case 'p':
        carrier.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'F':
        fast_sweep ^= true;
        break;
      case 'v':
        srsran_verbose++;
        break;
      default:
        usage(argv[0]);
        return SRSRAN_ERROR;
    }
  }

  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
  int ret = SRSRAN_ERROR;

  srsran_pdcch_nr_args_t args = {};
  args.disable_simd           = false;
  args.measure_evm            = true;
  args.measure_time           = true;

  srsran_pdcch_nr_t pdcch_tx = {};
  srsran_pdcch_nr_t pdcch_rx = {};

  if (parse_args(argc, argv) < SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  uint32_t                grid_sz  = carrier.nof_prb * SRSRAN_NRE * SRSRAN_NSYMB_PER_SLOT_NR;
  srsran_random_t         rand_gen = srsran_random_init(1234);
  srsran_dmrs_pdcch_ce_t* ce       = SRSRAN_MEM_ALLOC(srsran_dmrs_pdcch_ce_t, 1);
  cf_t*                   buffer   = srsran_vec_cf_malloc(grid_sz);
  if (rand_gen == NULL || ce == NULL || buffer == NULL) {
    ERROR("Error malloc");
    goto clean_exit;
  }

  SRSRAN_MEM_ZERO(ce, srsran_dmrs_pdcch_ce_t, 1);

  if (srsran_pdcch_nr_init_tx(&pdcch_tx, &args) < SRSRAN_SUCCESS) {
    ERROR("Error init");
    goto clean_exit;
  }

  if (srsran_pdcch_nr_init_rx(&pdcch_rx, &args) < SRSRAN_SUCCESS) {
    ERROR("Error init");
    goto clean_exit;
  }

  srsran_coreset_t coreset                = {};
  uint32_t         nof_frequency_resource = SRSRAN_MIN(SRSRAN_CORESET_FREQ_DOMAIN_RES_SIZE, carrier.nof_prb / 6);
  for (uint32_t frequency_resources = 1; frequency_resources < (1U << nof_frequency_resource);
       frequency_resources          = (fast_sweep) ? ((frequency_resources << 1U) | 1U) : (frequency_resources + 1)) {
    for (uint32_t i = 0; i < nof_frequency_resource; i++) {
      uint32_t mask             = ((frequency_resources >> i) & 1U);
      coreset.freq_resources[i] = (mask == 1);
    }
    for (coreset.duration = SRSRAN_CORESET_DURATION_MIN; coreset.duration <= SRSRAN_CORESET_DURATION_MAX;
         coreset.duration++) {
      srsran_search_space_t search_space = {};
      search_space.type                  = srsran_search_space_type_ue;

      if (srsran_pdcch_nr_set_carrier(&pdcch_tx, &carrier, &coreset) < SRSRAN_SUCCESS) {
        ERROR("Error setting carrier");
        goto clean_exit;
      }

      if (srsran_pdcch_nr_set_carrier(&pdcch_rx, &carrier, &coreset) < SRSRAN_SUCCESS) {
        ERROR("Error setting carrier");
        goto clean_exit;
      }

      // Fill search space maximum number of candidates
      for (uint32_t aggregation_level = 0; aggregation_level < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR;
           aggregation_level++) {
        search_space.nof_candidates[aggregation_level] =
            srsran_pdcch_nr_max_candidates_coreset(&coreset, aggregation_level);
      }

      for (uint32_t aggregation_level = 0; aggregation_level < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR;
           aggregation_level++) {
        uint32_t L = 1U << aggregation_level;

        for (uint32_t slot_idx = 0; slot_idx < SRSRAN_NSLOTS_PER_FRAME_NR(carrier.numerology); slot_idx++) {
          uint32_t dci_locations[SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR] = {};

          // Calculate candidate locations
          int n = srsran_pdcch_nr_locations_coreset(
              &coreset, &search_space, rnti, aggregation_level, slot_idx, dci_locations);
          if (n < SRSRAN_SUCCESS) {
            ERROR("Error calculating locations in CORESET");
            goto clean_exit;
          }

          // Skip if no candidates
          if (n == 0) {
            continue;
          }

          for (uint32_t ncce_idx = 0; ncce_idx < n; ncce_idx++) {
            // Init MSG
            srsran_dci_msg_nr_t dci_msg = {};
            dci_msg.format              = srsran_dci_format_nr_1_0;
            dci_msg.rnti_type           = srsran_rnti_type_c;
            dci_msg.location.L          = aggregation_level;
            dci_msg.location.ncce       = dci_locations[ncce_idx];
            dci_msg.nof_bits            = srsran_dci_nr_format_1_0_sizeof(&carrier, &coreset, dci_msg.rnti_type);

            // Generate random payload
            for (uint32_t i = 0; i < dci_msg.nof_bits; i++) {
              dci_msg.payload[i] = srsran_random_uniform_int_dist(rand_gen, 0, 1);
            }

            // Set channel estimate number of elements and set out-of-range values to zero
            ce->nof_re = (SRSRAN_NRE - 3) * 6 * L;
            for (uint32_t i = 0; i < SRSRAN_PDCCH_MAX_RE; i++) {
              ce->ce[i] = (i < ce->nof_re) ? 1.0f : 0.0f;
            }
            ce->noise_var = 0.0f;

            if (test(&pdcch_tx, &pdcch_rx, buffer, ce, &dci_msg) < SRSRAN_SUCCESS) {
              ERROR("test failed");
              goto clean_exit;
            }
          }
        }
      }
    }
  }

  printf("+--------+--------+--------+--------+\n");
  printf("| %6s | %6s | %6s | %6s |\n", " ", " ", " Time ", " Time ");
  printf("| %6s | %6s | %6s | %6s |\n", "  L  ", "Count", "Encode", "Decode");
  printf("| %6s | %6s | %6s | %6s |\n", " ", " ", " (us) ", " (us) ");
  printf("+--------+--------+--------+--------+\n");
  for (uint32_t i = 0; i < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR; i++) {
    if (enc_time[i].count > 0 && dec_time[i].count) {
      printf("| %6" PRIu32 "| %6" PRIu64 " | %6.1f | %6.1f |\n",
             i,
             enc_time[i].count,
             (double)enc_time[i].time_us / (double)enc_time[i].count,
             (double)dec_time[i].time_us / (double)dec_time[i].count);
    }
  }
  printf("+--------+--------+--------+--------+\n");

  ret = SRSRAN_SUCCESS;
clean_exit:
  srsran_random_free(rand_gen);

  if (ce) {
    free(ce);
  }

  if (buffer) {
    free(buffer);
  }

  srsran_pdcch_nr_free(&pdcch_tx);
  srsran_pdcch_nr_free(&pdcch_rx);

  if (ret == SRSRAN_SUCCESS) {
    printf("Passed!\n");
  } else {
    printf("Failed!\n");
  }

  return ret;
}