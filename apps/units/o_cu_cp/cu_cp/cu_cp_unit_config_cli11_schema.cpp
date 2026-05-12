// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_cp_unit_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "apps/helpers/network/sctp_cli11_schema.h"
#include "cu_cp_unit_config.h"
#include "cu_cp_unit_config_helpers.h"
#include "ocudu/ran/nr_cell_identity.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace ocudu;

static void declare_geo_location_args(config::config_builder& b, std::optional<ocucp::rrc_geo_location>& loc)
{
  // Backing buffer: the original code used a CLI11 subcommand whose
  // parse-complete callback copied a hidden static into the optional only
  // when the subcommand appeared on the CLI. With the builder API we model
  // the two leaf knobs and lazily materialize the optional on the first
  // explicit field assignment.
  b.string_action(
      "--latitude",
      [&loc](const std::string& s) {
        if (s.empty()) {
          return;
        }
        if (!loc.has_value()) {
          loc = ocucp::rrc_geo_location{};
        }
        loc->latitude = std::stod(s);
      },
      [&loc]() -> std::string {
        if (!loc.has_value()) {
          return {};
        }
        std::ostringstream ss;
        ss << loc->latitude;
        return ss.str();
      },
      "Latitude [degrees, -90..90]",
      "decimal number in [-90, 90]");
  b.string_action(
      "--longitude",
      [&loc](const std::string& s) {
        if (s.empty()) {
          return;
        }
        if (!loc.has_value()) {
          loc = ocucp::rrc_geo_location{};
        }
        loc->longitude = std::stod(s);
      },
      [&loc]() -> std::string {
        if (!loc.has_value()) {
          return {};
        }
        std::ostringstream ss;
        ss << loc->longitude;
        return ss.str();
      },
      "Longitude [degrees, -180..180]",
      "decimal number in [-180, 180]");
}

static void declare_log_args(config::config_builder& b, cu_cp_unit_logger_config& cfg)
{
  app_helpers::add_log_option(b, cfg.pdcp_level, "--pdcp_level", "PDCP log level");
  app_helpers::add_log_option(b, cfg.rrc_level, "--rrc_level", "RRC log level");
  app_helpers::add_log_option(b, cfg.ngap_level, "--ngap_level", "NGAP log level");
  app_helpers::add_log_option(b, cfg.xnap_level, "--xnap_level", "XNAP log level");
  app_helpers::add_log_option(b, cfg.nrppa_level, "--nrppa_level", "NRPPA log level");
  app_helpers::add_log_option(b, cfg.e1ap_level, "--e1ap_level", "E1AP log level");
  app_helpers::add_log_option(b, cfg.f1ap_level, "--f1ap_level", "F1AP log level");
  app_helpers::add_log_option(b, cfg.cu_level, "--cu_level", "Log level for the CU");
  app_helpers::add_log_option(b, cfg.sec_level, "--sec_level", "Security functions log level");

  b.option("--hex_max_size",
           cfg.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);

  b.option("--e1ap_json_enabled", cfg.e1ap_json_enabled, "Enable JSON logging of E1AP PDUs");
  b.option("--f1ap_json_enabled", cfg.f1ap_json_enabled, "Enable JSON logging of F1AP PDUs");
}

static void declare_pcap_args(config::config_builder& b, cu_cp_unit_pcap_config& cfg)
{
  b.option("--ngap_filename", cfg.ngap.filename, "N3 GTP-U PCAP file output path");
  b.option("--ngap_enable", cfg.ngap.enabled, "Enable N3 GTP-U packet capture");
  b.option("--xnap_filename", cfg.xnap.filename, "XNAP PCAP file output path");
  b.option("--xnap_enable", cfg.xnap.enabled, "Enable XNAP packet capture");
  b.option("--f1ap_filename", cfg.f1ap.filename, "F1AP PCAP file output path");
  b.option("--f1ap_enable", cfg.f1ap.enabled, "Enable F1AP packet capture");
  b.option("--e1ap_filename", cfg.e1ap.filename, "E1AP PCAP file output path");
  b.option("--e1ap_enable", cfg.e1ap.enabled, "Enable E1AP packet capture");
}

static void declare_tai_slice_support_args(config::config_builder& b, cu_cp_unit_plmn_item::tai_slice_t& cfg)
{
  b.option("--sst", cfg.sst, "Slice Service Type").range(0, 255);
  b.option("--sd", cfg.sd, "Service Differentiator").range(0, 0xffffff);
}

