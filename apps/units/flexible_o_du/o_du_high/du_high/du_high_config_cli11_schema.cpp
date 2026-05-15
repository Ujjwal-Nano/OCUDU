// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_high_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "du_high_config.h"
#include "ntn/du_high_ntn_config_cli11_schema.h"
#include "ocudu/adt/ranges/transform.h"
#include "ocudu/ran/csi_report/csi_report_configuration.h"
#include "ocudu/ran/drx_config.h"
#include "ocudu/ran/du_types.h"
#include "ocudu/ran/duplex_mode.h"
#include "ocudu/ran/pucch/pucch_mapping.h"
#include "ocudu/ran/slot_point_extended.h"
#include "ocudu/scheduler/config/scheduler_expert_config.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"
#include "ocudu/support/format/fmt_to_c_str.h"
#include "CLI/CLI11.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
#include <vector>

using namespace ocudu;

namespace {

template <typename Integer>
expected<Integer, std::string> parse_int(const std::string& value)
{
  try {
    return std::stoi(value);
  } catch (const std::invalid_argument& e) {
    return make_unexpected(e.what());
  } catch (const std::out_of_range& e) {
    return make_unexpected(e.what());
  }
}

expected<uint32_t, std::string> parse_32bit_mask_input(std::string_view value)
{
  if (value.empty()) {
    return make_unexpected("Input is empty");
  }

  const char* first               = value.data();
  const char* last                = value.data() + value.size();
  unsigned    base                = 10;
  unsigned    expected_nof_digits = 0;

  // Detect prefix and adjust first pointer and base.
  if (value.size() >= 2 && value[0] == '0') {
    char format = std::tolower(value[1]);
    if (format == 'x') {
      base = 16;
      first += 2; // Skip 0x.
      expected_nof_digits = 8;
    } else if (format == 'b') {
      base = 2;
      first += 2; // Skip 0b.
      expected_nof_digits = 32;
    }
  }

  if ((last - first) != expected_nof_digits) {
    return make_unexpected(base == 16 ? "hex string must be exactly 8 characters (32 bits)"
                                      : "binary string must be exactly 32 characters (32 bits)");
  }

  uint32_t result;
  auto [ptr, ec] = std::from_chars(first, last, result, base);

  if (ec == std::errc::invalid_argument) {
    return make_unexpected("Not a number");
  }
  if (ec == std::errc::result_out_of_range) {
    return make_unexpected("Number too large for type");
  }
  if (ptr != last) {
    return make_unexpected("Invalid characters in string");
  }
  return result;
}

/// Format a std::array<uint8_t, N> as a comma-separated string within square brackets, matching the legacy
/// default_str for the ss1/ss2 candidate arrays.
template <std::size_t N>
std::string format_uint8_array(const std::array<uint8_t, N>& values)
{
  std::string out = "[";
  for (std::size_t i = 0; i < N; ++i) {
    if (i != 0) {
      out += ',';
    }
    out += std::to_string(values[i]);
  }
  out += ']';
  return out;
}

/// Parse a string of the form "[v0,v1,v2,v3,v4]" / "v0 v1 v2 v3 v4" / "v0,v1,v2,v3,v4" into a std::array<uint8_t, N>.
/// Whitespace, brackets and commas are treated as separators. Missing entries leave the array's default in place.
template <std::size_t N>
void parse_uint8_array(const std::string& in, std::array<uint8_t, N>& out)
{
  std::string buf = in;
  // Replace brackets/commas with spaces so std::istringstream tokenises uniformly.
  for (auto& c : buf) {
    if (c == '[' || c == ']' || c == ',') {
      c = ' ';
    }
  }
  std::istringstream ss(buf);
  for (std::size_t i = 0; i < N; ++i) {
    unsigned v = 0;
    if (!(ss >> v)) {
      return;
    }
    out[i] = static_cast<uint8_t>(v);
  }
}

/// Builds a setter/getter pair for a std::array<uint8_t, N> field bound through string_action.
template <std::size_t N>
auto make_uint8_array_actions(std::array<uint8_t, N>& target)
{
  return std::make_pair(std::function<void(const std::string&)>(
                            [&target](const std::string& value) { parse_uint8_array(value, target); }),
                        std::function<std::string()>([&target]() { return format_uint8_array(target); }));
}

/// Builds a setter/getter pair for the 32-bit-mask "false"/"true"/"0xXX"/"0bXX" idiom into a bitset.
template <typename Bitset>
auto make_32bit_mask_actions(Bitset& target)
{
  auto setter = [&target](const std::string& value) {
    uint32_t mask;
    if (value == "false") {
      mask = 0x00000000;
    } else if (value == "true") {
      // Keep four leftmost slots disabled (matches legacy behaviour).
      mask = 0x0fffffff;
    } else {
      mask = parse_32bit_mask_input(value).value();
    }
    for (unsigned i = 0; i != MAX_NOF_HARQS; ++i) {
      target.set(i, (mask >> (31 - i)) & 1);
    }
  };
  auto getter = [&target]() -> std::string {
    uint32_t mask = 0;
    for (unsigned i = 0; i != MAX_NOF_HARQS; ++i) {
      if (target.test(i)) {
        mask |= (1U << (31 - i));
      }
    }
    if (mask == 0) {
      return "false";
    }
    if (mask == 0x0fffffff) {
      return "true";
    }
    return fmt::format("0x{:08x}", mask);
  };
  return std::make_pair(std::function<void(const std::string&)>(setter), std::function<std::string()>(getter));
}

/// Builds a setter/getter pair for a dc_offset_t value that accepts either the sentinel strings
/// "outside"/"undetermined"/"center" or an integer encoded as the underlying enum value.
auto make_dc_offset_actions(dc_offset_t& target)
{
  auto setter = [&target](const std::string& value) {
    if (value == "undetermined") {
      target = dc_offset_t::undetermined;
    } else if (value == "outside") {
      target = dc_offset_t::outside;
    } else if (value == "center") {
      target = dc_offset_t::center;
    } else {
      auto v = parse_int<int>(value);
      if (v.has_value()) {
        target = static_cast<dc_offset_t>(v.value());
      }
    }
  };
  auto getter = [&target]() -> std::string {
    if (target == dc_offset_t::undetermined) {
      return "undetermined";
    }
    if (target == dc_offset_t::outside) {
      return "outside";
    }
    if (target == dc_offset_t::center) {
      return "center";
    }
    return std::to_string(static_cast<int>(target));
  };
  return std::make_pair(std::function<void(const std::string&)>(setter), std::function<std::string()>(getter));
}

/// Side buffer used by the --srbs option to bridge a vector-typed array_of into the std::map-typed target. Cleared
/// on each (re-)registration of the schema and folded back into the map by autoderive_du_high_parameters_after_parsing.
inline std::vector<du_high_unit_srb_config>& get_srb_buffer()
{
  static std::vector<du_high_unit_srb_config> buf;
  return buf;
}

} // namespace

// ===========================================================================
// Section configurators (builder API).
// ===========================================================================

static void declare_log_args(config::config_builder& b, du_high_unit_logger_config& log_params)
{
  app_helpers::add_log_option(b, log_params.mac_level, "--mac_level", "MAC log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.rlc_level, "--rlc_level", "RLC log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.f1ap_level, "--f1ap_level", "F1AP log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.f1u_level, "--f1u_level", "F1-U log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.gtpu_level, "--gtpu_level", "GTPU log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.ntn_level, "--ntn_level", "NTN log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.du_level, "--du_level", "Log level for the DU").fallback_from("--all_level");

  b.option("--hex_max_size",
           log_params.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);
  b.option("--broadcast_enabled",
           log_params.broadcast_enabled,
           "Enable logging in the physical and MAC layer of broadcast messages and all PRACH opportunities");
  b.option("--f1ap_json_enabled", log_params.f1ap_json_enabled, "Enable JSON logging of F1AP PDUs");
  b.option("--high_latency_diagnostics_enabled",
           log_params.high_latency_diagnostics_enabled,
           "Log performance diagnostics when high computational latencies are detected");
}

static void declare_trace_args(config::config_builder& b, du_high_unit_tracer_config& config)
{
  b.group("layers", "Layer basis tracing configuration", [&](config::config_builder& l) {
    l.option("--du_high_enable", config.executor_tracing_enable, "Enable tracing for DU-high executors");
  });
}

static void declare_expert_execution_args(config::config_builder& b, du_high_unit_expert_execution_config& config)
{
  b.group("queues", "Task executor queue parameters", [&](config::config_builder& q) {
    q.option("--du_ue_data_executor_queue_size",
             config.du_queue_cfg.ue_data_executor_queue_size,
             "DU's UE executor task queue size for PDU processing");
  });
}

static void declare_pdcch_common_args(config::config_builder& b, pdcch_common_unit_config& common_params)
{
  b.option("--coreset0_index", common_params.coreset0_index, "CORESET#0 index").range(0, 15);

  // std::array<uint8_t, 5> — bound via string_action because the builder taxonomy doesn't yet expose
  // std::array scalars. The legacy CLI11 binding accepted a list of integers in {0,1,2,3,4,5,6,8}.
  auto [ss1_setter, ss1_getter] = make_uint8_array_actions(common_params.ss1_n_candidates);
  b.string_action(
       "--ss1_n_candidates",
       std::move(ss1_setter),
       std::move(ss1_getter),
       "Number of PDCCH candidates per aggregation level for SearchSpace#1. Default: {0, 0, 1, 0, 0}",
       "5-element list of uint8_t (e.g. \"[0,0,1,0,0]\"). Each element must be in {0,1,2,3,4,5,6,8}.")
      .note("legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string.");

  b.option("--ss0_index", common_params.ss0_index, "SearchSpace#0 index").range(0, 15);

  // NOTE: The CORESET duration of 3 symbols is only permitted if the dmrs-typeA-Position information element has
  // been set to 3. And, we use only pos2 or pos1.
  b.option("--max_coreset0_duration",
           common_params.max_coreset0_duration,
           "Maximum CORESET#0 duration in OFDM symbols to consider when deriving CORESET#0 index")
      .range(1, 2);
}

static void declare_pdcch_dedicated_args(config::config_builder& b, pdcch_dedicated_unit_config& ded_params)
{
  b.option("--coreset1_rb_start",
           ded_params.coreset1_rb_start,
           "Starting Common Resource Block (CRB) number for CORESET 1 relative to CRB 0. Default: CRB0")
      .range(0, 275);
  b.option("--coreset1_l_crb",
           ded_params.coreset1_l_crb,
           "Length of CORESET 1 in number of CRBs. Default: Across entire BW of cell")
      .range(0, 275);

  // NOTE: The CORESET duration of 3 symbols is only permitted if the dmrs-typeA-Position information element has been
  // set to 3. And, we use only pos2 or pos1.
  b.option("--coreset1_duration",
           ded_params.coreset1_duration,
           "Duration of CORESET 1 in number of OFDM symbols. Default: Max(2, Nof. CORESET#0 symbols)")
      .range(1, 2);

  auto [ss2_setter, ss2_getter] = make_uint8_array_actions(ded_params.ss2_n_candidates);
  b.string_action(
       "--ss2_n_candidates",
       std::move(ss2_setter),
       std::move(ss2_getter),
       "Number of PDCCH candidates per aggregation level for SearchSpace#2. Default: {0, 0, 0, 0, 0} i.e. "
       "auto-compute nof. candidates",
       "5-element list of uint8_t (e.g. \"[0,0,0,0,0]\"). Each element must be in {0,1,2,3,4,5,6,8}.")
      .note("legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string.");

  b.option("--dci_format_0_1_and_1_1", ded_params.dci_format_0_1_and_1_1, "DCI format to use in UE dedicated SearchSpace#2");
  b.enum_option("--ss2_type",
                ded_params.ss2_type,
                "SearchSpace type for UE dedicated SearchSpace#2",
                {{"common", search_space_configuration::type_t::common},
                 {"ue_dedicated", search_space_configuration::type_t::ue_dedicated}});

  b.option("--al_cqi_offset",
           ded_params.al_cqi_offset,
           "Offset to apply to the CQI value used in the PDCCH aggregation level calculation.")
      .range(-15, 15);
}

static void declare_pdcch_args(config::config_builder& b, du_high_unit_pdcch_config& pdcch_params)
{
  b.group("common", "PDCCH Common configuration parameters",
          [&](config::config_builder& sub) { declare_pdcch_common_args(sub, pdcch_params.common); });
  b.group("dedicated", "PDCCH Dedicated configuration parameters",
          [&](config::config_builder& sub) { declare_pdcch_dedicated_args(sub, pdcch_params.dedicated); });
}

