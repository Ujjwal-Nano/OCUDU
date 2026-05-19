// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_meas_config_manager.h"
#include "du_ue_resource_config.h"
#include "ocudu/asn1/rrc_nr/dl_dcch_msg_ies.h"
#include "ocudu/ocudulog/ocudulog.h"

using namespace ocudu;
using namespace odu;
using namespace asn1::rrc_nr;

static bool unpack_meas_cfg(meas_cfg_s& meas_cfg, const byte_buffer& container)
{
  asn1::cbit_ref bref{container};
  return meas_cfg.unpack(bref) == asn1::OCUDUASN_SUCCESS;
}

/// A periodic UL occasion defined by its repetition period and slot offset.
struct periodic_ul_occasion {
  unsigned period_slots;
  unsigned offset_slots;
};

// Returns true if the gap with the given offset_ms overlaps any UL occasion within one MGRP period.
static bool gap_conflicts_with_ul_occasions(unsigned                         gap_offset_ms,
                                            unsigned                         mgrp_ms,
                                            unsigned                         slot_per_sf,
                                            unsigned                         length_slots,
                                            span<const periodic_ul_occasion> occasions)
{
  const unsigned period_slots   = mgrp_ms * slot_per_sf;
  const unsigned gap_start_slot = (gap_offset_ms * slot_per_sf) % period_slots;
  for (const auto& occ : occasions) {
    // Iterate distinct UL slot positions within one MGRP period (at most period_slots iterations).
    unsigned       ul_slot = occ.offset_slots % period_slots;
    const unsigned start   = ul_slot;
    unsigned       iters   = 0;
    do {
      const unsigned slot_mod = (ul_slot + period_slots - gap_start_slot) % period_slots;
      if (slot_mod <= length_slots) {
        return true;
      }
      ul_slot = (ul_slot + occ.period_slots) % period_slots;
      ++iters;
    } while (ul_slot != start and iters < period_slots);
  }
  return false;
}

// Finds the smallest valid MGRP >= min_mgrp and a gap offset that covers the SSB at ssb_offset
// without conflicting with any periodic UL occasion (SR, periodic PUCCH CSI) in pcell_serv_cfg.
// Returns {gap_offset, mgrp} on success, or {0ms, 0ms} if no solution exists across all valid MGRPs.
static std::pair<std::chrono::milliseconds, std::chrono::milliseconds>
find_gap_offset(std::chrono::milliseconds  min_mgrp,
                meas_gap_length            mgl,
                std::chrono::milliseconds  ssb_offset,
                unsigned                   slot_per_sf,
                const serving_cell_config& pcell_serv_cfg,
                ocudulog::basic_logger&    logger)
{
  using ms = std::chrono::milliseconds;

  // Collect periodic UL occasions from the UE's serving cell config.
  static_vector<periodic_ul_occasion, MAX_NOF_SR_RESOURCES + MAX_NOF_CSI_REPORT_CONFIGS> ul_occasions;
  if (pcell_serv_cfg.ul_config.has_value() and pcell_serv_cfg.ul_config->init_ul_bwp.pucch_cfg.has_value()) {
    for (const auto& sr : pcell_serv_cfg.ul_config->init_ul_bwp.pucch_cfg->sr_res_list) {
      ul_occasions.push_back({sr_periodicity_to_slot(sr.period), sr.offset});
    }
  }
  if (pcell_serv_cfg.csi_meas_cfg.has_value()) {
    for (const auto& csi_cfg : pcell_serv_cfg.csi_meas_cfg->csi_report_cfg_list) {
      if (std::holds_alternative<csi_report_config::periodic_or_semi_persistent_report_on_pucch>(
              csi_cfg.report_cfg_type)) {
        const auto& periodic =
            std::get<csi_report_config::periodic_or_semi_persistent_report_on_pucch>(csi_cfg.report_cfg_type);
        ul_occasions.push_back(
            {csi_report_periodicity_to_uint(periodic.report_slot_period), periodic.report_slot_offset});
      }
    }
  }

  const auto length_slots = static_cast<unsigned>(std::ceil(meas_gap_length_to_msec(mgl) * slot_per_sf));
  // delta=0: SSB at gap start. delta=max_delta: SSB near gap end.
  const auto max_delta = static_cast<unsigned>(meas_gap_length_to_msec(mgl));

  // Try each valid MGRP from min_mgrp upward until a conflict-free offset is found.
  static constexpr std::array<unsigned, 4> valid_mgrp_ms_values = {20, 40, 80, 160};
  for (unsigned mgrp_ms : valid_mgrp_ms_values) {
    if (ms{(long)mgrp_ms} < min_mgrp) {
      continue;
    }
    const unsigned ssb_sf_ms = (unsigned)ssb_offset.count() % mgrp_ms;
    for (unsigned delta = 0; delta <= max_delta; ++delta) {
      const unsigned candidate_ms = (ssb_sf_ms + mgrp_ms - delta) % mgrp_ms;
      if (!gap_conflicts_with_ul_occasions(candidate_ms, mgrp_ms, slot_per_sf, length_slots, ul_occasions)) {
        return {ms{(long)candidate_ms}, ms{(long)mgrp_ms}};
      }
    }
  }

  logger.warning("measGap: no conflict-free gap offset found across all MGRP values; measGap not set");
  return {ms{0}, ms{0}};
}