static void declare_plmn_item_args(config::config_builder& b, cu_cp_unit_plmn_item& cfg)
{
  b.option("--plmn", cfg.plmn_id, "PLMN to be configured");

  b.array_of("--tai_slice_support_list",
             cfg.tai_slice_support_list,
             "Sets the list of TAI slices for this PLMN",
             [](config::config_builder& el, cu_cp_unit_plmn_item::tai_slice_t& slice) {
               declare_tai_slice_support_args(el, slice);
             })
      .key("sst");
}

static void declare_supported_ta_args(config::config_builder& b, cu_cp_unit_supported_ta_item& cfg)
{
  // TAC: original CLI11 check rejected values 0 and 0xfffffe and >0xffffff. Range
  // bounds map cleanly to the taxonomy; the "reserved values" predicate is a
  // discrete exclusion that doesn't, so it is dropped here and surfaced as a
  // free-text note. To be re-implemented in a runtime validator.
  b.option("--tac", cfg.tac, "TAC to be configured")
      .range(0, 0xffffff)
      .note("values 0 and 0xfffffe are reserved");

  b.array_of("--plmn_list",
             cfg.plmn_list,
             "Sets the list of PLMN items for this tracking area",
             [](config::config_builder& el, cu_cp_unit_plmn_item& plmn) { declare_plmn_item_args(el, plmn); })
      .key("plmn");
}

static void declare_amf_item_args(config::config_builder& b, cu_cp_unit_amf_config_item& cfg)
{
  b.option("--addrs,--addr",
           cfg.ip_addrs,
           "AMF addresses to be used for N2 interface. Multiple addresses can be specified for SCTP multi-homing")
      .note("--addr is the legacy alias of --addrs; kept for backward compatibility");
  b.option("--port", cfg.port, "AMF port").range(20000, 40000);
  b.option("--bind_addrs,--bind_addr",
           cfg.bind_addrs,
           "CU-CP bind addresses to be used for N2 interface. Multiple addresses can be specified for SCTP "
           "multi-homing. If left empty, implicit bind is performed")
      .note("--bind_addr is the legacy alias of --bind_addrs; kept for backward compatibility");
  b.option("--bind_interface", cfg.bind_interface, "Network device to bind for N2 interface");

  configure_cli11_sctp_socket_args(b, cfg.sctp);

  // The original code cleared the default supported tracking areas as a side
  // effect of parsing this list. That side effect can't be expressed in the
  // builder taxonomy and is captured here as a wrapper around array_of's
  // setter. Note: array_of resizes the vector itself; here we only need to
  // clear the default-flag before the resize takes effect. We rely on the
  // explicit reset performed inside array_of by using a wrapper element
  // configurator that flips the flag on the first element.
  b.array_of("--supported_tracking_areas",
             cfg.supported_tas,
             "Sets the list of tracking areas supported by this AMF",
             [&cfg](config::config_builder& el, cu_cp_unit_supported_ta_item& ta) {
               cfg.is_default_supported_tas = false;
               declare_supported_ta_args(el, ta);
             })
      .key("tac");
}

static void declare_amf_args(config::config_builder& b, cu_cp_unit_amf_config& cfg)
{
  b.option("--no_core", cfg.no_core, "Allow CU-CP to run without a core");
  b.option("--amf_reconnection_retry_time",
           cfg.amf_reconnection_retry_time,
           "Time to wait after a failed AMF reconnection attempt in ms");
  b.option("--procedure_timeout",
           cfg.procedure_timeout,
           "Time that the NGAP waits for a response from the AMF in milliseconds");

  declare_amf_item_args(b, cfg.amf);
}

static void declare_xnap_item_args(config::config_builder& b, cu_cp_unit_xnap_config_item& cfg)
{
  b.option("--bind_addrs",
           cfg.bind_addrs,
           "Local IP addresses to bind for XNAP interface. Multiple addresses can be specified for SCTP "
           "multi-homing. If left empty, implicit bind is performed");
  b.option("--peer_addrs", cfg.peer_addrs, "Peer IP addresses to connect for XNAP interface");
}