static void declare_pdsch_args(config::config_builder& b, du_high_unit_pdsch_config& pdsch_params)
{
  b.option("--min_ue_mcs", pdsch_params.min_ue_mcs, "Minimum UE MCS").range(0, 28);
  b.option("--max_ue_mcs", pdsch_params.max_ue_mcs, "Maximum UE MCS").range(0, 28);
  b.option("--fixed_rar_mcs", pdsch_params.fixed_rar_mcs, "Fixed RAR MCS").range(0, 28);
  b.option("--fixed_sib1_mcs", pdsch_params.fixed_sib1_mcs, "Fixed SIB1 MCS").range(0, 28);

  // 32-bit bitmask idiom; string_action is the escape hatch.
  {
    auto [setter, getter] = make_32bit_mask_actions(pdsch_params.harq_feedback_disabled);
    b.string_action("--harq_feedback_disabled",
                    std::move(setter),
                    std::move(getter),
                    "Disable DL HARQ Feedback (only for NTN cells).\n"
                    "If set to true, applies the mask 0x0fffffff and disables HARQ feedback for all except the "
                    "first four HARQs.\n"
                    "If set to a string, it must be a 32-bit bitmap (0x… or 0b…) of the HARQ processes to disable.\n"
                    "The bit set to 1 indicates HARQ processes with disabled DL HARQ feedback and the bit set to 0,\n"
                    "identify HARQ processes with enabled DL HARQ feedback."
                    "The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are "
                    "ignored.\n",
                    "accepts \"true\", \"false\" or a 32-bit bitmap (\"0x...\" / \"0b...\").");
  }

  b.option("--nof_harqs",
           pdsch_params.nof_harqs,
           "Number of DL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; "
           "otherwise, it defaults to 16.")
      .note("legal values: {2, 4, 6, 8, 10, 12, 16, 32}");
  b.option("--max_nof_harq_retxs",
           pdsch_params.max_nof_harq_retxs,
           "Maximum number of times a DL HARQ can be retransmitted, before it gets discarded.")
      .range(0, 64);
  b.option("--harq_retx_timeout",
           pdsch_params.harq_retx_timeout,
           "Maximum time, in milliseconds, between a HARQ NACK and the scheduler allocating the respective "
           "HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded.")
      .range(10, 500);
  b.option("--max_consecutive_kos",
           pdsch_params.max_consecutive_kos,
           "Maximum number of HARQ-ACK consecutive KOs before an Radio Link Failure is reported");
  b.option("--rv_sequence", pdsch_params.rv_sequence, "RV sequence for PUSCH. (e.g. [0 2 3 1]")
      .note("each element must be in {0, 1, 2, 3}");
  b.enum_option("--mcs_table",
                pdsch_params.mcs_table,
                "MCS table to use PDSCH",
                {{"qam64", pdsch_mcs_table::qam64},
                 {"qam256", pdsch_mcs_table::qam256},
                 {"qam64lowse", pdsch_mcs_table::qam64LowSe}});
  b.option("--min_rb_size", pdsch_params.min_rb_size, "Minimum RB size for UE PDSCH resource allocation")
      .range(1, static_cast<int>(MAX_NOF_PRBS));
  b.option("--max_rb_size", pdsch_params.max_rb_size, "Maximum RB size for UE PDSCH resource allocation")
      .range(1, static_cast<int>(MAX_NOF_PRBS));
  b.option("--start_rb", pdsch_params.start_rb, "Start RB for resource allocation of UE PDSCHs")
      .range(0, static_cast<int>(MAX_NOF_PRBS));
  b.option("--end_rb", pdsch_params.end_rb, "End RB for resource allocation of UE PDSCHs")
      .range(0, static_cast<int>(MAX_NOF_PRBS));
  b.option("--max_pdschs_per_slot",
           pdsch_params.max_pdschs_per_slot,
           "Maximum number of PDSCH grants per slot, including SIB, RAR, Paging and UE data grants.")
      .range(1, static_cast<int>(MAX_PDSCH_PDUS_PER_SLOT));
  b.option("--max_alloc_attempts",
           pdsch_params.max_pdcch_alloc_attempts_per_slot,
           "Maximum number of DL or UL PDCCH grant allocation attempts per slot before scheduler skips the slot")
      .range(1, static_cast<int>(std::max(MAX_DL_PDCCH_PDUS_PER_SLOT, MAX_UL_PDCCH_PDUS_PER_SLOT)));
  b.option("--olla_cqi_inc_step",
           pdsch_params.olla_cqi_inc,
           "Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled")
      .range(0.0, 1.0);
  b.option("--olla_target_bler",
           pdsch_params.olla_target_bler,
           "Target DL BLER set in Outer-loop link adaptation (OLLA) algorithm")
      .range(0.0, 0.5);
  b.option("--olla_max_cqi_offset",
           pdsch_params.olla_max_cqi_offset,
           "Maximum offset that the Outer-loop link adaptation (OLLA) can apply to CQI")
      .min_value(0.0);

  {
    auto [setter, getter] = make_dc_offset_actions(pdsch_params.dc_offset);
    b.string_action("--dc_offset",
                    std::move(setter),
                    std::move(getter),
                    "Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for "
                    "carrier spacing, and the center of the gNB DL carrier as DC offset value 0. The user can "
                    "additionally set \"outside\" to define that the DC offset falls outside the DL carrier or "
                    "\"undetermined\" in the case the DC offset is unknown.",
                    "accepts an integer in [min,max] or one of {\"outside\",\"undetermined\",\"center\"}.");
  }

  b.option("--harq_la_cqi_drop_threshold",
           pdsch_params.harq_la_cqi_drop_threshold,
           "Link Adaptation (LA) threshold for drop in CQI of the first HARQ transmission above which HARQ "
           "retransmissions are cancelled. Set this value to 0 to disable this feature")
      .range(0, 15);
  b.option("--harq_la_ri_drop_threshold",
           pdsch_params.harq_la_ri_drop_threshold,
           "Link Adaptation (LA) threshold for drop in nof. layers of the first HARQ transmission above which "
           "HARQ retransmission is cancelled. Set this value to 0 to disable this feature")
      .range(0, 4);
  b.option("--dmrs_additional_position", pdsch_params.dmrs_add_pos, "PDSCH DMRS additional position").range(0, 3);
  b.option("--interleaving_bundle_size",
           pdsch_params.interleaving_bundle_size,
           "PDSCH interleaving bundle size. Valid values: [0, 2, 4]")
      .note("legal values: {0, 2, 4}");
  b.option("--max_rank",
           pdsch_params.max_rank,
           "Maximum number of PDSCH transmission layers. The actual maximum is limited by the number of DL antennas.")
      .min_value(0.0);
  b.option("--enable_csi_rs_pdsch_multiplexing",
           pdsch_params.enable_csi_rs_pdsch_multiplexing,
           "Enable multiplexing of CSI-RS and PDSCH");
}

static void declare_du_args(config::config_builder& b, bool& warn_on_drop)
{
  b.option("--warn_on_drop", warn_on_drop, "Log a warning for dropped packets in F1-U, RLC and MAC due to full queues");
}

static void declare_mac_bsr_args(config::config_builder& b, mac_bsr_unit_config& bsr_params)
{
  b.option("--periodic_bsr_timer",
           bsr_params.periodic_bsr_timer,
           "Periodic Buffer Status Report Timer value in nof. subframes. Value 0 equates to infinity")
      .note("legal values: {1, 5, 10, 16, 20, 32, 40, 64, 80, 128, 160, 320, 640, 1280, 2560, 0}");
  b.option("--retx_bsr_timer",
           bsr_params.retx_bsr_timer,
           "Retransmission Buffer Status Report Timer value in nof. subframes")
      .note("legal values: {10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240}");
  b.option("--lc_sr_delay_timer",
           bsr_params.lc_sr_delay_timer,
           "Logical Channel SR delay timer in nof. subframes")
      .note("legal values: {20, 40, 64, 128, 512, 1024, 2560}");
}

static void declare_mac_phr_args(config::config_builder& b, mac_phr_unit_config& phr_params)
{
  b.option("--phr_prohibit_timer", phr_params.phr_prohib_timer, "PHR prohibit timer in nof. subframes")
      .note("legal values: {0, 10, 20, 50, 100, 200, 500, 1000}");
}

static void declare_mac_sr_args(config::config_builder& b, mac_sr_unit_config& sr_params)
{
  b.option("--sr_trans_max", sr_params.sr_trans_max, "Maximum number of SR transmissions")
      .note("legal values: {4, 8, 16, 32, 64}");
  b.option("--sr_prohibit_timer", sr_params.sr_prohibit_timer, "Timer for SR transmission on PUCCH in ms")
      .note("legal values: {1, 2, 4, 8, 16, 32, 64, 128}");
}

static void declare_mac_cell_group_args(config::config_builder& b, du_high_unit_mac_cell_group_config& mcg_params)
{
  b.group("bsr_cfg", "Buffer status report configuration parameters",
          [&](config::config_builder& sub) { declare_mac_bsr_args(sub, mcg_params.bsr_cfg); });
  b.group("phr_cfg", "Power Headroom report configuration parameters",
          [&](config::config_builder& sub) { declare_mac_phr_args(sub, mcg_params.phr_cfg); });
  b.group("sr_cfg", "Scheduling Request configuration parameters",
          [&](config::config_builder& sub) { declare_mac_sr_args(sub, mcg_params.sr_cfg); });
}

static void declare_ssb_args(config::config_builder& b, du_high_unit_ssb_config& ssb_params)
{
  b.option("--ssb_period", ssb_params.ssb_period_msec, "Period of SSB scheduling in milliseconds")
      .note("legal values: {5, 10, 20}");
  b.option("--ssb_block_power_dbm", ssb_params.ssb_block_power, "SS_PBCH_power_block in dBm").range(-60, 50);

  b.enum_option("--pss_to_sss_epre",
                ssb_params.pss_to_sss_epre,
                "SSB PSS to SSS EPRE ratio in dB {0, 3}",
                {{"0", ocudu::ssb_pss_to_sss_epre::dB_0}, {"3", ocudu::ssb_pss_to_sss_epre::dB_3}})
      .note("legacy CLI11 accepted only the integer values 0 and 3; the builder API maps them to the string keys "
            "\"0\" and \"3\".");
}

static void declare_tdd_ul_dl_pattern_args(config::config_builder& b, tdd_ul_dl_pattern_unit_config& pattern_params)
{
  b.option("--dl_ul_tx_period",
           pattern_params.dl_ul_period_slots,
           "TDD pattern periodicity in slots. The combination of this value and the chosen numerology must lead"
           " to a TDD periodicity of 0.5, 0.625, 1, 1.25, 2, 2.5, 3, 4, 5 or 10 milliseconds.")
      .range(2, 80);
  b.option("--nof_dl_slots", pattern_params.nof_dl_slots, "TDD pattern nof. consecutive full DL slots").range(0, 80);
  b.option("--nof_dl_symbols",
           pattern_params.nof_dl_symbols,
           "TDD pattern nof. DL symbols at the beginning of the slot following full DL slots")
      .range(0, 13);
  b.option("--nof_ul_slots", pattern_params.nof_ul_slots, "TDD pattern nof. consecutive full UL slots").range(0, 80);
  b.option("--nof_ul_symbols",
           pattern_params.nof_ul_symbols,
           "TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot")
      .range(0, 13);
}

static void declare_tdd_ul_dl_args(config::config_builder& b, du_high_unit_tdd_ul_dl_config& tdd_ul_dl_params)
{
  declare_tdd_ul_dl_pattern_args(b, tdd_ul_dl_params.pattern1);

  // pattern2 is held as std::optional. The legacy CLI11 binding emplaced the optional only when the "pattern2"
  // subcommand was used (via parse_complete_callback). The builder API has no parse-complete hook yet, so we
  // emplace the optional eagerly so child options can bind, and rely on the autoderive step or a runtime
  // validator to clear it back when no field was set. (TODO)
  if (!tdd_ul_dl_params.pattern2.has_value()) {
    tdd_ul_dl_params.pattern2.emplace();
  }
  b.group("pattern2", "TDD UL DL pattern2 configuration parameters",
          [&](config::config_builder& sub) { declare_tdd_ul_dl_pattern_args(sub, *tdd_ul_dl_params.pattern2); });
}

