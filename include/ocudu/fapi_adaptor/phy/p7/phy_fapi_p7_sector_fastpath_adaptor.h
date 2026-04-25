// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/fapi_adaptor/phy/p7/phy_fapi_p7_sector_adaptor.h"

namespace ocudu {

class upper_phy_error_notifier;
class upper_phy_timing_notifier;
class upper_phy_rx_results_notifier;

namespace fapi_adaptor {

/// \brief PHY-FAPI bidirectional P7 sector fastpath adaptor interface.
///
/// This adaptor is a collection of interfaces to translate FAPI messages into their PHY layer counterpart and vice
/// versa.
class phy_fapi_p7_sector_fastpath_adaptor : public phy_fapi_p7_sector_adaptor
{
public:
  /// Returns a reference to the error notifier used by the adaptor.
  virtual upper_phy_error_notifier& get_error_notifier() = 0;

  /// Returns a reference to the timing notifier used by the adaptor.
  virtual upper_phy_timing_notifier& get_timing_notifier() = 0;

  /// Returns a reference to the results notifier used by the adaptor.
  virtual upper_phy_rx_results_notifier& get_rx_results_notifier() = 0;

  /// \brief Toggle slot-indication delivery to the MAC for this sector.
  ///
  /// Inactive (default at construction) suppresses slot indications so MAC stops scheduling and
  /// the cell goes off-air at the symbol level. Set true when MAC sends FAPI START — the next
  /// slot indication then acks the START transaction and the cell is on-air. Set false on FAPI
  /// STOP. The factory wires this to upper_phy_operation_controller so the FAPI lifecycle drives
  /// it transparently.
  virtual void set_active(bool active) = 0;
};

} // namespace fapi_adaptor
} // namespace ocudu