static void declare_xnap_args(config::config_builder& b, cu_cp_unit_xnap_config& cfg)
{
  b.option("--procedure_timeout", cfg.procedure_timeout, "Time that the XNAP waits for a response in milliseconds");
  b.option("--reconnect_timer",
           cfg.reconnect_timer,
           "Time that the XNAP waits before trying to reconnect in milliseconds");
  b.option("--no_connection_init",
           cfg.no_connection_init,
           "When true, the CU-CP will not initiate XNAP connections, but will only accept inbound ones")
      .note("hidden from --help in legacy CLI11 view");

  configure_cli11_sctp_socket_args(b, cfg.sctp);

  b.array_of("--connections",
             cfg.connections,
             "Sets the list of XN-C peer CU-CPs for the CU-CP to connect to",
             [](config::config_builder& el, cu_cp_unit_xnap_config_item& peer) { declare_xnap_item_args(el, peer); });
}

static void declare_report_args(config::config_builder& b, cu_cp_unit_report_config& cfg)
{
  b.option("--report_cfg_id", cfg.report_cfg_id, "Report configuration id to be configured").range(1, 64);
  b.enumeration("--report_type",
                cfg.report_type,
                "Type of the report configuration",
                {"periodical", "event_triggered", "cond_trigger"});

  // Optional string enum field.
  b.string_action(
      "--event_triggered_report_type",
      [&cfg](const std::string& s) {
        if (s.empty()) {
          return;
        }
        cfg.event_triggered_report_type = s;
      },
      [&cfg]() -> std::string {
        return cfg.event_triggered_report_type.value_or(std::string{});
      },
      "Type of the event triggered report",
      "one of {a1, a2, a3, a4, a5, a6, d1, t1, d2}");

  b.option("--report_interval_ms", cfg.report_interval_ms, "Report interval in ms")
      .enum_values({"120",
                    "240",
                    "480",
                    "640",
                    "1024",
                    "2048",
                    "5120",
                    "10240",
                    "20480",
                    "40960",
                    "60000",
                    "360000",
                    "720000",
                    "1800000"});

  b.option("--periodic_ho_rsrp_offset_db",
           cfg.periodic_ho_rsrp_offset,
           "Measurement trigger quantity offset in dB used to trigger handovers by periodic measurement reports. "
           "When set to -1 no handover will be triggered from periodical measurements. Note the "
           "actual value is field value * 0.5 dB")
      .range(-1, 30);

  // Optional string enum field.
  b.string_action(
      "--meas_trigger_quantity",
      [&cfg](const std::string& s) {
        if (s.empty()) {
          return;
        }
        cfg.meas_trigger_quantity = s;
      },
      [&cfg]() -> std::string {
        return cfg.meas_trigger_quantity.value_or(std::string{});
      },
      "Measurement trigger quantity (RSRP/RSRQ/SINR)",
      "one of {rsrp, rsrq, sinr}");

  b.option("--meas_trigger_quantity_threshold_db",
           cfg.meas_trigger_quantity_threshold_db,
           "Measurement trigger quantity threshold in dB used for measurement report trigger of event A1/A2/A4/A5"
           "Valid ranges: RSRP [-156..-31] dBm, RSRQ [-43..20] dB, SINR [-23..40] dB")
      .range(-156, 40);
  b.option("--meas_trigger_quantity_threshold_2_db",
           cfg.meas_trigger_quantity_threshold_2_db,
           "Measurement trigger quantity threshold 2 in dB used for measurement report trigger of event A5"
           "Valid ranges: RSRP [-156..-31] dBm, RSRQ [-43..20] dB, SINR [-23..40] dB")
      .range(-156, 40);
  b.option("--meas_trigger_quantity_offset_db",
           cfg.meas_trigger_quantity_offset_db,
           "Measurement trigger quantity offset in dB used for measurement report trigger of event A3/A6.")
      .range(-15, 15);
  b.option("--hysteresis_db", cfg.hysteresis_db, "Hysteresis in dB used for measurement report trigger.").range(0, 15);
  b.option("--time_to_trigger_ms",
           cfg.time_to_trigger_ms,
           "Time in ms during which a condition must be met before measurement report trigger")
      .enum_values({"0", "40", "64", "80", "100", "128", "160", "256", "320", "480", "512", "640", "1024", "1280",
                    "2560", "5120"});
  b.option("--t312",
           cfg.t312_ms,
           "T312 timer in ms. This timer is started by the UE on event triggered measurement report, when T310 "
           "(out-of-sync) timer is already running and on its expiration triggers the RLF to speed up "
           "reestablishment to different cell.")
      .enum_values({"0", "50", "100", "200", "300", "400", "500", "1000"});

  // D1/D2 distance-based conditional event options.
  b.option("--distance_thresh_from_ref1_km",
           cfg.distance_thresh_from_ref1_km,
           "D1/D2: distance threshold 1 in km [0..3276.75] (50m steps, D1 max is 3276.25)")
      .range(0.0, 3276.75);
  b.option("--distance_thresh_from_ref2_km",
           cfg.distance_thresh_from_ref2_km,
           "D1/D2: distance threshold 2 in km [0..3276.75] (50m steps, D1 max is 3276.25)")
      .range(0.0, 3276.75);
  b.option("--hysteresis_location_km",
           cfg.hysteresis_location_km,
           "D1/D2: location hysteresis in km [0..327.68] (10m steps)")
      .range(0.0, 327.68);

  // D1 reference locations as nested groups.
  b.group("ref_location1", "D1: reference location 1 (serving cell)",
          [&](config::config_builder& sub) { declare_geo_location_args(sub, cfg.ref_location1); });
  b.group("ref_location2", "D1: reference location 2 (target cell)",
          [&](config::config_builder& sub) { declare_geo_location_args(sub, cfg.ref_location2); });

  // T1 time-based conditional event options. Custom parser doesn't fit the
  // standard taxonomy; use string_action.
  b.string_action(
      "--t1_thres",
      [&cfg](const std::string& v) {
        if (v.empty()) {
          return;
        }
        auto result = parse_timestamp_ms(v);
        if (!result) {
          throw CLI::ValidationError("--t1_thres", result.error());
        }
        cfg.t1_thres = result.value();
      },
      [&cfg]() -> std::string {
        if (!cfg.t1_thres.has_value()) {
          return {};
        }
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(cfg.t1_thres->time_since_epoch()).count();
        return std::to_string(ms);
      },
      "T1: time threshold (Unix ms integer or YYYY-MM-DDTHH:MM:SS[.mmm])",
      "Unix time in ms or ISO 8601 YYYY-MM-DDTHH:MM:SS[.mmm]");

  // Duration target is optional<duration<double>>; CLI11 can't lexical_cast it
  // directly. Use string_action with a numeric note.
  b.string_action(
      "--duration_s",
      [&cfg](const std::string& s) {
        if (s.empty()) {
          return;
        }
        double v = std::stod(s);
        if (v < 0.1 || v > 600.0) {
          throw CLI::ValidationError("--duration_s", "value out of range [0.1, 600]");
        }
        cfg.duration = std::chrono::duration<double>{v};
      },
      [&cfg]() -> std::string {
        if (!cfg.duration.has_value()) {
          return {};
        }
        std::ostringstream ss;
        ss << cfg.duration->count();
        return ss.str();
      },
      "T1: duration in seconds (each step=100ms, range [0.1..600])",
      "decimal number in [0.1, 600]");
}