static void declare_paging_args(config::config_builder& b, du_high_unit_paging_config& pg_params)
{
  b.option("--pg_search_space_id", pg_params.paging_search_space_id, "SearchSpace to use for Paging")
      .note("legal values: {0, 1}");
  b.option("--default_pg_cycle_in_rf", pg_params.default_paging_cycle, "Default Paging cycle in nof. Radio Frames")
      .note("legal values: {32, 64, 128, 256}");
  b.enum_option("--nof_pf_per_paging_cycle",
                pg_params.nof_pf,
                "Number of paging frames per DRX cycle {oneT, halfT, quarterT, oneEighthT, oneSixteethT}. "
                "Default: oneT",
                {{"oneT", pcch_config::nof_pf_per_drx_cycle::oneT},
                 {"halfT", pcch_config::nof_pf_per_drx_cycle::halfT},
                 {"quarterT", pcch_config::nof_pf_per_drx_cycle::quarterT},
                 {"oneEighthT", pcch_config::nof_pf_per_drx_cycle::oneEighthT},
                 {"oneSixteethT", pcch_config::nof_pf_per_drx_cycle::oneSixteethT}});
  b.option("--pf_offset", pg_params.pf_offset, "Paging frame offset");
  b.option("--nof_po_per_pf", pg_params.nof_po_per_pf, "Number of paging occasions per paging frame")
      .note("legal values: {1, 2, 4}");
  b.option("--edrx_enabled", pg_params.edrx_enabled, "Enable eDRX");
}

static void declare_csi_args(config::config_builder& b, du_high_unit_csi_config& csi_params)
{
  b.option("--csi_rs_enabled", csi_params.csi_rs_enabled, "Enable CSI-RS resources and CSI reporting");
  b.option("--csi_rs_period", csi_params.csi_rs_period_msec, "CSI-RS period in milliseconds")
      .note("legal values: {10, 20, 40, 80}");
  b.enum_option("--report_type",
                csi_params.report_type,
                "Type of CSI reporting configuration to use",
                {{"periodic", csi_report_type::periodic}, {"aperiodic", csi_report_type::aperiodic}});
  b.option("--meas_csi_rs_slot_offset",
           csi_params.meas_csi_slot_offset,
           "Slot offset of first CSI-RS resource used for measurement");
  b.option("--tracking_csi_rs_slot_offset",
           csi_params.tracking_csi_slot_offset,
           "Slot offset of first CSI-RS resource used for tracking");
  b.option("--zp_csi_rs_slot_offset", csi_params.zp_csi_slot_offset, "Slot offset of the ZP CSI-RS resources");
  b.option("--pwr_ctrl_offset",
           csi_params.pwr_ctrl_offset,
           "powerControlOffset, Power offset of PDSCH RE to NZP CSI-RS RE in dB")
      .range(-8, 15);
}

static void declare_qos_aware_policy_args(config::config_builder& b, time_qos_scheduler_config& expert_params)
{
  b.enum_option("--combine_function",
                expert_params.combine_function,
                "QoS-aware scheduler policy weight combining function",
                {{"gbr_prioritized", time_qos_scheduler_config::combine_function_type::gbr_prioritized},
                 {"geometric_mean", time_qos_scheduler_config::combine_function_type::geometric_mean}});
  b.option("--pf_fairness_coeff",
           expert_params.pf_fairness_coeff,
           "Fairness Coefficient to use in Proportional Fair (PF) weight");
  b.option("--prio_enabled",
           expert_params.priority_enabled,
           "Whether to take into account the QoS Flow priority in QoS-aware scheduling");
  b.option("--pdb_enabled",
           expert_params.pdb_enabled,
           "Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling");
  b.option("--gbr_enabled",
           expert_params.gbr_enabled,
           "Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling");
}

static void declare_scheduler_policy_args(config::config_builder& b, std::optional<scheduler_policy_config>& policy_cfg)
{
  // Legacy CLI11 emplaced policy_cfg with the right alternative based on which subcommand was used (via
  // parse_complete_callback). Without that hook the alternative must be (re-)selected here. We default to the QoS
  // alternative so its child options have a stable binding target. (TODO: port the legacy subcommand-presence gate
  // to a runtime validator.)
  if (!policy_cfg.has_value() || !std::holds_alternative<time_qos_scheduler_config>(*policy_cfg)) {
    policy_cfg = time_qos_scheduler_config{};
  }

  b.group("qos_sched", "Time-domain QoS-aware policy configuration", [&](config::config_builder& sub) {
    declare_qos_aware_policy_args(sub, std::get<time_qos_scheduler_config>(*policy_cfg));
  });

  b.group("rr_sched", "Time-domain Round-robin policy configuration", [&](config::config_builder& /*sub*/) {
    // No fields; selecting this policy is gated by subcommand presence in legacy CLI11. (TODO)
  });
}

static void declare_ta_control_args(config::config_builder& b, du_high_unit_ta_sched_control_config& ta_params)
{
  b.option("--ta_measurement_slot_period",
           ta_params.ta_measurement_slot_period,
           "Measurements periodicity in number of slots over which the new Timing Advance Command is computed");
  b.option("--ta_measurement_slot_prohibit_period",
           ta_params.ta_measurement_slot_prohibit_period,
           "Delay in number of slots between issuing the TA_CMD and starting TA measurements.")
      .range(0, 10000);
  b.option("--ta_cmd_offset_threshold",
           ta_params.ta_cmd_offset_threshold,
           "Timing Advance Command (T_A) offset threshold above which Timing Advance Command is triggered. If set to "
           "less than zero, issuing of TA Command is disabled")
      .range(-1, 31);
  b.option("--ta_target", ta_params.ta_target, "Timing Advance target in units of TA").range(-30.0, 30.0);
  b.option("--ta_update_measurement_ul_sinr_threshold",
           ta_params.ta_update_measurement_ul_sinr_threshold,
           "UL SINR threshold (in dB) above which reported N_TA update measurement is considered valid");
  b.option("--ta_outlier_detection_zscore_threshold",
           ta_params.ta_outlier_detection_zscore_threshold,
           "Z-score threshold for outlier detection in N_TA measurements. Controls the sensitivity of the outlier "
           "detection algorithm. A lower value makes the filter more aggressive (rejects more measurements), while "
           "a higher value makes it more permissive. Typical values range from 1.5 to 3.0. Setting to 0.0 disables "
           "outlier detection.")
      .range(0.0, 5.0);
}

static void declare_scheduler_args(config::config_builder& b, du_high_unit_scheduler_config& sched_params)
{
  b.option("--nof_preselected_newtx_ues",
           sched_params.nof_preselected_newtx_ues,
           "Number of UEs pre-selected for potential newTx allocations in a slot. The scheduling policy will only "
           "be applied to the pre-selected UEs.")
      .range(1, static_cast<int>(MAX_NOF_DU_UES));

  b.group("policy",
          "Scheduler policy configuration. By default, time-domain QoS-aware policy is used.",
          [&](config::config_builder& sub) { declare_scheduler_policy_args(sub, sched_params.policy_cfg); });
}

static void declare_drx_args(config::config_builder& b, du_high_unit_drx_config& drx_params)
{
  // Legacy CLI11 enforced membership against drx_helper::valid_*_values(). The builder taxonomy is currently
  // expressed as a .note(); a runtime validator should re-enforce membership.
  b.option("--on_duration_timer",
           drx_params.on_duration_timer,
           "Minimum duration in milliseconds that the UE stays in active mode, when DRX is configured.")
      .note("legal values: drx_helper::valid_on_duration_timer_values()");
  b.option("--inactivity_timer",
           drx_params.inactivity_timer,
           "Duration in milliseconds that the UE stays active after PDCCH reception, when DRX is configured.")
      .note("legal values: drx_helper::valid_inactivity_timer_values()");
  b.option("--retx_timer_dl",
           drx_params.retx_timer_dl,
           "Maximum duration in slots until a DL ReTX is received by the UE, when DRX is configured.")
      .note("legal values: drx_helper::valid_retx_timer_values()");
  b.option("--retx_timer_ul",
           drx_params.retx_timer_ul,
           "Maximum duration in slots until a grant for UL ReTX is received by the UE, when DRX is configured.")
      .note("legal values: drx_helper::valid_retx_timer_values()");
  b.option("--long_cycle",
           drx_params.long_cycle,
           "Duration in milliseconds between UE DRX long cycles. The value 0 is used to disable DRX")
      .note("legal values: drx_helper::valid_long_cycle_values() ∪ {0}");
}

static void declare_ul_common_args(config::config_builder& b, du_high_unit_ul_common_config& ul_common_params)
{
  b.option("--p_max", ul_common_params.p_max, "Maximum transmit power allowed in this serving cell").range(-30, 23);
  b.option("--max_pucchs_per_slot",
           ul_common_params.max_pucchs_per_slot,
           "Maximum number of PUCCH grants that can be allocated per slot")
      .range(1, static_cast<int>(MAX_PUCCH_PDUS_PER_SLOT));
  b.option("--max_ul_grants_per_slot",
           ul_common_params.max_ul_grants_per_slot,
           "Maximum number of UL grants that can be allocated per slot")
      .range(1, static_cast<int>(MAX_PUSCH_PDUS_PER_SLOT + MAX_PUCCH_PDUS_PER_SLOT));
  b.option("--min_pucch_pusch_prb_distance",
           ul_common_params.min_pucch_pusch_prb_distance,
           "Minimum PRB distance between PUCCH and UE-dedicated PUSCH grants")
      .range(0, static_cast<int>(MAX_NOF_PRBS) / 2);
}