// Returns the smallest valid MGRP that is >= period.
static std::chrono::milliseconds min_mgrp_covering(std::chrono::milliseconds period)
{
  if (period.count() <= 20) {
    return std::chrono::milliseconds{20};
  }
  if (period.count() <= 40) {
    return std::chrono::milliseconds{40};
  }
  if (period.count() <= 80) {
    return std::chrono::milliseconds{80};
  }
  return std::chrono::milliseconds{160};
}

/// \brief Convert SMTC period and offset to std::chrono::milliseconds.
static std::pair<std::chrono::milliseconds, std::chrono::milliseconds>
get_smtc_period_offset(const ssb_mtc_s::periodicity_and_offset_c_& smtc)
{
  using ms = std::chrono::milliseconds;
  switch (smtc.type().value) {
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf5:
      return {ms{5}, ms{smtc.sf5()}};
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf10:
      return {ms{10}, ms{smtc.sf10()}};
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf20:
      return {ms{20}, ms{smtc.sf20()}};
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf40:
      return {ms{40}, ms{smtc.sf40()}};
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf80:
      return {ms{80}, ms{smtc.sf80()}};
    case ssb_mtc_s::periodicity_and_offset_c_::types_opts::sf160:
      return {ms{160}, ms{smtc.sf160()}};
    default:
      break;
  }
  return {ms{0}, ms{0}};
}