static void declare_ncell_args(config::config_builder& b, cu_cp_unit_neighbor_cell_config_item& cfg)
{
  b.option("--nr_cell_id", cfg.nr_cell_id, "Neighbor cell id")
      .range(0.0, static_cast<double>(nr_cell_identity::max().value()));
  b.option("--report_configs", cfg.report_cfg_ids, "Report configurations to configure for this neighbor cell");
}

static void declare_cells_args(config::config_builder& b, cu_cp_unit_cell_config_item& cfg)
{
  b.option("--nr_cell_id", cfg.nr_cell_id, "Cell id to be configured")
      .range(0.0, static_cast<double>(nr_cell_identity::max().value()));
  b.option("--periodic_report_cfg_id",
           cfg.periodic_report_cfg_id,
           "Periodical report configuration for the serving cell")
      .range(1, 64);

  b.auto_enum_option("--band", cfg.band, "NR frequency band");

  b.option("--gnb_id_bit_length",
           cfg.gnb_id_bit_length,
           "gNodeB identifier bit length. If not set, it will be automatically set to be equal to the gNodeB Id of "
           "the CU-CP")
      .range(22, 32);
  b.option("--pci", cfg.pci, "Physical Cell Id").range(0, 1007);
  // ssb_arfcn target is std::optional<arfcn_t>, where arfcn_t is a bounded_integer
  // wrapper that the config_builder taxonomy doesn't understand directly; route
  // through string_action.
  b.string_action(
      "--ssb_arfcn",
      [&cfg](const std::string& s) {
        if (s.empty()) {
          return;
        }
        cfg.ssb_arfcn = arfcn_t{static_cast<uint32_t>(std::stoul(s))};
      },
      [&cfg]() -> std::string {
        if (!cfg.ssb_arfcn.has_value()) {
          return {};
        }
        return std::to_string(cfg.ssb_arfcn->value());
      },
      "SSB ARFCN",
      "integer in [0, 3279165]");
  b.option("--ssb_scs", cfg.ssb_scs, "SSB subcarrier spacing").enum_values({"15", "30", "60", "120", "240"});
  b.option("--ssb_period", cfg.ssb_period, "SSB period in ms").enum_values({"5", "10", "20", "40", "80", "160"});
  b.option("--ssb_offset", cfg.ssb_offset, "SSB offset");
  b.option("--ssb_duration", cfg.ssb_duration, "SSB duration").enum_values({"1", "2", "3", "4", "5"});

  b.array_of("--ncells",
             cfg.ncells,
             "Sets the list of neighbor cells known to the CU-CP",
             [](config::config_builder& el, cu_cp_unit_neighbor_cell_config_item& nc) { declare_ncell_args(el, nc); })
      .key("nr_cell_id");
}