static void declare_pusch_args(config::config_builder& b, du_high_unit_pusch_config& pusch_params)
{
  b.option("--min_ue_mcs", pusch_params.min_ue_mcs, "Minimum UE MCS").range(0, 28);
  b.option("--max_ue_mcs", pusch_params.max_ue_mcs, "Maximum UE MCS").range(0, 28);

  {
    auto [setter, getter] = make_32bit_mask_actions(pusch_params.harq_mode_b);
    b.string_action("--harq_mode_b",
                    std::move(setter),
                    std::move(getter),
                    "Set HARQ Mode B (only for NTN cells).\n"
                    "If set to true, applies the mask 0x0fffffff to set HARQ Mode B for all except the first four "
                    "HARQ processes.\n"
                    "If set to a string, it must be a 32-bit bitmap (0x… or 0b…) indicating which HARQ processes "
                    "use Mode B.\n"
                    "A bit set to 1 indicates HARQ Mode B; a bit set to 0 indicates HARQ Mode A.\n"
                    "The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are "
                    "ignored.\n",
                    "accepts \"true\", \"false\" or a 32-bit bitmap (\"0x...\" / \"0b...\").");
  }

  b.option("--nof_harqs",
           pusch_params.nof_harqs,
           "Number of UL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; "
           "otherwise, it defaults to 16.")
      .note("legal values: {16, 32}");
  b.option("--max_nof_harq_retxs",
           pusch_params.max_nof_harq_retxs,
           "Maximum number of times a UL HARQ can be retransmitted, before it gets discarded.")
      .range(0, 64);
  b.option("--harq_retx_timeout",
           pusch_params.harq_retx_timeout,
           "Maximum time, in milliseconds, between a CRC=KO and the scheduler allocating the respective "
           "HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded.")
      .range(10, 500);
  b.option("--max_consecutive_kos",
           pusch_params.max_consecutive_kos,
           "Maximum number of CRC consecutive KOs before an Radio Link Failure is reported");
  b.option("--rv_sequence", pusch_params.rv_sequence, "RV sequence for PUSCH. (e.g. [0 2 3 1]")
      .note("each element must be in {0, 1, 2, 3}");
  b.enum_option("--mcs_table",
                pusch_params.mcs_table,
                "MCS table to use PUSCH",
                {{"qam64", pusch_mcs_table::qam64},
                 {"qam256", pusch_mcs_table::qam256},
                 {"qam64lowse", pusch_mcs_table::qam64LowSe}});
  b.option("--max_rank",
           pusch_params.max_rank,
           "Maximum number of PUSCH transmission layers. The actual maximum is limited by the number of receive "
           "ports and UE capabilities.")
      .range(1, 4);
  b.option("--msg3_delta_preamble",
           pusch_params.msg3_delta_preamble,
           "msg3-DeltaPreamble, Power offset between msg3 and RACH preamble transmission")
      .range(-1, 6);
  b.option("--p0_nominal_with_grant",
           pusch_params.p0_nominal_with_grant,
           "P0 value for PUSCH with grant (except msg3). Value in dBm. Valid values must be multiple of 2 and "
           "within the [-202, 24] interval.  Default: -76")
      .note("must be a multiple of 2 within [-202, 24]");
  b.option("--msg3_delta_power",
           pusch_params.msg3_delta_power,
           "Target power level at the network receiver side, in dBm. Valid values must be multiple of 2 and "
           "within the [-6, 8] interval. Default: 8")
      .note("must be a multiple of 2 within [-6, 8]");
  b.option("--max_puschs_per_slot", pusch_params.max_puschs_per_slot, "Maximum number of PUSCH grants per slot")
      .range(1, static_cast<int>(MAX_PUSCH_PDUS_PER_SLOT));
  b.option("--beta_offset_ack_idx_1", pusch_params.beta_offset_ack_idx_1, "betaOffsetACK-Index1 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_ack_idx_2", pusch_params.beta_offset_ack_idx_2, "betaOffsetACK-Index2 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_ack_idx_3", pusch_params.beta_offset_ack_idx_3, "betaOffsetACK-Index3 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_csi_p1_idx_1",
           pusch_params.beta_offset_csi_p1_idx_1,
           "betaOffsetCSI-Part1-Index1 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_csi_p1_idx_2",
           pusch_params.beta_offset_csi_p1_idx_2,
           "betaOffsetCSI-Part1-Index2 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_csi_p2_idx_1",
           pusch_params.beta_offset_csi_p2_idx_1,
           "betaOffsetCSI-Part2-Index1 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--beta_offset_csi_p2_idx_2",
           pusch_params.beta_offset_csi_p2_idx_2,
           "betaOffsetCSI-Part2-Index2 part of UCI-OnPUSCH")
      .range(0, 31);
  b.option("--min_k2", pusch_params.min_k2, "Minimum value of K2 (difference in slots between PDCCH and PUSCH).")
      .range(1, 4);

  {
    auto [setter, getter] = make_dc_offset_actions(pusch_params.dc_offset);
    b.string_action("--dc_offset",
                    std::move(setter),
                    std::move(getter),
                    "Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for "
                    "carrier spacing, and the center of the gNB UL carrier as DC offset value 0. The user can "
                    "additionally set \"outside\" to define that the DC offset falls outside the UL carrier or "
                    "\"undetermined\" in the case the DC offset is unknown.",
                    "accepts an integer in [min,max] or one of {\"outside\",\"undetermined\",\"center\"}.");
  }

  b.option("--olla_snr_inc_step",
           pusch_params.olla_snr_inc,
           "Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled")
      .range(0.0, 1.0);
  b.option("--olla_target_bler",
           pusch_params.olla_target_bler,
           "Target UL BLER set in Outer-loop link adaptation (OLLA) algorithm")
      .range(0.0, 0.5);
  b.option("--olla_max_snr_offset",
           pusch_params.olla_max_snr_offset,
           "Maximum offset that the Outer-loop link adaptation (OLLA) can apply to the estimated UL SINR")
      .min_value(0.0);
  b.option("--dmrs_additional_position", pusch_params.dmrs_add_pos, "PUSCH DMRS additional position").range(0, 3);
  b.option("--min_rb_size", pusch_params.min_rb_size, "Minimum RB size for UE PUSCH resource allocation")
      .range(1, static_cast<int>(MAX_NOF_PRBS));
  b.option("--max_rb_size", pusch_params.max_rb_size, "Maximum RB size for UE PUSCH resource allocation")
      .range(1, static_cast<int>(MAX_NOF_PRBS));
  b.option("--start_rb", pusch_params.start_rb, "Start RB for resource allocation of UE PUSCHs")
      .range(0, static_cast<int>(MAX_NOF_PRBS));
  b.option("--end_rb", pusch_params.end_rb, "End RB for resource allocation of UE PUSCHs")
      .range(0, static_cast<int>(MAX_NOF_PRBS));
  b.option("--enable_cl_loop_pw_control",
           pusch_params.enable_closed_loop_pw_control,
           "Enable closed-loop power control for PUSCH");
  b.option("--enable_phr_bw_adaptation",
           pusch_params.enable_phr_bw_adaptation,
           "Enable bandwidth adaptation to prevent negative PHR");
  b.option("--target_sinr", pusch_params.target_pusch_sinr, "Target PUSCH SINR in dB").range(-5.0, 30.0);
  b.option("--ref_path_loss",
           pusch_params.path_loss_for_target_pusch_sinr,
           "Reference path-loss for target PUSCH SINR in dB")
      .range(50.0, 120.0);
  b.option("--pl_compensation_factor",
           pusch_params.path_loss_compensation_factor,
           "Fractional path-loss compensation factor in PUSCH power control")
      .note("legal values: {0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0}");
  b.option("--enable_transform_precoding",
           pusch_params.enable_transform_precoding,
           "Enable transform precoding for PUSCH.");
}

namespace {

std::vector<std::pair<std::string, max_pucch_code_rate>> pucch_code_rate_mapping()
{
  return {{"dot08", max_pucch_code_rate::dot_08},
          {"dot15", max_pucch_code_rate::dot_15},
          {"dot25", max_pucch_code_rate::dot_25},
          {"dot35", max_pucch_code_rate::dot_35},
          {"dot45", max_pucch_code_rate::dot_45},
          {"dot60", max_pucch_code_rate::dot_60},
          {"dot80", max_pucch_code_rate::dot_80}};
}

} // namespace

static void declare_pucch_args(config::config_builder& b, du_high_unit_pucch_config& pucch_params)
{
  b.option("--p0_nominal",
           pucch_params.p0_nominal,
           "Power control parameter P0 for PUCCH transmissions. Value in dBm. Valid values must be multiple of 2 "
           "and within the [-202, 24] interval. Default: -90")
      .note("must be a multiple of 2 within [-202, 24]");
  b.option("--pucch_resource_common",
           pucch_params.pucch_resource_common,
           "Index of PUCCH resource set for the common configuration")
      .range(0, 15);
  b.option("--sr_period_ms", pucch_params.sr_period_msec, "SR period in msec")
      .note("legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 40, 80, 160, 320}");
  b.enum_option("--formats",
                pucch_params.formats,
                "PUCCH formats combination to use. Values: {f0_and_f2, f1_and_f2, f1_and_f3, f1_and_f4}. "
                "Default: f1_and_f2",
                {{"f0_and_f2", pucch_formats::f0_and_f2},
                 {"f1_and_f2", pucch_formats::f1_and_f2},
                 {"f1_and_f3", pucch_formats::f1_and_f3},
                 {"f1_and_f4", pucch_formats::f1_and_f4}});
  b.option("--resource_set_size",
           pucch_params.res_set_size,
           "Number of PUCCH resources in each PUCCH resource set")
      .range(1, 8);
  b.option("--nof_cell_res_set_configs",
           pucch_params.nof_cell_res_set_configs,
           "Number of PUCCH Resource Set configurations that are available per cell. NOTE: the higher the number "
           "of configurations, the lower the chances UEs have to share the same PUCCH resources for HARQ-ACK.")
      .range(1, 10);
  b.option("--nof_cell_sr_res",
           pucch_params.nof_cell_sr_resources,
           "Number of PUCCH F0/F1 resources available per cell for SR")
      .range(1, 100);
  b.option("--nof_cell_csi_res",
           pucch_params.nof_cell_csi_resources,
           "Number of PUCCH F2/F3/F4 resources available per cell for CSI")
      .range(0, 100);
  b.option("--f0_intraslot_freq_hop",
           pucch_params.f0_intraslot_freq_hopping,
           "Enable intra-slot frequency hopping for PUCCH F0");
  b.option("--f1_enable_occ", pucch_params.f1_enable_occ, "Enable OCC for PUCCH F1");
  b.option("--f1_nof_cyclic_shifts",
           pucch_params.f1_nof_cyclic_shifts,
           "Number of possible cyclic shifts available for PUCCH F1 resources")
      .note("legal values: {1, 2, 3, 4, 6, 12}");
  b.option("--f1_intraslot_freq_hop",
           pucch_params.f1_intraslot_freq_hopping,
           "Enable intra-slot frequency hopping for PUCCH F1");
  b.option("--f2_max_nof_rbs", pucch_params.f2_max_nof_rbs, "Max number of RBs for PUCCH F2 resources").range(1, 16);
  b.option("--f2_max_payload",
           pucch_params.f2_max_payload_bits,
           "Min required payload capacity in bits for PUCCH F2 resources")
      .range(4, 40);
  b.enum_option("--f2_max_code_rate",
                pucch_params.f2_max_code_rate,
                "PUCCH F2 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35",
                pucch_code_rate_mapping());
  b.option("--f2_intraslot_freq_hop",
           pucch_params.f2_intraslot_freq_hopping,
           "Enable intra-slot frequency hopping for PUCCH F2");
  b.option("--f3_max_nof_rbs", pucch_params.f3_max_nof_rbs, "Max number of RBs for PUCCH F3 resources")
      .note("legal values: {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16}");
  b.option("--f3_max_payload",
           pucch_params.f3_max_payload_bits,
           "Min required payload capacity in bits for PUCCH F3 resources")
      .range(4, 40);
  b.enum_option("--f3_max_code_rate",
                pucch_params.f3_max_code_rate,
                "PUCCH F3 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35",
                pucch_code_rate_mapping());
  b.option("--f3_intraslot_freq_hop",
           pucch_params.f3_intraslot_freq_hopping,
           "Enable intra-slot frequency hopping for PUCCH F3");
  b.option("--f3_additional_dmrs", pucch_params.f3_additional_dmrs, "Enable additional DM-RS for PUCCH F3");
  b.option("--f3_pi2_bpsk", pucch_params.f3_pi2_bpsk, "Enable pi/2-BPSK modulation for PUCCH F3");
  b.enum_option("--f4_max_code_rate",
                pucch_params.f4_max_code_rate,
                "PUCCH F4 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35",
                pucch_code_rate_mapping());
  b.option("--f4_intraslot_freq_hop",
           pucch_params.f4_intraslot_freq_hopping,
           "Enable intra-slot frequency hopping for PUCCH F4");
  b.option("--f4_additional_dmrs", pucch_params.f4_additional_dmrs, "Enable additional DM-RS for PUCCH F4");
  b.option("--f4_pi2_bpsk", pucch_params.f4_pi2_bpsk, "Enable pi/2-BPSK modulation for PUCCH F4");
  b.option("--f4_occ_length", pucch_params.f4_occ_length, "OCC length for PUCCH F4").note("legal values: {2, 4}");
  b.option("--f4_enable_occ", pucch_params.f4_enable_occ, "Enable OCC multiplexing for PUCCH F4");
  b.option("--min_k1",
           pucch_params.min_k1,
           "Minimum value of K1 (difference in slots between PDSCH and HARQ-ACK). Lower k1 values will reduce "
           "latency, but place a stricter requirement on the UE decode latency.")
      .range(1, 4);
  b.option("--max_consecutive_kos",
           pucch_params.max_consecutive_kos,
           "Maximum number of consecutive undecoded PUCCH F2 for CSI before an Radio Link Failure is reported");
  b.option("--enable_cl_loop_pw_control",
           pucch_params.enable_closed_loop_pw_control,
           "Enable closed-loop power control for PUCCH");
  b.option("--target_sinr_f0", pucch_params.pucch_f0_sinr_target_dB, "Target PUCCH F0 SINR in dB").range(-10.0, 20.0);
  b.option("--target_sinr_f2", pucch_params.pucch_f2_sinr_target_dB, "Target PUCCH F2 SINR in dB").range(-10.0, 20.0);
  b.option("--target_sinr_f3", pucch_params.pucch_f3_sinr_target_dB, "Target PUCCH F3 SINR in dB").range(-15.0, 10.0);
}

