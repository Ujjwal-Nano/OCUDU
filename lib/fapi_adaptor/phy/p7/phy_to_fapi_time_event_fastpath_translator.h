// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/fapi/p7/p7_slot_indication_notifier.h"
#include "ocudu/phy/upper/upper_phy_timing_handler.h"
#include "ocudu/phy/upper/upper_phy_timing_notifier.h"
#include <atomic>

namespace ocudu {
namespace fapi_adaptor {

class fapi_to_phy_fastpath_translator;

/// \brief PHY-to-FAPI time event fastpath translator.
///
/// This class listens to upper PHY timing events and translates them into FAPI indication messages that are sent
/// through the FAPI time-specific message notifier.
class phy_to_fapi_time_event_fastpath_translator : public upper_phy_timing_notifier
{
public:
  /// \brief Constructor for the PHY-to-FAPI time event fastpath translator.
  ///
  /// \param[in] translator_ FAPI-to-PHY translator.
  explicit phy_to_fapi_time_event_fastpath_translator(fapi_to_phy_fastpath_translator& translator_);

  // See interface for documentation.
  void on_tti_boundary(const upper_phy_timing_context& context) override;

  /// \brief Configures the FAPI P7 slot indication notifier to the given one.
  void set_p7_slot_indication_notifier(fapi::p7_slot_indication_notifier& notifier)
  {
    slot_indication_notifier = &notifier;
  }

  /// \brief Toggle delivery of slot indications to the MAC.
  ///
  /// Default is active=true so the cell's first activation at DU init does not require the FAPI
  /// START handshake to flip a gate that's currently closed — see mac_cell_processor::start() and
  /// the init-bypass note in cns-ocudu-changes.md (Change 7) for why a default of false would
  /// deadlock during DU.start() in the monolithic deployment. Runtime cell lock/unlock toggles
  /// this normally via the controller chain.
  void set_active(bool a) { active.store(a, std::memory_order_relaxed); }

private:
  /// FAPI to PHY data translator.
  fapi_to_phy_fastpath_translator& translator;
  /// FAPI slot-based, slot indication notifier.
  fapi::p7_slot_indication_notifier* slot_indication_notifier;
  /// Gates slot indication delivery. Written from control executor via set_active(); read on the
  /// timing executor in on_tti_boundary().
  std::atomic<bool> active{true};
};

} // namespace fapi_adaptor
} // namespace ocudu