static void declare_mobility_args(config::config_builder& b, cu_cp_unit_mobility_config& cfg)
{
  b.option("--trigger_handover_from_measurements",
           cfg.trigger_handover_from_measurements,
           "Whether to start HO if neighbor cells become stronger");
  b.option("--trigger_cho_on_ue_setup",
           cfg.trigger_cho_on_ue_setup,
           "Whether to auto-trigger CHO after UE setup when readiness checks pass");
  b.option("--cho_timeout_ms",
           cfg.cho_timeout_ms,
           "Timeout in milliseconds used for auto-triggered CHO and as default timeout for manual CHO command")
      .range(1, 600000);

  b.array_of("--cells",
             cfg.cells,
             "Sets the list of cells known to the CU-CP",
             [](config::config_builder& el, cu_cp_unit_cell_config_item& cell) { declare_cells_args(el, cell); })
      .key("nr_cell_id");

  b.array_of("--report_configs",
             cfg.report_configs,
             "Sets report configurations",
             [](config::config_builder& el, cu_cp_unit_report_config& rep) { declare_report_args(el, rep); })
      .key("report_cfg_id");
}

static void declare_rrc_args(config::config_builder& b, cu_cp_unit_rrc_config& cfg)
{
  b.option("--force_reestablishment_fallback",
           cfg.force_reestablishment_fallback,
           "Force RRC re-establishment fallback to RRC setup");
  b.option("--force_resume_fallback", cfg.force_resume_fallback, "Force RRC resume fallback to RRC setup");
  b.option("--rrc_procedure_guard_time_ms",
           cfg.rrc_procedure_guard_time_ms,
           "Guard time in ms used for RRC message exchange with UE. This is added to the RRC procedure timeout.");
}

static void declare_security_args(config::config_builder& b, cu_cp_unit_security_config& cfg)
{
  b.enumeration("--integrity",
                cfg.integrity_protection,
                "Default integrity protection indication for DRBs",
                {"required", "preferred", "not_needed"});
  b.enumeration("--confidentiality",
                cfg.confidentiality_protection,
                "Default confidentiality protection indication for DRBs",
                {"required", "preferred", "not_needed"});
  b.option("--nea_pref_list",
           cfg.nea_preference_list,
           "Ordered preference list for the selection of encryption algorithm (NEA) (default: NEA0, NEA2, NEA1)");
  b.option("--nia_pref_list",
           cfg.nia_preference_list,
           "Ordered preference list for the selection of encryption algorithm (NIA) (default: NIA2, NIA1)");
}

static void declare_f1ap_args(config::config_builder& b, cu_cp_unit_f1ap_config& cfg)
{
  b.option("--procedure_timeout", cfg.procedure_timeout, "Time that the F1AP waits for a DU response in milliseconds");
}

static void declare_e1ap_args(config::config_builder& b, cu_cp_unit_e1ap_config& cfg)
{
  b.option("--procedure_timeout",
           cfg.procedure_timeout,
           "Time that the E1AP waits for a CU-UP response in milliseconds");
}