static void declare_srs_args(config::config_builder& b, du_high_unit_srs_config& srs_params)
{
  b.enumeration("--type_enabled",
                srs_params.srs_type_enabled,
                "Enable/disable SRS and set resource type",
                {"disabled", "periodic", "aperiodic"});
  b.option("--period_ms",
           srs_params.srs_period_prohibit_time_ms,
           "SRS period in ms. For aperiodic SRS, it indicates a tentative timing, and should not be interpreted "
           "as a precise period. The SRS period needs to be compatible with the subcarrier spacing")
      .note("legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560}");
  b.option("--max_nof_sym_per_slot",
           srs_params.max_nof_symbols_per_slot,
           "Number of symbols for UL slot that are reserved for the SRS cell resources")
      .range(1, 6);
  b.option("--nof_sym_per_resource", srs_params.nof_symbols, "Number of symbols per SRS resource")
      .note("legal values: {1, 2, 4}");
  b.option("--c_srs",
           srs_params.c_srs,
           "C_SRS parameter for SRS. If not set, it's computed automatically from the cell parameters")
      .range(0, 63);
  b.option("--freq_domain_shift",
           srs_params.freq_domain_shift,
           "SRS frequency domain shift. Only applies if c_srs is set")
      .range(0, 268);
  b.option("--tx_comb", srs_params.tx_comb, "SRS TX comb size").note("legal values: {2, 4}");
  b.option("--cyclic_shift_reuse",
           srs_params.cyclic_shift_reuse_factor,
           "SRS cyclic shift reuse factor. It needs to be compatible with the TX comb and number of UL antenna "
           "ports")
      .note("legal values: {1, 2, 3, 4, 6}");
  b.option("--sequence_id_reuse",
           srs_params.sequence_id_reuse_factor,
           "Enable the reuse of SRS sequence id with the set reuse factor")
      .note("legal values: {1, 2, 3, 5, 6, 10, 15, 30}");
  b.option("p0",
           srs_params.p0,
           "P0 value for SRS. Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval. "
           "Default: -84")
      .note("must be a multiple of 2 within [-202, 24]");
}

static void declare_si_sched_info(config::config_builder&                          b,
                                  du_high_unit_sib_config::si_sched_info_config& si_sched_info)
{
  b.option("--si_period", si_sched_info.si_period_rf, "SI message scheduling period in radio frames")
      .note("legal values: {8, 16, 32, 64, 128, 256, 512}");
  b.option("--sib_mapping",
           si_sched_info.sib_mapping_info,
           "Mapping of SIB types to SI-messages. SIB numbers should not be repeated")
      .note("each element must be in {2, 3, 4, 5, 6, 7, 8, 19}");
  b.option("--si_window_position", si_sched_info.si_window_position, "SI window position of the associated SI-message")
      .range(1, 256);
}

static void declare_ra_prioritization_info(config::config_builder&                                 b,
                                           du_high_unit_rach_config::ra_prioritization_slice_info& ra_info)
{
  b.option("--power_ramp_step_high_priority",
           ra_info.power_ramp_step_high_priority,
           "Power ramping step applied for prioritized random access procedure [dB].")
      .note("legal values: {0, 2, 4, 6}");
  b.option("--scaling_factor_bi",
           ra_info.scaling_factor_bi,
           "Scaling factor for backoff indicator (BI) for the prioritized RA procedure.")
      .note("legal values: {0.0, 0.25, 0.5, 0.75}");
  b.option("--nsag_ids", ra_info.nsag_ids, "NSAGs associated with this prioritized RA configuration.");
}

static void declare_two_step_rach_args(config::config_builder&                  b,
                                       du_high_unit_rach_config::two_step_info& two_step_params)
{
  b.option("--cb_preambles_per_ssb_per_shared_ro",
           two_step_params.cb_preambles_per_ssb_per_shared_ro,
           "Number of CB preambles per SSB per shared RACH occasion for 2-step RA")
      .range(1, 60);
  b.option("--msgA_rsrp_thres_dbm",
           two_step_params.msga_rsrp_thres_dbm,
           "RSRP threshold in dBm above which the UE selects 2-step RA over 4-step RA")
      .range(-156, -29);
  b.option("--msgB_response_window_slots",
           two_step_params.msgb_response_window_slots,
           "MsgB response window length in slots")
      .note("legal values: {1, 2, 4, 8, 10, 20, 40, 80, 160, 320}");
  b.option("--td_offset", two_step_params.td_offset, "Time-domain offset in slots from the PRACH slot to the MsgA PUSCH slot")
      .range(1, 32);
  b.option("--pusch_td_res_index",
           two_step_params.pusch_td_res_index,
           "Index into the PUSCH-TimeDomainAllocationResource table for MsgA PUSCH scheduling");
  b.option("--mcs", two_step_params.mcs, "MCS index for MsgA PUSCH transmission").range(0, 28);
  b.option("--nof_prbs_per_msgA_po",
           two_step_params.nof_prbs_per_msga_po,
           "Number of PRBs per MsgA PUSCH occasion")
      .range(1, 32);
  b.option("--prb_start",
           two_step_params.prb_start,
           "Frequency offset in PRBs of the lowest MsgA PUSCH occasion from PRB 0");
  b.option("--po_fdm",
           two_step_params.po_fdm,
           "Number of MsgA PUSCH occasions FDMed in one time instance")
      .note("legal values: {1, 2, 4, 8}");
}

static void declare_prach_args(config::config_builder& b, du_high_unit_rach_config& prach_params)
{
  b.option("--prach_config_index",
           prach_params.prach_config_index,
           "PRACH configuration index. If not set, the value is derived, so that the PRACH fits in an UL slot")
      .range(0, 255);
  b.option("--prach_root_sequence_index",
           prach_params.prach_root_sequence_index,
           "PRACH root sequence index. NOTE: values: [0, 837] for PRACH format 0, 1, 2, 3. [0, 137] for other "
           "formats")
      .range(0, 837);
  b.option("--zero_correlation_zone", prach_params.zero_correlation_zone, "Zero correlation zone index").range(0, 15);
  b.option("--fixed_msg3_mcs", prach_params.fixed_msg3_mcs, "Fixed message 3 MCS").range(0, 28);
  b.option("--max_msg3_harq_retx", prach_params.max_msg3_harq_retx, "Maximum number of message 3 HARQ retransmissions")
      .range(0, 4);
  b.option("--total_nof_ra_preambles",
           prach_params.total_nof_ra_preambles,
           "Number of different contention-based PRACH preambles per occasion. If less than 64 preambles are used, "
           "the remaining preambles can be used for contention-free PRACHs")
      .range(1, 64);
  b.option("--cfra_enabled",
           prach_params.cfra_enabled,
           "Whether to enable Contention-free Random Access (CFRA). If enabled, the total_nof_ra_preambles must be "
           "lower than 64");
  b.option("--prach_frequency_start",
           prach_params.prach_frequency_start,
           "PRACH message frequency offset in PRBs. NOTE: When setting this parameter, it's up to user the ensure "
           "the PRACH opportunities do not overlap with the PUCCH resources")
      .range(0, 274);
  b.option("--preamble_rx_target_pw",
           prach_params.preamble_rx_target_pw,
           "Target power level at the network receiver side, in dBm")
      .note("must be a multiple of 2 within [-202, -60]");
  b.option("--preamble_trans_max",
           prach_params.preamble_trans_max,
           "Max number of RA preamble transmissions performed before declaring a failure")
      .note("legal values: {3, 4, 5, 6, 7, 8, 10, 20, 50, 100, 200}");
  b.option("--power_ramping_step_db", prach_params.power_ramping_step_db, "Power ramping steps for PRACH")
      .note("legal values: {0, 2, 4, 6}");
  b.option("--ports", prach_params.ports, "List of antenna ports");
  b.option("--nof_ssb_per_ro", prach_params.nof_ssb_per_ro, "Number of SSBs per RACH occasion")
      .note("legal values: {1}");
  b.option("--nof_cb_preambles_per_ssb",
           prach_params.nof_cb_preambles_per_ssb,
           "Number of Contention Based preambles per SSB")
      .range(1, 64);
  b.option("--ra_resp_window", prach_params.ra_resp_window, "RA-Response window length in number of slots.")
      .note("legal values: {1, 2, 4, 8, 10, 20, 40, 80}");
  b.option("--nof_prach_guardbands_rbs",
           prach_params.nof_prach_guardbands_rbs,
           "Number of RBs that are used as guardband on each side of the PRACH RBs interval for short PRACH "
           "formats.")
      .range(1, 10);

  b.array_of("--slice_based_ra_prioritization",
             prach_params.ra_prio_slice_info_list,
             "List of configurations for slice-based RA prioritization",
             [](config::config_builder& el, du_high_unit_rach_config::ra_prioritization_slice_info& ra) {
               declare_ra_prioritization_info(el, ra);
             });

  // two_step is std::optional. Legacy code emplaced only when the "two_step" subcommand was used.
  // Eagerly emplace; gating must move to a runtime validator. (TODO)
  if (!prach_params.two_step.has_value()) {
    prach_params.two_step.emplace();
  }
  b.group("two_step", "Two-step RACH (MsgA/MsgB) configuration",
          [&](config::config_builder& sub) { declare_two_step_rach_args(sub, *prach_params.two_step); });
}

static void declare_sib2_config_args(config::config_builder& b, du_high_unit_sib_config::sib2_config& sib2_cfg)
{
  b.option("--q_hyst", sib2_cfg.q_hyst, "Hysteresis value for ranking criteria.")
      .note("legal values: {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24}");
  b.option("--thresh_serving_low_p",
           sib2_cfg.thresh_serving_low_p,
           "Rx level threshold used by the UE on the serving cell when reselecting towards a lower priority "
           "RAT/frequency.")
      .range(0, 31);
  b.option("--cell_reselection_priority",
           sib2_cfg.cell_reselection_priority,
           "Integer part of the cell reselection priority for the frequency of this cell")
      .range(0, 7);
  b.option("--q_rx_lev_min", sib2_cfg.q_rx_lev_min, "Minimum required Rx level in the cell in dBm")
      .note("must be an even value within [-140, -44]");
  b.option("--s_intra_search_p",
           sib2_cfg.s_intra_search_p,
           "Rx level threshold for intra frequency measurements in dB")
      .note("must be an even value within [0, 62]");
  b.option("--t_reselection_nr", sib2_cfg.t_reselection_nr, "Cell reselection timer value in seconds").range(0, 7);
}

static void declare_intra_freq_neigh_cell_info_args(
    config::config_builder&                                             b,
    du_high_unit_sib_config::sib3_config::intra_freq_neigh_cell_config& neigh_info)
{
  b.option("--pci", neigh_info.pci, "PCI").range(0, 1007);
  b.option("--q_offset_cell", neigh_info.q_offset_cell, "PCI")
      .note("legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, "
            "6, 8, 10, 12, 14, 16, 18, 20, 22, 24}");
}

static void declare_pci_range_args(config::config_builder& b, pci_range_config& range)
{
  b.option("--start", range.start, "Range start").required().range(0, 1007);
  b.option("--size", range.size, "Range size")
      .required()
      .note("legal values: {1, 4, 8, 12, 16, 24, 32, 48, 64, 84, 96, 128, 168, 252, 504, 1008}");
}

static void declare_sib3_config_args(config::config_builder& b, du_high_unit_sib_config::sib3_config& sib3_cfg)
{
  b.array_of("--intra_freq_neigh_cell_list",
             sib3_cfg.intra_freq_neigh_cell_list,
             "Intra frequency neighbor cell list",
             [](config::config_builder&                                             el,
                du_high_unit_sib_config::sib3_config::intra_freq_neigh_cell_config& neigh) {
               declare_intra_freq_neigh_cell_info_args(el, neigh);
             });
  b.array_of("--intra_freq_excluded_cell_list",
             sib3_cfg.intra_freq_excluded_cell_list,
             "Intra frequency excluded cell list",
             [](config::config_builder& el, pci_range_config& range) { declare_pci_range_args(el, range); });
}

static void declare_inter_freq_carrier_freq_info_args(
    config::config_builder&                                               b,
    du_high_unit_sib_config::sib4_config::inter_freq_carrier_freq_config& config)
{
  b.option("--arfcn", config.arfcn, "ARFCN");

  // ssb_scs: parse via to_subcarrier_spacing; render via to_string.
  auto ssb_scs_setter = [&scs = config.ssb_scs](const std::string& value) {
    auto parsed = to_subcarrier_spacing(value);
    if (parsed != subcarrier_spacing::invalid) {
      scs = parsed;
    }
  };
  auto ssb_scs_getter = [&scs = config.ssb_scs]() -> std::string {
    return scs == subcarrier_spacing::invalid ? std::string{} : std::string(to_string(scs));
  };
  b.string_action("--ssb_scs",
                  std::function<void(const std::string&)>(ssb_scs_setter),
                  std::function<std::string()>(ssb_scs_getter),
                  "SSB subcarrier spacing",
                  "accepts SCS strings (e.g. \"15kHz\", \"30kHz\")");

  b.option("--derive_ssb_index_from_cell", config.derive_ssb_index_from_cell, "Derive SSB index from cell");
  b.option("--q_rx_lev_min", config.q_rx_lev_min, "Minimum required Rx level in the cell in dBm")
      .note("must be an even value within [-140, -44]");
  b.option("--thresh_x_high_p",
           config.thresh_x_high_p,
           "Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB")
      .note("must be an even value within [0, 62]");
  b.option("--thresh_x_low_p",
           config.thresh_x_low_p,
           "Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB")
      .note("must be an even value within [0, 62]");
  b.option("--q_offset_freq",
           config.q_offset_freq,
           "Frequency specific offset in dB for equal priority NR frequencies.")
      .note("legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, "
            "6, 8, 10, 12, 14, 16, 18, 20, 22, 24}");
}

