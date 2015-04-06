/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2014 The srsLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * A copy of the GNU Lesser General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "srslte/srslte.h"

#define MAX_CANDIDATES 64

char *input_file_name = NULL;

srslte_cell_t cell = {
  6,            // nof_prb
  1,            // nof_ports
  0,            // bw_idx
  0,            // cell_id
  SRSLTE_CP_NORM,       // cyclic prefix
  SRSLTE_PHICH_R_1,          // PHICH resources      
  SRSLTE_PHICH_NORM    // PHICH length
};

int flen;

uint32_t cfi = 2;
uint16_t rnti = SRSLTE_SIRNTI;

int max_frames = 10;
uint32_t sf_idx = 0;

srslte_dci_format_t dci_format = SRSLTE_DCI_FORMAT1A;
srslte_filesource_t fsrc;
srslte_pdcch_t pdcch;
srslte_pdsch_t pdsch;
srslte_harq_t harq_process;
cf_t *input_buffer, *fft_buffer, *ce[SRSLTE_MAX_PORTS];
srslte_regs_t regs;
srslte_ofdm_t fft;
srslte_chest_dl_t chest;

void usage(char *prog) {
  printf("Usage: %s [rovfcenmps] -i input_file\n", prog);
  printf("\t-o DCI format [Default %s]\n", srslte_dci_format_string(dci_format));
  printf("\t-c cell.id [Default %d]\n", cell.id);
  printf("\t-s Start subframe_idx [Default %d]\n", sf_idx);
  printf("\t-f cfi [Default %d]\n", cfi);
  printf("\t-r rnti [Default 0x%x]\n",rnti);
  printf("\t-p cell.nof_ports [Default %d]\n", cell.nof_ports);
  printf("\t-n cell.nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-m max_frames [Default %d]\n", max_frames);
  printf("\t-e Set extended prefix [Default Normal]\n");
  printf("\t-v [set srslte_verbose to debug, default none]\n");
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "irovfcenmps")) != -1) {
    switch(opt) {
    case 'i':
      input_file_name = argv[optind];
      break;
    case 'c':
      cell.id = atoi(argv[optind]);
      break;
    case 's':
      sf_idx = atoi(argv[optind]);
      break;
    case 'r':
      rnti = strtoul(argv[optind], NULL, 0);
      break;
    case 'm':
      max_frames = strtoul(argv[optind], NULL, 0);
      break;
    case 'f':
      cfi = atoi(argv[optind]);
      break;
    case 'n':
      cell.nof_prb = atoi(argv[optind]);
      break;
    case 'p':
      cell.nof_ports = atoi(argv[optind]);
      break;
    case 'o':
      dci_format = srslte_dci_format_from_string(argv[optind]);
      if (dci_format == SRSLTE_DCI_FORMAT_ERROR) {
        fprintf(stderr, "Error unsupported format %s\n", argv[optind]);
        exit(-1);
      }
      break;
    case 'v':
      srslte_verbose++;
      break;
    case 'e':
      cell.cp = SRSLTE_CP_EXT;
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
  if (!input_file_name) {
    usage(argv[0]);
    exit(-1);
  }
}

int base_init() {
  int i;

  if (srslte_filesource_init(&fsrc, input_file_name, SRSLTE_COMPLEX_FLOAT_BIN)) {
    fprintf(stderr, "Error opening file %s\n", input_file_name);
    exit(-1);
  }

  flen = 2 * (SRSLTE_SLOT_LEN(srslte_symbol_sz(cell.nof_prb)));

  input_buffer = malloc(flen * sizeof(cf_t));
  if (!input_buffer) {
    perror("malloc");
    exit(-1);
  }

  fft_buffer = malloc(SRSLTE_SF_LEN_RE(cell.nof_prb, cell.cp) * sizeof(cf_t));
  if (!fft_buffer) {
    perror("malloc");
    return -1;
  }

  for (i=0;i<SRSLTE_MAX_PORTS;i++) {
    ce[i] = malloc(SRSLTE_SF_LEN_RE(cell.nof_prb, cell.cp) * sizeof(cf_t));
    if (!ce[i]) {
      perror("malloc");
      return -1;
    }
  }

  if (srslte_chest_dl_init(&chest, cell)) {
    fprintf(stderr, "Error initializing equalizer\n");
    return -1;
  }

  if (srslte_ofdm_tx_init(&fft, cell.cp, cell.nof_prb)) {
    fprintf(stderr, "Error initializing FFT\n");
    return -1;
  }

  if (srslte_regs_init(&regs, cell)) {
    fprintf(stderr, "Error initiating regs\n");
    return -1;
  }

  if (srslte_regs_set_cfi(&regs, cfi)) {
    fprintf(stderr, "Error setting CFI %d\n", cfi);
    return -1;
  }

  if (srslte_pdcch_init(&pdcch, &regs, cell)) {
    fprintf(stderr, "Error creating PDCCH object\n");
    exit(-1);
  }

  if (srslte_pdsch_init(&pdsch, cell)) {
    fprintf(stderr, "Error creating PDSCH object\n");
    exit(-1);
  }
  srslte_pdsch_set_rnti(&pdsch, rnti);
  
  if (srslte_harq_init(&harq_process, cell)) {
    fprintf(stderr, "Error initiating HARQ process\n");
    exit(-1);
  }

  DEBUG("Memory init OK\n",0);
  return 0;
}

void base_free() {
  int i;

  srslte_filesource_free(&fsrc);

  free(input_buffer);
  free(fft_buffer);

  srslte_filesource_free(&fsrc);
  for (i=0;i<SRSLTE_MAX_PORTS;i++) {
    free(ce[i]);
  }
  srslte_chest_dl_free(&chest);
  srslte_ofdm_tx_free(&fft);

  srslte_pdcch_free(&pdcch);
  srslte_pdsch_free(&pdsch);
  srslte_harq_free(&harq_process);
  srslte_regs_free(&regs);
}

int main(int argc, char **argv) {
  srslte_ra_pdsch_t ra_dl;  
  int i;
  int nof_frames;
  int ret;
  uint8_t *data;
  srslte_dci_location_t locations[MAX_CANDIDATES];
  uint32_t nof_locations = 0;
  srslte_dci_msg_t dci_msg; 
  
  data = malloc(100000);

  if (argc < 3) {
    usage(argv[0]);
    exit(-1);
  }

  parse_args(argc,argv);

  if (base_init()) {
    fprintf(stderr, "Error initializing memory\n");
    exit(-1);
  }

  if (rnti == SRSLTE_SIRNTI) {
    INFO("Initializing common search space for SI-RNTI\n",0);
    nof_locations = srslte_pdcch_common_locations(&pdcch, locations, MAX_CANDIDATES, cfi);
  } 
  
  ret = -1;
  nof_frames = 0;
  do {
    srslte_filesource_read(&fsrc, input_buffer, flen);
    INFO("Reading %d samples sub-frame %d\n", flen, sf_idx);

    srslte_ofdm_tx_sf(&fft, input_buffer, fft_buffer);

    /* Get channel estimates for each port */
    srslte_chest_dl_estimate(&chest, fft_buffer, ce, sf_idx);
    
    if (rnti != SRSLTE_SIRNTI) {
      INFO("Initializing user-specific search space for RNTI: 0x%x\n", rnti);
      nof_locations = srslte_pdcch_ue_locations(&pdcch, locations, MAX_CANDIDATES, sf_idx, cfi, rnti); 
    }
    
    uint16_t srslte_crc_rem = 0;
    if (srslte_pdcch_extract_llr(&pdcch, fft_buffer, ce, srslte_chest_dl_get_noise_estimate(&chest), sf_idx, cfi)) {
      fprintf(stderr, "Error extracting LLRs\n");
      return -1;
    }
    for (i=0;i<nof_locations && srslte_crc_rem != rnti;i++) {
      if (srslte_pdcch_decode_msg(&pdcch, &dci_msg, &locations[i], dci_format, &srslte_crc_rem)) {
        fprintf(stderr, "Error decoding DCI msg\n");
        return -1;
      }
    }
    
    if (srslte_crc_rem == rnti) {
      if (srslte_dci_msg_to_ra_dl(&dci_msg, rnti, cell, cfi, &ra_dl)) {
        fprintf(stderr, "Error unpacking PDSCH scheduling DCI message\n");
        goto goout;
      }
      if (ra_dl.mcs.tbs > 0) {
        if (srslte_harq_setup_dl(&harq_process, ra_dl.mcs, ra_dl.rv_idx, sf_idx, &ra_dl.prb_alloc)) {
          fprintf(stderr, "Error configuring HARQ process\n");
          goto goout;
        }
        if (srslte_pdsch_decode(&pdsch, &harq_process, fft_buffer, ce, srslte_chest_dl_get_noise_estimate(&chest), data)) {
          fprintf(stderr, "Error decoding PDSCH\n");
          goto goout;
        } else {
          printf("PDSCH Decoded OK!\n");
        }
      } else {
        printf("Received DCI with no resource allocation\n");
      }
      sf_idx = (sf_idx+1)%10;
    }

    nof_frames++;
  } while (nof_frames <= max_frames);

  ret = 0;

goout:
  base_free();
  exit(ret);
}