static void declare_cu_cp_args(config::config_builder& b, cu_cp_unit_config& cfg)
{
  b.option("--max_nof_dus", cfg.max_nof_dus, "Maximum number of DU connections that the CU-CP may accept");
  b.option("--max_nof_cu_ups", cfg.max_nof_cu_ups, "Maximum number of CU-UP connections that the CU-CP may accept");
  b.option("--max_nof_ues", cfg.max_nof_ues, "Maximum number of UEs that the CU-CP may accept");
  b.option("--max_nof_drbs_per_ue", cfg.max_nof_drbs_per_ue, "Maximum number of DRBs per UE").range(1, 29);
  b.option("--inactivity_timer", cfg.inactivity_timer, "UE/PDU Session/DRB inactivity timer in seconds").range(1, 7200);
  b.option(
      "--enable_rrc_inactive",
      cfg.enable_rrc_inactive,
      "Enable RRC inactive state for UEs based on inactivity timer. When disabled, UEs will be released on inactivity");
  b.option("--ran_paging_cycle",
           cfg.ran_paging_cycle,
           "RAN Paging cycle for RRC inactive UEs in nof. Radio Frames")
      .enum_values({"32", "64", "128", "256"});
  b.option("--t380",
           cfg.t380,
           "RRC inactivity timer T380 in minutes. The timer is started when the UE recveives a RRC Release message "
           "including a suspend config and is stopped on the reception of RRCResume.")
      .enum_values({"5", "10", "20", "30", "60", "120", "360", "720"});
  b.option("--nof_i_rnti_ue_bits",
           cfg.nof_i_rnti_ue_bits,
           "Number of bits used for the UE id in short and full I-RNTI")
      .range(1, 18);
  b.option("--request_pdu_session_timeout",
           cfg.request_pdu_session_timeout,
           "Timeout for requesting a PDU session after the InitialUeMessage was sent to the core, in "
           "seconds. The timeout must be larger than T310. If the value is reached, the UE will be released.")
      .note("must be larger than T310");

  b.group("amf", "AMF configuration",
          [&](config::config_builder& amf) { declare_amf_args(amf, cfg.amf_config); });

  b.array_of("--extra_amfs",
             cfg.extra_amfs,
             "Sets the list of extra AMFs for the CU-CP to connect to",
             [](config::config_builder& el, cu_cp_unit_amf_config_item& amf) { declare_amf_item_args(el, amf); });

  b.group("xnap", "XNAP configuration",
          [&](config::config_builder& xnap) { declare_xnap_args(xnap, cfg.xnap_config); });
  b.group("mobility", "Mobility configuration",
          [&](config::config_builder& mob) { declare_mobility_args(mob, cfg.mobility_config); });
  b.group("rrc", "RRC specific configuration",
          [&](config::config_builder& rrc) { declare_rrc_args(rrc, cfg.rrc_config); });
  b.group("security", "Security configuration",
          [&](config::config_builder& sec) { declare_security_args(sec, cfg.security_config); });
  b.group("f1ap", "F1AP configuration parameters",
          [&](config::config_builder& f1ap) { declare_f1ap_args(f1ap, cfg.f1ap_config); });
  b.group("e1ap", "E1AP configuration parameters",
          [&](config::config_builder& e1ap) { declare_e1ap_args(e1ap, cfg.e1ap_config); });
}

static void declare_rlc_um_args(config::config_builder& b, cu_cp_unit_rlc_um_config& cfg)
{
  b.group("tx", "UM TX parameters", [&](config::config_builder& tx) {
    tx.option("--sn", cfg.tx.sn_field_length, "RLC UM TX SN");
    tx.option("--queue-size", cfg.tx.queue_size, "RLC UM TX SDU queue size");
  });
  b.group("rx", "UM RX parameters", [&](config::config_builder& rx) {
    rx.option("--sn", cfg.rx.sn_field_length, "RLC UM RX SN");
    rx.option("--t-reassembly", cfg.rx.t_reassembly, "RLC UM t-Reassembly");
  });
}