static void declare_sib4_config_args(config::config_builder& b, du_high_unit_sib_config::sib4_config& sib4_cfg)
{
  b.array_of("--inter_freq_carrier_freq_list",
             sib4_cfg.inter_freq_carrier_freq_list,
             "Inter frequency carrier frequency list",
             [](config::config_builder& el, du_high_unit_sib_config::sib4_config::inter_freq_carrier_freq_config& cfg) {
               declare_inter_freq_carrier_freq_info_args(el, cfg);
             });
}

static void declare_carrier_freq_eutra_args(config::config_builder&                                          b,
                                            du_high_unit_sib_config::sib5_config::carrier_freq_eutra_config& config)
{
  b.option("--earfcn", config.earfcn, "EARFCN");
  b.option("--allowed_meas_bandwidth", config.allowed_meas_bandwidth, "Allowed measurement bandwidth")
      .note("legal values: {6, 15, 25, 50, 75, 100}");
  b.option("--presence_antenna_port1", config.presence_antenna_port1, "Whether all neighbor cells use Antenna Port 1");
  b.option("--cell_reselection_priority",
           config.cell_reselection_priority,
           "Integer part of the cell reselection priority for the frequency of this cell")
      .range(0, 7);
  b.option("--thresh_x_high",
           config.thresh_x_high,
           "Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB")
      .note("must be an even value within [0, 62]");
  b.option("--thresh_x_low",
           config.thresh_x_low,
           "Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB")
      .note("must be an even value within [0, 62]");
  b.option("--q_rx_lev_min", config.q_rx_lev_min, "Minimum required Rx level in the cell in dBm")
      .note("must be an even value within [-140, -44]");
  b.option("--q_qual_min", config.q_qual_min, "Minimum required quality level in the cell in dB").range(-34, -3);
  b.option("--p_max_eutra",
           config.p_max_eutra,
           "Maximum allowed transmission power in dBm on the (uplink) carrier frequency.")
      .range(-30, 33);
}

static void declare_sib5_config_args(config::config_builder& b, du_high_unit_sib_config::sib5_config& sib5_cfg)
{
  b.option("--t_reselection_eutra", sib5_cfg.t_reselection_eutra, "Cell reselection timer value in seconds").range(0, 7);
  b.array_of("--carrier_freq_list_eutra",
             sib5_cfg.carrier_freq_list_eutra,
             "EUTRA carrier frequency list",
             [](config::config_builder& el, du_high_unit_sib_config::sib5_config::carrier_freq_eutra_config& cfg) {
               declare_carrier_freq_eutra_args(el, cfg);
             });
}

static void declare_sib16_slice_info_args(config::config_builder&                                   b,
                                          du_high_unit_sib_config::sib16_config::slice_info_config& slice)
{
  b.option("--nsag_id", slice.nsag_id, "NSAG ID").range(0, 255);
  b.option("--allowed",
           slice.allowed,
           "Whether the list of cells in this slice info is allowed (true) or excluded (false)");
  b.option("--reselection_priority",
           slice.reselection_priority,
           "Priority associated with this cell reselection slice")
      .note("must be a multiple of 0.2 within [0.0, 7.8]");
  b.array_of("--cells_allowed",
             slice.cells,
             "Slice cell list entries",
             [](config::config_builder& el, pci_range_config& range) { declare_pci_range_args(el, range); });
}

static void
declare_sib16_freq_prio_slicing_args(config::config_builder&                                             b,
                                     du_high_unit_sib_config::sib16_config::freq_priority_slicing_config& freq_cfg)
{
  b.option("--dl_implicit_carrier_freq",
           freq_cfg.dl_implicit_carrier_freq,
           "DL implicit carrier frequency index for this slicing entry")
      .range(0, 8);
  b.array_of("--slice_info_list",
             freq_cfg.slice_info_list,
             "Slice info list entries",
             [](config::config_builder& el, du_high_unit_sib_config::sib16_config::slice_info_config& slice) {
               declare_sib16_slice_info_args(el, slice);
             });
}

static void declare_sib16_config_args(config::config_builder& b, du_high_unit_sib_config::sib16_config& sib16_cfg)
{
  b.array_of("--freq_prio_list_slicing",
             sib16_cfg.freq_prio_list_slicing,
             "Frequency priority slicing list entries",
             [](config::config_builder& el, du_high_unit_sib_config::sib16_config::freq_priority_slicing_config& freq) {
               declare_sib16_freq_prio_slicing_args(el, freq);
             });
}

static void declare_etws_args(config::config_builder& b, du_high_unit_sib_config::etws_config& sib_params)
{
  b.option("--message_id", sib_params.message_id, "ETWS message ID.").range(0, 0xffff);
  b.option("--serial_num", sib_params.serial_num, "ETWS message serial number.").range(0, 0xffff);
  b.option("--warning_type", sib_params.warning_type, "ETWS warning type.").range(0, 0xffff);
  b.option("--data_coding_scheme", sib_params.data_coding_scheme, "ETWS message CBS coding scheme.").range(0, 0xff);
  b.option("--warning_message",
           sib_params.warning_message,
           "ETWS warning message. Max. Length and character support depends on the chosen coding scheme.");
}

static void declare_cmas_args(config::config_builder& b, du_high_unit_sib_config::cmas_config& sib_params)
{
  b.option("--message_id", sib_params.message_id, "CMAS message ID.").range(0, 0xffff);
  b.option("--serial_num", sib_params.serial_num, "CMAS message serial number.").range(0, 0xffff);
  b.option("--data_coding_scheme", sib_params.data_coding_scheme, "CMAS message CBS coding scheme.").range(0, 0xff);
  b.option("--warning_message",
           sib_params.warning_message,
           "CMAS warning message. Max. Length and character support depends on the chosen coding scheme.");
}

static void declare_sib_args(config::config_builder& b, du_high_unit_sib_config& sib_params)
{
  b.option("--si_window_length",
           sib_params.si_window_len_slots,
           "The length of the SI scheduling window, in slots. It must be always shorter or equal to the period of "
           "the SI message.")
      .note("legal values: {5, 10, 20, 40, 80, 160, 320, 640, 1280}");

  b.array_of("--si_sched_info",
             sib_params.si_sched_info,
             "Configures the scheduling for each of the SI-messages broadcast by the gNB",
             [](config::config_builder& el, du_high_unit_sib_config::si_sched_info_config& si) {
               declare_si_sched_info(el, si);
             });

  // Optional sub-structs; emplace eagerly (TODO: subcommand-presence gating must be re-introduced via runtime
  // validator).
  if (!sib_params.sib2_cfg.has_value()) {
    sib_params.sib2_cfg.emplace();
  }
  b.group("sib2", "SIB2 parameters",
          [&](config::config_builder& sub) { declare_sib2_config_args(sub, *sib_params.sib2_cfg); });

  if (!sib_params.sib3_cfg.has_value()) {
    sib_params.sib3_cfg.emplace();
  }
  b.group("sib3", "SIB3 parameters",
          [&](config::config_builder& sub) { declare_sib3_config_args(sub, *sib_params.sib3_cfg); });

  if (!sib_params.sib4_cfg.has_value()) {
    sib_params.sib4_cfg.emplace();
  }
  b.group("sib4", "SIB4 parameters",
          [&](config::config_builder& sub) { declare_sib4_config_args(sub, *sib_params.sib4_cfg); });

  if (!sib_params.sib5_cfg.has_value()) {
    sib_params.sib5_cfg.emplace();
  }
  b.group("sib5", "SIB5 parameters",
          [&](config::config_builder& sub) { declare_sib5_config_args(sub, *sib_params.sib5_cfg); });

  if (!sib_params.sib16_cfg.has_value()) {
    sib_params.sib16_cfg.emplace();
  }
  b.group("sib16", "SIB16 parameters",
          [&](config::config_builder& sub) { declare_sib16_config_args(sub, *sib_params.sib16_cfg); });

  if (!sib_params.etws_cfg.has_value()) {
    sib_params.etws_cfg.emplace();
  }
  b.group("etws", "ETWS configuration parameters",
          [&](config::config_builder& sub) { declare_etws_args(sub, *sib_params.etws_cfg); });

  if (!sib_params.cmas_cfg.has_value()) {
    sib_params.cmas_cfg.emplace();
  }
  b.group("cmas", "CMAS configuration parameters",
          [&](config::config_builder& sub) { declare_cmas_args(sub, *sib_params.cmas_cfg); });

  b.option("--t300",
           sib_params.ue_timers_and_constants.t300,
           "RRC Connection Establishment timer in ms. The timer starts upon transmission of RRCSetupRequest.")
      .note("legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000}");
  b.option("--t301",
           sib_params.ue_timers_and_constants.t301,
           "RRC Connection Re-establishment timer in ms. The timer starts upon transmission of "
           "RRCReestablishmentRequest.")
      .note("legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000}");
  b.option("--t310",
           sib_params.ue_timers_and_constants.t310,
           "Out-of-sync timer in ms. The timer starts upon detecting physical layer problems for the SpCell i.e. "
           "upon receiving N310 consecutive out-of-sync indications from lower layers.")
      .note("legal values: {0, 50, 100, 200, 500, 1000, 2000}");
  b.option("--n310",
           sib_params.ue_timers_and_constants.n310,
           "Out-of-sync counter. The counter is increased upon reception of \"out-of-sync\" from lower layer "
           "while the timer T310 is stopped. Starts the timer T310, when configured value is reached.")
      .note("legal values: {1, 2, 3, 4, 6, 8, 10, 20}");
  b.option("--t311",
           sib_params.ue_timers_and_constants.t311,
           "RRC Connection Re-establishment procedure timer in ms. The timer starts upon initiating the RRC "
           "connection re-establishment procedure.")
      .note("legal values: {1000, 3000, 5000, 10000, 15000, 20000, 30000}");
  b.option("--n311",
           sib_params.ue_timers_and_constants.n311,
           "In-sync counter. The counter is increased upon reception of the \"in-sync\" from lower layer while "
           "the timer T310 is running. Stops the timer T310, when configured value is reached.")
      .note("legal values: {1, 2, 3, 4, 5, 6, 8, 10}");
  b.option("--t319",
           sib_params.ue_timers_and_constants.t319,
           "RRC Connection Resume timer in ms. The timer starts upon transmission of RRCResumeRequest "
           "or RRCResumeRequest1.")
      .note("legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000}");
}

static void declare_slicing_scheduling_args(config::config_builder&               b,
                                            du_high_unit_cell_slice_sched_config& slice_sched_params)
{
  b.option("--min_prb_policy_ratio",
           slice_sched_params.min_prb_policy_ratio,
           "Minimum percentage of PRBs to be allocated to the slice")
      .range(0, 100);
  b.option("--max_prb_policy_ratio",
           slice_sched_params.max_prb_policy_ratio,
           "Maximum percentage of PRBs to be allocated to the slice")
      .range(1, 100);
  b.option("--ded_prb_policy_ratio",
           slice_sched_params.ded_prb_policy_ratio,
           "Dedicated percentage of PRBs to be allocated to the slice")
      .range(1, 100);
  b.option("--priority", slice_sched_params.priority, "Slice priority").range(0, 254);

  b.group("policy",
          "Scheduler policy configuration for the slice. If not specified, the policy configured for the cell is "
          "used",
          [&](config::config_builder& sub) { declare_scheduler_policy_args(sub, slice_sched_params.slice_policy_cfg); });
}

static void declare_slicing_args(config::config_builder& b, du_high_unit_cell_slice_config& slice_params)
{
  b.option("--sst", slice_params.sst, "Slice Service Type").range(0, 255);
  b.option("--sd", slice_params.sd, "Service Differentiator").range(0, 0xffffff);
  b.group("sched_cfg",
          "Slice scheduling configuration",
          [&](config::config_builder& sub) { declare_slicing_scheduling_args(sub, slice_params.sched_cfg); });
}

