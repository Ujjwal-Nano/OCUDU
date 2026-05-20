// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/mac/cell_configuration.h"
#include "ocudu/ran/dmrs/dmrs.h"
#include "ocudu/ran/pci.h"
#include "ocudu/ran/ssb/ssb_configuration.h"
#include <atomic>
#include <cstddef>

namespace ocudu {

struct dl_ssb_pdu;
struct ssb_information;

/// This class implements a helper to compute some SSB-specific parameters from the MAC's Cell configuration. These
/// parameters are passed to the scheduler and then used to assemble the SSB message to be sent to the PHY.
class ssb_assembler
{
public:
  explicit ssb_assembler(const mac_cell_creation_request& cell_cfg);

  /// \brief Assemble SSB message to be sent to PHY.
  /// This function fills the SSB msg to send to PHY using parameters from: (i) MAC configuration (general and SSB);(ii)
  /// SSB-specific dependent parameters; (iii) SSB scheduling results.
  /// \param[out] ssb_pdu SSB message to be sent to PHY.
  /// \param[in]  ssb_info SSB scheduling results.
  void assemble_ssb(dl_ssb_pdu& ssb_pdu, const ssb_information& ssb_info);

  /// Update the MIB cellBarred flag at runtime. The new value takes effect on the next SSB assembly.
  void set_cell_barred(bool value) { cell_barred.store(value, std::memory_order_relaxed); }

  /// Update the MIB intraFreqReselection flag at runtime. The new value takes effect on the next SSB assembly.
  void set_intra_freq_reselection(bool value) { intra_freq_reselection.store(value, std::memory_order_relaxed); }

private:
  /// Cell PCI.
  pci_t pci;
  /// SSB configuration for the cell.
  const ssb_configuration ssb_cfg;
  uint8_t                 pdcch_config_sib1;
  dmrs_typeA_position     dmrs_typeA_pos;
  /// MIB cellBarred. Read on cell executor in assemble_ssb(); written on control executor via
  /// set_cell_barred().
  std::atomic<bool> cell_barred;
  /// MIB intraFreqReselection. Read on cell executor in assemble_ssb(); written on control executor
  /// via set_intra_freq_reselection().
  std::atomic<bool> intra_freq_reselection;

  /// Other derived SSB parameters.
  ssb_pattern_case ssb_case;
  uint8_t          L_max;
};

} // namespace ocudu