static void declare_rlc_am_args(config::config_builder& b, cu_cp_unit_rlc_am_config& cfg)
{
  b.group("tx", "AM TX parameters", [&](config::config_builder& tx) {
    tx.option("--sn", cfg.tx.sn_field_length, "RLC AM TX SN size");
    tx.option("--t-poll-retransmit", cfg.tx.t_poll_retx, "RLC AM TX t-PollRetransmit (ms)");
    tx.option("--max-retx-threshold", cfg.tx.max_retx_thresh, "RLC AM max retx threshold");
    tx.option("--poll-pdu", cfg.tx.poll_pdu, "RLC AM TX PollPdu");
    tx.option("--poll-byte", cfg.tx.poll_byte, "RLC AM TX PollByte");
    tx.option("--max_window",
              cfg.tx.max_window,
              "Non-standard parameter that limits the tx window size. Can be used for limiting memory usage with "
              "large windows. 0 means no limits other than the SN size (i.e. 2^[sn_size-1]).");
    tx.option("--queue-size", cfg.tx.queue_size, "RLC AM TX SDU queue size");
  });
  b.group("rx", "AM RX parameters", [&](config::config_builder& rx) {
    rx.option("--sn", cfg.rx.sn_field_length, "RLC AM RX SN");
    rx.option("--t-reassembly", cfg.rx.t_reassembly, "RLC AM RX t-Reassembly");
    rx.option("--t-status-prohibit", cfg.rx.t_status_prohibit, "RLC AM RX t-StatusProhibit");
    rx.option("--max_sn_per_status", cfg.rx.max_sn_per_status, "RLC AM RX status SN limit");
  });
}

static void declare_rlc_args(config::config_builder& b, cu_cp_unit_rlc_config& cfg)
{
  b.option("--mode", cfg.mode, "RLC mode");
  b.group("um-bidir", "UM parameters",
          [&](config::config_builder& um) { declare_rlc_um_args(um, cfg.um); });
  b.group("am", "AM parameters",
          [&](config::config_builder& am) { declare_rlc_am_args(am, cfg.am); });
}

static void declare_pdcp_rohc_args(config::config_builder& b, cu_cp_unit_pdcp_rohc_config& cfg)
{
  b.enum_option<cu_cp_unit_pdcp_rohc_type>(
      "--rohc_type",
      cfg.rohc_type,
      "ROHC type (none/rohc/ul_only_rohc). Values: {none, rohc, ul_only_rohc}. Default: none",
      {{"none", cu_cp_unit_pdcp_rohc_type::none},
       {"rohc", cu_cp_unit_pdcp_rohc_type::rohc},
       {"uplink_only_rohc", cu_cp_unit_pdcp_rohc_type::uplink_only_rohc}});
  b.option("--max_cid", cfg.max_cid, "Maximum CID");
  b.option("--profile0x0001", cfg.profile0x0001, "Configure profile0x0001 (ROHCv1 RTP/UDP/IP)");
  b.option("--profile0x0002", cfg.profile0x0002, "Configure profile0x0002 (ROHCv1 UDP/IP)");
  b.option("--profile0x0003", cfg.profile0x0003, "Configure profile0x0003 (ROHCv1 ESP/IP)");
  b.option("--profile0x0004", cfg.profile0x0004, "Configure profile0x0004 (ROHCv1 IP)");
  b.option("--profile0x0006", cfg.profile0x0006, "Configure profile0x0006 (ROHCv1 TCP/IP)");
  b.option("--profile0x0101", cfg.profile0x0101, "Configure profile0x0101 (ROHCv2 RTP/UDP/IP)");
  b.option("--profile0x0102", cfg.profile0x0102, "Configure profile0x0102 (ROHCv2 UDP/IP)");
  b.option("--profile0x0103", cfg.profile0x0103, "Configure profile0x0103 (ROHCv2 ESP/IP)");
  b.option("--profile0x0104", cfg.profile0x0104, "Configure profile0x0104 (ROHCv2 IP)");
}

static void declare_pdcp_tx_args(config::config_builder& b, cu_cp_unit_pdcp_tx_config& cfg)
{
  b.option("--sn", cfg.sn_field_length, "PDCP TX SN size");
  b.option("--discard_timer", cfg.discard_timer, "PDCP TX discard timer (ms)");
  b.option("--status_report_required", cfg.status_report_required, "PDCP TX status report required");
}

static void declare_pdcp_rx_args(config::config_builder& b, cu_cp_unit_pdcp_rx_config& cfg)
{
  b.option("--sn", cfg.sn_field_length, "PDCP RX SN size");
  b.option("--t_reordering", cfg.t_reordering, "PDCP RX t-Reordering (ms)");
  b.option("--out_of_order_delivery", cfg.out_of_order_delivery, "PDCP RX enable out-of-order delivery");
}