static void declare_rlm_args(config::config_builder& b, du_high_unit_rlm_config& rlm_params)
{
  b.enum_option("--rlm_resource_type",
                rlm_params.resource_type,
                "Radio Link Monitoring resource detection type {default_type, ssb, csi_rs, ssb_and_csi_rs}. "
                "Default: default_type",
                {{"default_type", rlm_resource_type::default_type},
                 {"ssb", rlm_resource_type::ssb},
                 {"csi_rs", rlm_resource_type::csi_rs},
                 {"ssb_and_csi_rs", rlm_resource_type::ssb_and_csi_rs}});
}

static void declare_common_cell_args(config::config_builder& b, du_high_unit_base_cell_config& cell_params)
{
  b.option("--pci", cell_params.pci, "PCI").range(0, 1007);
  b.option("--sector_id",
           cell_params.sector_id,
           "Sector ID (4-14 bits). This value is concatenated with the gNB Id to form the NR Cell Identity "
           "(NCI). If not specified, a unique value for the DU is automatically derived")
      .range(0, (1 << 14) - 1);
  // dl_f_ref_arfcn is a bounded_integer<unsigned, 0, 3279165>; the builder taxonomy doesn't yet expose a
  // bounded_integer scalar, so it is bridged through string_action.
  {
    auto& arfcn  = cell_params.dl_f_ref_arfcn;
    auto  setter = [&arfcn](const std::string& value) {
      try {
        arfcn = static_cast<unsigned>(std::stoul(value));
      } catch (...) {
        // Leave unchanged on parse failure; CLI11's lexical conversion would have caught this previously.
      }
    };
    auto getter = [&arfcn]() -> std::string { return std::to_string(arfcn.value()); };
    b.string_action("--dl_arfcn",
                    std::function<void(const std::string&)>(setter),
                    std::function<std::string()>(getter),
                    "Downlink ARFCN",
                    "non-negative integer; valid range [0, 3279165]");
  }
  b.auto_enum_option("--band", cell_params.band, "NR band");

  // common_scs is a subcarrier_spacing enum parsed from strings like "15kHz". string_action handles both directions.
  auto common_scs_setter = [&scs = cell_params.common_scs](const std::string& value) {
    auto parsed = to_subcarrier_spacing(value);
    if (parsed != subcarrier_spacing::invalid) {
      scs = parsed;
    }
  };
  auto common_scs_getter = [&scs = cell_params.common_scs]() -> std::string {
    return scs == subcarrier_spacing::invalid ? std::string{} : std::string(to_string(scs));
  };
  b.string_action("--common_scs",
                  std::function<void(const std::string&)>(common_scs_setter),
                  std::function<std::string()>(common_scs_getter),
                  "Cell common subcarrier spacing",
                  "accepts SCS strings (e.g. \"15kHz\", \"30kHz\")");

  b.option("--channel_bandwidth_MHz", cell_params.channel_bw_mhz, "Channel bandwidth in MHz")
      .note("legal values: {5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100, 200, 400}");

  b.option("--nof_antennas_ul", cell_params.nof_antennas_ul, "Number of antennas in uplink");
  b.option("--nof_antennas_dl", cell_params.nof_antennas_dl, "Number of antennas in downlink");

  b.option("--plmn", cell_params.plmn, "PLMN").pattern("[0-9]{5,6}");
  b.option("--additional_plmns", cell_params.additional_plmns, "List of PLMNs")
      .note("each element must be a valid PLMN string");
  b.option("--tac", cell_params.tac, "TAC").note("values 0 and 0xfffffe are reserved; value must fit in 24 bits");

  b.option("--enabled", cell_params.enabled, "Automatically activate the cell on startup");
  b.option("--cell_barred", cell_params.cell_barred, "MIB cellBarred: if true, UEs cannot camp on this cell");
  b.option("--intra_freq_reselection",
           cell_params.intra_freq_reselection,
           "MIB intraFreqReselection: if true, intra-frequency cell reselection is allowed when cell is barred");
  b.option("--q_rx_lev_min",
           cell_params.q_rx_lev_min,
           "q-RxLevMin, required minimum received RSRP level for cell selection/re-selection, in dBm")
      .range(-70, -22);
  b.option("--q_qual_min",
           cell_params.q_qual_min,
           "q-QualMin, required minimum received RSRQ level for cell selection/re-selection, in dB")
      .range(-43, -12);
  b.option("--pcg_p_nr_fr1",
           cell_params.pcg_cfg.p_nr_fr1,
           "p-nr-fr1, maximum total TX power to be used by the UE in this NR cell group across in FR1")
      .range(-30, 23);

  b.group("mac_cell_group", "MAC Cell Group parameters",
          [&](config::config_builder& sub) { declare_mac_cell_group_args(sub, cell_params.mcg_cfg); });
  b.group("ssb", "SSB parameters", [&](config::config_builder& sub) { declare_ssb_args(sub, cell_params.ssb_cfg); });
  b.group("sib", "SIB configuration parameters",
          [&](config::config_builder& sub) { declare_sib_args(sub, cell_params.sib_cfg); });
  b.group("ul_common", "UL common parameters",
          [&](config::config_builder& sub) { declare_ul_common_args(sub, cell_params.ul_common_cfg); });
  b.group("pdcch", "PDCCH parameters",
          [&](config::config_builder& sub) { declare_pdcch_args(sub, cell_params.pdcch_cfg); });
  b.group("pdsch", "PDSCH parameters",
          [&](config::config_builder& sub) { declare_pdsch_args(sub, cell_params.pdsch_cfg); });
  b.group("pusch", "PUSCH parameters",
          [&](config::config_builder& sub) { declare_pusch_args(sub, cell_params.pusch_cfg); });
  b.group("pucch", "PUCCH parameters",
          [&](config::config_builder& sub) { declare_pucch_args(sub, cell_params.pucch_cfg); });
  b.group("srs", "SRS parameters", [&](config::config_builder& sub) { declare_srs_args(sub, cell_params.srs_cfg); });
  b.group("prach", "PRACH parameters",
          [&](config::config_builder& sub) { declare_prach_args(sub, cell_params.prach_cfg); });

  // tdd_ul_dl_cfg is std::optional; emplace eagerly (TODO: presence gating via runtime validator).
  if (!cell_params.tdd_ul_dl_cfg.has_value()) {
    cell_params.tdd_ul_dl_cfg.emplace();
  }
  b.group("tdd_ul_dl_cfg", "TDD UL DL configuration parameters",
          [&](config::config_builder& sub) { declare_tdd_ul_dl_args(sub, *cell_params.tdd_ul_dl_cfg); });

  b.group("paging", "Paging parameters",
          [&](config::config_builder& sub) { declare_paging_args(sub, cell_params.paging_cfg); });
  b.group("csi", "CSI-Meas parameters",
          [&](config::config_builder& sub) { declare_csi_args(sub, cell_params.csi_cfg); });
  b.group("scheduler", "Scheduler parameters",
          [&](config::config_builder& sub) { declare_scheduler_args(sub, cell_params.scheduler_cfg); });
  b.group("ta", "Time Advance (TA) parameters",
          [&](config::config_builder& sub) { declare_ta_control_args(sub, cell_params.ta_cfg); });
  b.group("drx", "DRX parameters",
          [&](config::config_builder& sub) { declare_drx_args(sub, cell_params.drx_cfg); });

  b.array_of("--slicing",
             cell_params.slice_cfg,
             "Network slicing configuration",
             [](config::config_builder& el, du_high_unit_cell_slice_config& slice) {
               declare_slicing_args(el, slice);
             });

  // NTN configuration.
  configure_cli11_cell_ntn_args(b, cell_params.ntn_cfg);

  b.group("rlm", "Radio Link Monitoring parameters",
          [&](config::config_builder& sub) { declare_rlm_args(sub, cell_params.rlm_cfg); });
}

static void declare_cells_args(config::config_builder& b, du_high_unit_cell_config& cell_params)
{
  declare_common_cell_args(b, cell_params.cell);
}

static void declare_test_ue_mode_args(config::config_builder& b, du_high_unit_test_mode_ue_config& test_params)
{
  b.option("--rnti", test_params.rnti, "C-RNTI (0x0 if not configured)")
      .range(to_value(rnti_t::INVALID_RNTI), to_value(rnti_t::MAX_CRNTI));
  b.option("--nof_ues", test_params.nof_ues, "Number of test UE(s) to create.").range(1, MAX_NOF_DU_UES);
  b.option("--ue_creation_stagger_slots",
           test_params.ue_creation_stagger_slots,
           "Number of slots between consecutive test mode UE creations")
      .range(0, 10240);
  b.option("--auto_ack_indication_delay",
           test_params.auto_ack_indication_delay,
           "Delay before the UL and DL HARQs are automatically ACKed. This feature should only be used if the UL "
           "PHY is not operational");
  b.option("--attach_detach_duration_ms",
           test_params.attach_detach_duration_ms,
           "Duration in milliseconds of active traffic after all UEs are established before they are released and "
           "recreated. When set, UEs cycle indefinitely through attach, traffic, and detach. Unset disables cycling.")
      .range(100, 10000);
  b.option("--attach_detach_guard_duration_ms",
           test_params.attach_detach_guard_duration_ms,
           "Guard period duration in milliseconds between a release cycle and the next creation cycle.")
      .range(100, 60000);
  b.option("--pdsch_active", test_params.pdsch_active, "PDSCH enabled");
  b.option("--pusch_active", test_params.pusch_active, "PUSCH enabled");
  b.option("--cqi", test_params.cqi, "Channel Quality Information (CQI) to be forwarded to test UE.").range(1, 15);
  b.option("--ri", test_params.ri, "Rank Indicator (RI) to be forwarded to test UE.").range(1, 4);
  b.option("--pmi", test_params.pmi, "Precoder Matrix Indicator (PMI) to be forwarded to test UE.").range(0, 3);
  b.option("--i_1_1",
           test_params.i_1_1,
           "Precoder Matrix codebook index \"i_1_1\" to be forwarded to test UE, in the case of more than 2 antennas.")
      .range(0, 7);
  b.option("--i_1_3",
           test_params.i_1_3,
           "Precoder Matrix codebook index \"i_1_3\" to be forwarded to test UE, in the case of more than 2 antennas.")
      .range(0, 1);
  b.option("--i_2",
           test_params.i_2,
           "Precoder Matrix codebook index \"i_2\" to be forwarded to test UE, in the case of more than 2 antennas.")
      .range(0, 3);
}

static void declare_test_mode_args(config::config_builder& b, du_high_unit_test_mode_config& test_params)
{
  b.group("test_ue", "automatically created UE for testing purposes",
          [&](config::config_builder& sub) { declare_test_ue_mode_args(sub, test_params.test_ue); });
}

static void declare_pcap_args(config::config_builder& b, du_high_unit_pcap_config& pcap_params)
{
  b.option("--f1ap_filename", pcap_params.f1ap.filename, "F1AP PCAP file output path");
  b.option("--f1ap_enable", pcap_params.f1ap.enabled, "Enable F1AP packet capture");
  b.option("--f1u_filename", pcap_params.f1u.filename, "F1-U PCAP file output path");
  b.option("--f1u_enable", pcap_params.f1u.enabled, "Enable F1-U packet capture");
  b.option("--rlc_filename", pcap_params.rlc.filename, "RLC PCAP file output path");
  b.option("--rlc_rb_type", pcap_params.rlc.rb_type, "RLC PCAP RB type (all, srb, drb)");
  b.option("--rlc_enable", pcap_params.rlc.enabled, "Enable RLC packet capture");
  b.option("--mac_filename", pcap_params.mac.filename, "MAC PCAP file output path");
  b.option("--mac_type", pcap_params.mac.type, "MAC PCAP pcap type (dlt or udp)");
  b.option("--mac_enable", pcap_params.mac.enabled, "Enable MAC packet capture");
}