// Creates a measurement gap based on a SSB MTC config.
// Selects MGRP >= all UL periodic resource periods, then finds an offset avoiding SR/CSI collisions.
static meas_gap_config create_meas_gap(subcarrier_spacing         scs,
                                       const ssb_mtc_s&           smtc1,
                                       const du_cell_config&      cell_cfg,
                                       const serving_cell_config& pcell_serv_cfg,
                                       ocudulog::basic_logger&    logger)
{
  using ms = std::chrono::milliseconds;

  meas_gap_config meas_gap;
  const unsigned  slot_per_sf = get_nof_slots_per_subframe(scs);

  // Determine measGap Length.
  switch (smtc1.dur.value) {
    case ssb_mtc_s::dur_opts::sf1:
      meas_gap.mgl = scs != subcarrier_spacing::kHz15 ? meas_gap_length::ms1dot5 : meas_gap_length::ms3;
      break;
    case ssb_mtc_s::dur_opts::sf2:
      meas_gap.mgl = meas_gap_length::ms3;
      break;
    case ssb_mtc_s::dur_opts::sf3:
      meas_gap.mgl = meas_gap_length::ms4;
      break;
    case ssb_mtc_s::dur_opts::sf4:
    case ssb_mtc_s::dur_opts::sf5:
      meas_gap.mgl = meas_gap_length::ms6;
      break;
    default:
      report_fatal_error("Invalid SSB MTC duration");
  }

  // Extract SSB offset and period.
  const auto [ssb_period, ssb_offset] = get_smtc_period_offset(smtc1.periodicity_and_offset);

  // Determine minimum MGRP.
  // MGRP should not be lower than the SSB period and than the PUCCH SR/CSI report periods to avoid collisions.
  const ms sr_period{sr_periodicity_to_slot(cell_cfg.ran.init_bwp.pucch.sr_period) / slot_per_sf};
  const ms csi_period{cell_cfg.ran.init_bwp.csi.has_value()
                          ? (csi_resource_periodicity_to_uint(cell_cfg.ran.init_bwp.csi->csi_rs_period) / slot_per_sf)
                          : 0};
  ms min_mgrp = std::max({min_mgrp_covering(ssb_period), min_mgrp_covering(sr_period), min_mgrp_covering(csi_period)});

  // Find a gap offset and period that covers the SSB while avoiding conflicts with SR and CSI.
  auto [mg_offset, mgrp] = find_gap_offset(min_mgrp, meas_gap.mgl, ssb_offset, slot_per_sf, pcell_serv_cfg, logger);
  if (mgrp != ms{0}) {
    // Solution found.
    meas_gap.offset = mg_offset.count();
    meas_gap.mgrp   = static_cast<meas_gap_repetition_period>(mgrp.count());
  }

  return meas_gap;
}

du_meas_config_manager::du_meas_config_manager(span<const du_cell_config> cell_cfg_list_) :
  cell_cfg_list(cell_cfg_list_), logger(ocudulog::fetch_basic_logger("DU-MNG"))
{
}

void du_meas_config_manager::update(du_ue_resource_config& ue_cfg, const byte_buffer& packed_meas_cfg)
{
  if (packed_meas_cfg.empty()) {
    return;
  }

  meas_cfg_s meas_cfg;
  if (not unpack_meas_cfg(meas_cfg, packed_meas_cfg)) {
    logger.error("Failed to unpack meas config. Discarding it...");
    return;
  }

  const du_cell_config& pcell_common =
      cell_cfg_list[ue_cfg.cell_group.cells.at(SERVING_PCELL_IDX).serv_cell_cfg.cell_index];

  for (const auto& asn1measobj : meas_cfg.meas_obj_to_add_mod_list) {
    if (asn1measobj.meas_obj.type().value != meas_obj_to_add_mod_s::meas_obj_c_::types_opts::meas_obj_nr) {
      logger.warning("Ignoring measObject of type {}. Cause: Unsupported", asn1measobj.meas_obj.type().to_string());
      continue;
    }
    const auto& asn1nr = asn1measobj.meas_obj.meas_obj_nr();

    if (not asn1nr.ssb_freq_present or not asn1nr.smtc1_present) {
      logger.info("Ignoring measObject of type {}. Cause: Lack of a SSB frequency or SMTC1 config",
                  asn1measobj.meas_obj.type().to_string());
      continue;
    }

    if (asn1nr.ssb_freq == pcell_common.ran.dl_cfg_common.freq_info_dl.absolute_frequency_ssb) {
      // Same frequency. No need for measGap.
      continue;
    }

    // Create measGap, choosing MGRP and offset that avoid SR and CSI conflicts.
    const auto& pcell_serv_cfg = ue_cfg.cell_group.cells.at(SERVING_PCELL_IDX).serv_cell_cfg;
    ue_cfg.meas_gap            = create_meas_gap(pcell_common.ran.dl_cfg_common.init_dl_bwp.generic_params.scs,
                                      asn1nr.smtc1,
                                      pcell_common,
                                      pcell_serv_cfg,
                                      logger);
  }
}