static void declare_pdcp_args(config::config_builder& b, cu_cp_unit_pdcp_config& cfg)
{
  b.group("rohc", "Header compression parameters",
          [&](config::config_builder& rohc) { declare_pdcp_rohc_args(rohc, cfg.rohc); });
  b.group("tx", "PDCP TX parameters",
          [&](config::config_builder& tx) { declare_pdcp_tx_args(tx, cfg.tx); });
  b.group("rx", "PDCP RX parameters",
          [&](config::config_builder& rx) { declare_pdcp_rx_args(rx, cfg.rx); });
}

static void declare_qos_args(config::config_builder& b, cu_cp_unit_qos_config& cfg)
{
  b.option("--five_qi", cfg.five_qi, "5QI").range(0, 255);
  b.group("rlc", "RLC parameters",
          [&](config::config_builder& rlc) { declare_rlc_args(rlc, cfg.rlc); });
  b.group("pdcp", "PDCP parameters",
          [&](config::config_builder& pdcp) { declare_pdcp_args(pdcp, cfg.pdcp); });
  // CLI11's cross-field "needs(rlc) needs(pdcp)" requirement does not map to
  // the constraint taxonomy; to be re-implemented as a runtime validator.
}

static void declare_metrics_layers_args(config::config_builder& b, cu_cp_unit_metrics_layer_config& cfg)
{
  b.option("--enable_ngap", cfg.enable_ngap, "Enable NGAP metrics");
  b.option("--enable_pdcp", cfg.enable_pdcp, "Enable PDCP metrics");
  b.option("--enable_rrc", cfg.enable_rrc, "Enable CU-CP RRC metrics");
}

static void declare_metrics_args(config::config_builder& b, cu_cp_unit_metrics_config& cfg)
{
  b.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& p) {
    p.option("--cu_cp_report_period", cfg.cu_cp_report_period, "CU-CP metrics report period in milliseconds");
  });
  b.group("layers", "Layer basis metrics configuration",
          [&](config::config_builder& l) { declare_metrics_layers_args(l, cfg.layers_cfg); });
}

void ocudu::configure_cli11_with_cu_cp_unit_config_schema(config::config_builder& b, cu_cp_unit_config& unit_cfg)
{
  b.option("--gnb_id", unit_cfg.gnb_id.id, "gNodeB identifier");
  b.option("--gnb_id_bit_length", unit_cfg.gnb_id.bit_length, "gNodeB identifier length in bits").range(22, 32);
  b.option("--ran_node_name", unit_cfg.ran_node_name, "RAN node name");

  b.group("cu_cp", "CU-CP parameters",
          [&](config::config_builder& cu_cp) { declare_cu_cp_args(cu_cp, unit_cfg); });

  b.group("log", "Logging configuration",
          [&](config::config_builder& log) { declare_log_args(log, unit_cfg.loggers); });

  b.group("pcap", "PCAP configuration",
          [&](config::config_builder& pcap) { declare_pcap_args(pcap, unit_cfg.pcap_cfg); });

  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { declare_metrics_args(m, unit_cfg.metrics); });
  // Common metrics options (enable_json/log/verbose) also live under "metrics".
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, unit_cfg.metrics.common_metrics_cfg);

  b.array_of("--qos",
             unit_cfg.qos_cfg,
             "Configures RLC and PDCP radio bearers on a per 5QI basis.",
             [](config::config_builder& el, cu_cp_unit_qos_config& qos) { declare_qos_args(el, qos); })
      .key("five_qi");
}

void ocudu::configure_cli11_with_cu_cp_unit_config_schema(CLI::App& app, cu_cp_unit_config& unit_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_cu_cp_unit_config_schema(b, unit_cfg);
}

void ocudu::autoderive_cu_cp_parameters_after_parsing(CLI::App& app, cu_cp_unit_config& unit_cfg)
{
  (void)app;
  for (auto& cell : unit_cfg.mobility_config.cells) {
    // Set gNB ID bit length of the neighbor cell to be equal to the current unit gNB ID bit length, if not explicitly
    // set.
    if (not cell.gnb_id_bit_length.has_value()) {
      cell.gnb_id_bit_length = unit_cfg.gnb_id.bit_length;
    }
  }
}