static void declare_rlc_am_args(config::config_builder& b, du_high_unit_rlc_am_config& rlc_am_params)
{
  b.group("tx", "AM TX parameters", [&](config::config_builder& tx) {
    tx.option("--sn", rlc_am_params.tx.sn_field_length, "RLC AM TX SN size");
    tx.option("--t-poll-retransmit", rlc_am_params.tx.t_poll_retx, "RLC AM TX t-PollRetransmit (ms)");
    tx.option("--max-retx-threshold", rlc_am_params.tx.max_retx_thresh, "RLC AM max retx threshold");
    tx.option("--poll-pdu", rlc_am_params.tx.poll_pdu, "RLC AM TX PollPdu");
    tx.option("--poll-byte", rlc_am_params.tx.poll_byte, "RLC AM TX PollByte");
    tx.option("--max_window",
              rlc_am_params.tx.max_window,
              "Non-standard parameter that limits the tx window size. Can be used for limiting memory usage with "
              "large windows. 0 means no limits other than the SN size (i.e. 2^[sn_size-1]).");
    tx.option("--queue-size", rlc_am_params.tx.queue_size, "RLC AM TX SDU queue size in PDUs");
    tx.option("--queue-bytes", rlc_am_params.tx.queue_size_bytes, "RLC AM TX SDU queue size in bytes");
  });
  b.group("rx", "AM RX parameters", [&](config::config_builder& rx) {
    rx.option("--sn", rlc_am_params.rx.sn_field_length, "RLC AM RX SN");
    rx.option("--t-reassembly", rlc_am_params.rx.t_reassembly, "RLC AM RX t-Reassembly");
    rx.option("--t-status-prohibit", rlc_am_params.rx.t_status_prohibit, "RLC AM RX t-StatusProhibit");
    rx.option("--max_sn_per_status", rlc_am_params.rx.max_sn_per_status, "RLC AM RX status SN limit");
  });
}

static void declare_srb_args(config::config_builder& b, du_high_unit_srb_config& srb_params)
{
  b.option("--srb_id", srb_params.srb_id, "SRB Id").note("legal values: {1, 2}");
  b.group("rlc", "RLC parameters",
          [&](config::config_builder& sub) { declare_rlc_am_args(sub, srb_params.rlc); });
  // Legacy CLI11 had `app.needs(rlc_subcmd)` requiring the rlc subcommand. This cross-subcommand constraint
  // belongs in a runtime validator. (TODO)
}

static void declare_metrics_layers_args(config::config_builder& b, du_high_unit_metrics_layer_config& metrics_params)
{
  b.option("--enable_sched", metrics_params.enable_scheduler, "Enable DU scheduler metrics");
  b.option("--enable_rlc", metrics_params.enable_rlc, "Enable RLC metrics");
  b.option("--enable_mac", metrics_params.enable_mac, "Enable MAC metrics");
  b.option("--enable_du_proc", metrics_params.enable_du_proc, "Enable DU management and control procedure metrics");
}

static void declare_metrics_args(config::config_builder& b, du_high_unit_metrics_config& metrics_params)
{
  b.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& sub) {
    sub.option("--du_report_period",
               metrics_params.du_report_period,
               "DU statistics report period in milliseconds")
        .range(0, static_cast<int>(NOF_SUBFRAMES_PER_FRAME * NOF_SFNS * NOF_HYPER_SFNS));
  });
  b.group("layers", "Layer basis metrics configuration",
          [&](config::config_builder& sub) { declare_metrics_layers_args(sub, metrics_params.layers_cfg); });
}

static void declare_rlc_um_args(config::config_builder& b, du_high_unit_rlc_um_config& rlc_um_params)
{
  b.group("tx", "UM TX parameters", [&](config::config_builder& tx) {
    tx.option("--sn", rlc_um_params.tx.sn_field_length, "RLC UM TX SN");
    tx.option("--queue-size", rlc_um_params.tx.queue_size, "RLC UM TX SDU queue limit in PDUs");
    tx.option("--queue-bytes", rlc_um_params.tx.queue_size_bytes, "RLC UM TX SDU queue limit in bytes");
  });
  b.group("rx", "UM TX parameters", [&](config::config_builder& rx) {
    rx.option("--sn", rlc_um_params.rx.sn_field_length, "RLC UM RX SN");
    rx.option("--t-reassembly", rlc_um_params.rx.t_reassembly, "RLC UM t-Reassembly");
  });
}

static void declare_rlc_args(config::config_builder& b, du_high_unit_rlc_config& rlc_params)
{
  b.option("--mode", rlc_params.mode, "RLC mode");
  b.group("um-bidir", "UM parameters",
          [&](config::config_builder& sub) { declare_rlc_um_args(sub, rlc_params.um); });
  b.group("am", "AM parameters",
          [&](config::config_builder& sub) { declare_rlc_am_args(sub, rlc_params.am); });
}

static void declare_f1u_du_args(config::config_builder& b, du_high_unit_f1u_du_config& f1u_du_params)
{
  b.option("--backoff_timer", f1u_du_params.t_notify, "F1-U backoff timer (ms)");
  b.option("--ul_buffer_size", f1u_du_params.ul_buffer_size, "F1-U handover buffer size");
}

static void declare_qos_args(config::config_builder& b, du_high_unit_qos_config& qos_params)
{
  b.option("--five_qi", qos_params.five_qi, "5QI").range(0, 255);
  b.group("rlc", "RLC parameters",
          [&](config::config_builder& sub) { declare_rlc_args(sub, qos_params.rlc); });
  b.group("f1u_du", "F1-U parameters at DU side",
          [&](config::config_builder& sub) { declare_f1u_du_args(sub, qos_params.f1u_du); });
  // Legacy CLI11 had `app.needs(rlc_subcmd)` and `app.needs(f1u_du_subcmd)`. These cross-subcommand
  // constraints belong in a runtime validator. (TODO)
}

// ===========================================================================
// Public entry points.
// ===========================================================================

void ocudu::configure_cli11_with_du_high_config_schema(config::config_builder& b, du_high_parsed_config& parsed_cfg)
{
  b.option("--gnb_id", parsed_cfg.config.gnb_id.id, "gNodeB identifier");
  b.option("--gnb_id_bit_length", parsed_cfg.config.gnb_id.bit_length, "gNodeB identifier length in bits")
      .range(22, 32);
  b.option("--gnb_du_id", parsed_cfg.config.gnb_du_id, "gNB-DU Id")
      .range(0.0, static_cast<double>((uint64_t(1) << 36) - 1));

  // Loggers section.
  b.group("log", "Logging configuration",
          [&](config::config_builder& sub) { declare_log_args(sub, parsed_cfg.config.loggers); });

  // Trace section.
  b.group("trace", "General tracer configuration",
          [&](config::config_builder& sub) { declare_trace_args(sub, parsed_cfg.config.tracer); });

  // Metrics section.
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& sub) { declare_metrics_args(sub, parsed_cfg.config.metrics); });
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, parsed_cfg.config.metrics.common_metrics_cfg);

  // PCAP section.
  b.group("pcap", "PCAP configuration",
          [&](config::config_builder& sub) { declare_pcap_args(sub, parsed_cfg.config.pcaps); });

  // Common cell section.
  // Note: the legacy CLI11 binding installed a parse_complete_callback that copied the common cell defaults into
  // every entry of --cells, and then re-ran the cells callback. The builder API has no parse-complete hook yet, so
  // this propagation must be re-done in autoderive_du_high_parameters_after_parsing. (TODO)
  b.group("cell_cfg", "Default cell configuration",
          [&](config::config_builder& sub) { declare_common_cell_args(sub, parsed_cfg.common_cell_cfg); });

  // DU section.
  b.group("du", "DU parameters",
          [&](config::config_builder& sub) { declare_du_args(sub, parsed_cfg.config.warn_on_drop); });

  // Expert execution section.
  b.group("expert_execution", "Expert execution configuration",
          [&](config::config_builder& sub) { declare_expert_execution_args(sub, parsed_cfg.config.expert_execution_cfg); });

  // Cell section.
  b.array_of("--cells",
             parsed_cfg.config.cells_cfg,
             "Sets the cell configuration on a per cell basis, overwriting the default configuration defined by "
             "cell_cfg",
             [](config::config_builder& el, du_high_unit_cell_config& cell) { declare_cells_args(el, cell); });

  // QoS section.
  b.array_of("--qos",
             parsed_cfg.config.qos_cfg,
             "Configures RLC and PDCP radio bearers on a per 5QI basis.",
             [](config::config_builder& el, du_high_unit_qos_config& qos) { declare_qos_args(el, qos); })
      .key("five_qi");

  // SRB section.
  // The legacy CLI11 binding parsed each SRB into a temporary du_high_unit_srb_config and inserted it into the
  // std::map keyed by srb_id. The builder's array_of binds against a Container::value_type, so the map target is
  // bridged through a side vector that the post-parse step folds back into the map.
  auto& srb_vector_buffer = get_srb_buffer();
  srb_vector_buffer.clear();
  for (const auto& [id, cfg] : parsed_cfg.config.srb_cfg) {
    srb_vector_buffer.push_back(cfg);
  }
  b.array_of("--srbs",
             srb_vector_buffer,
             "Configures signaling radio bearers.",
             [](config::config_builder& el, du_high_unit_srb_config& srb) { declare_srb_args(el, srb); });
  // The fold from srb_vector_buffer back into parsed_cfg.config.srb_cfg runs in
  // autoderive_du_high_parameters_after_parsing(). (TODO: when the builder gains map-target support, drop the
  // side-buffer and bind directly to the std::map.)

  // Test mode section.
  b.group("test_mode", "Test mode configuration",
          [&](config::config_builder& sub) { declare_test_mode_args(sub, parsed_cfg.config.test_mode_cfg); });
}

void ocudu::configure_cli11_with_du_high_config_schema(CLI::App& app, du_high_parsed_config& parsed_cfg)
{
  config::schema_node    discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_du_high_config_schema(b, parsed_cfg);
}

// ===========================================================================
// Autoderive (post-parse) helpers.
// ===========================================================================

// Derive the parameters set to "auto"-derived for a cell.
static void derive_cell_auto_params(du_high_unit_base_cell_config& cell_cfg)
{
  // If NR band is not set, derive a valid one from the DL-ARFCN.
  if (not cell_cfg.band.has_value()) {
    cell_cfg.band = band_helper::get_band_from_dl_arfcn(cell_cfg.dl_f_ref_arfcn);
  }
  if (not cell_cfg.scheduler_cfg.policy_cfg.has_value()) {
    cell_cfg.scheduler_cfg.policy_cfg.emplace(time_qos_scheduler_config{});
  }

  // If in TDD mode, and pattern was not set, generate a pattern DDDDDDXUUU.
  const duplex_mode dplx_mode = band_helper::get_duplex_mode(cell_cfg.band.value());
  if (dplx_mode == duplex_mode::TDD and not cell_cfg.tdd_ul_dl_cfg.has_value()) {
    cell_cfg.tdd_ul_dl_cfg.emplace();
    cell_cfg.tdd_ul_dl_cfg->pattern1.dl_ul_period_slots = 10;
    cell_cfg.tdd_ul_dl_cfg->pattern1.nof_dl_slots       = 6;
    cell_cfg.tdd_ul_dl_cfg->pattern1.nof_dl_symbols     = 8;
    cell_cfg.tdd_ul_dl_cfg->pattern1.nof_ul_slots       = 3;
    cell_cfg.tdd_ul_dl_cfg->pattern1.nof_ul_symbols     = 0;
  }

  // If PRACH configuration Index not set, a default one is assigned.
  if (not cell_cfg.prach_cfg.prach_config_index.has_value()) {
    if (band_helper::get_duplex_mode(cell_cfg.band.value()) == duplex_mode::FDD) {
      cell_cfg.prach_cfg.prach_config_index = 16;
    } else if (band_helper::get_freq_range(cell_cfg.band.value()) == frequency_range::FR1) {
      cell_cfg.prach_cfg.prach_config_index = 159;
    } else {
      cell_cfg.prach_cfg.prach_config_index = 112;
    }
  }

  // If PRACH RA Response Window not set, a default one is assigned.
  if (not cell_cfg.prach_cfg.ra_resp_window.has_value()) {
    cell_cfg.prach_cfg.ra_resp_window = 10U << to_numerology_value(cell_cfg.common_scs);
  }
}

static void derive_auto_params(du_high_unit_config& config)
{
  unsigned next_sector_id = 0;
  for (auto& cell : config.cells_cfg) {
    if (not cell.cell.sector_id.has_value()) {
      cell.cell.sector_id = next_sector_id;
      next_sector_id++;
    } else {
      next_sector_id = std::max(next_sector_id, cell.cell.sector_id.value() + 1);
    }
    derive_cell_auto_params(cell.cell);
  }
}

void ocudu::autoderive_du_high_parameters_after_parsing(CLI::App& /*app*/, du_high_unit_config& unit_cfg)
{
  // Fold the --srbs side-buffer into the srb_cfg map keyed by srb_id.
  auto& srb_vector_buffer = get_srb_buffer();
  for (auto& srb : srb_vector_buffer) {
    unit_cfg.srb_cfg[static_cast<srb_id_t>(srb.srb_id)] = srb;
  }
  srb_vector_buffer.clear();

  derive_auto_params(unit_cfg);
}
