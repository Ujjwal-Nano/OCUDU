// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_up_unit_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "apps/helpers/network/udp_cli11_schema.h"
#include "apps/units/o_cu_up/cu_up/cu_up_unit_config.h"
#include "apps/units/o_cu_up/cu_up/cu_up_unit_pcap_config.h"
#include "ocudu/ran/cu_up_constants.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void declare_ngu_socket_args(config::config_builder& b, cu_up_unit_ngu_socket_config& cfg)
{
  b.option("--bind_addr", cfg.bind_addr, "Local IP address to bind for N3 interface")
      .note("must be a valid IPv4 address or \"auto\"");
  b.option("--bind_interface", cfg.bind_interface, "Network device to bind for N3 interface");
  b.option("--ext_addr",
           cfg.ext_addr,
           "External IP address that is advertised to receive GTP-U packets from UPF via N3 interface")
      .note("must be a valid IPv4 address or \"auto\"");
  configure_cli11_with_udp_config_schema(b, cfg.udp_config);
}

static void declare_ngu_gtpu_args(config::config_builder& b, cu_up_unit_ngu_gtpu_config& cfg)
{
  b.option("--queue_size", cfg.gtpu_queue_size, "GTP-U queue size, in PDUs");
  b.option("--batch_size", cfg.gtpu_batch_size, "Maximum number of GTP-U PDUs processed in a batch");
  b.option("--reordering_timer", cfg.gtpu_reordering_timer_ms, "GTP-U RX reordering timer (in milliseconds)");
  b.option("--rate_limiter_period", cfg.rate_limiter_period, "GTP-U RX rate limiter period (in milliseconds)");
  b.option("--teid_release_linger_time",
           cfg.gtpu_teid_release_linger_time,
           "Error indication suppression time for released TEIDs (in milliseconds)");
  b.option("--ignore_ue_ambr", cfg.ignore_ue_ambr, "Ignore GTP-U DL UE-AMBR rate limiter");
}

static void declare_execution_args(config::config_builder& b, cu_up_unit_execution_config& cfg)
{
  b.group("queues", "Task executor queue parameters", [&](config::config_builder& q) {
    q.option("--cu_up_dl_ue_executor_queue_size", cfg.dl_ue_executor_queue_size, "CU-UP's DL UE executor queue size");
    q.option("--cu_up_ul_ue_executor_queue_size", cfg.ul_ue_executor_queue_size, "CU-UP's UL UE executor queue size");
    q.option("--cu_up_ctrl_ue_executor_queue_size",
             cfg.ctrl_ue_executor_queue_size,
             "CU-UP's CTRL UE executor queue size");
    q.option("--cu_up_strand_batch_size", cfg.strand_batch_size, "CU-UP's strands batch size");
  });
}

static void declare_ngu_args(config::config_builder& b, cu_up_unit_ngu_config& cfg)
{
  b.option("--no_core", cfg.no_core, "Allow gNB to run without a core");

  b.group("gtpu", "CU-UP NG-U GTP-U parameters", [&](config::config_builder& g) { declare_ngu_gtpu_args(g, cfg.gtpu_cfg); });

  b.array_of("--socket",
             cfg.ngu_socket_cfg,
             "Configures UDP/IP socket parameters of the N3 interface",
             [](config::config_builder& el, cu_up_unit_ngu_socket_config& sock) { declare_ngu_socket_args(el, sock); });
}

static void declare_test_mode_args(config::config_builder& b, cu_up_unit_test_mode_config& cfg)
{
  b.option("--enable", cfg.enabled, "Enable or disable CU-UP test mode");
  b.option("--integrity_enable", cfg.integrity_enabled, "Enable or disable PDCP integrity testing");
  b.option("--ciphering_enable", cfg.ciphering_enabled, "Enable or disable PDCP ciphering testing");
  b.option("--nea_algo", cfg.nea_algo, "NEA algo to use for testing. Valid values {0, 1, 2, 3}.").range(0, 3);
  b.option("--nia_algo", cfg.nea_algo, "NIA algo to use for testing. Valid values {1, 2, 3}.").range(1, 3);
  b.option("--ue_ambr", cfg.ue_ambr, "DL UE-AMBR used for testing in bps");
  b.option("--attach_detach_period",
           cfg.attach_detach_period,
           "Attach/detach period for test mode. 0 means always attached");
  b.option("--reestablish_period", cfg.reestablish_period, "Reestablish period for test mode. 0 means always attached");
  b.option("--f1u_peer_address", cfg.f1u_peer_address, "Address for DL F1-U packets for test mode");
  b.option("--nof_ues", cfg.nof_ues, "Number of UEs used for test mode");
}

static void declare_cu_up_args(config::config_builder& b, cu_up_unit_config& cfg)
{
  b.group("ngu", "NG-U parameters", [&](config::config_builder& ngu) { declare_ngu_args(ngu, cfg.ngu_cfg); });
  b.group("test_mode", "CU-UP test mode parameters",
          [&](config::config_builder& tm) { declare_test_mode_args(tm, cfg.test_mode_cfg); });
  b.option("--warn_on_drop",
           cfg.warn_on_drop,
           "Log a warning for dropped packets in GTP-U, SDAP, PDCP and F1-U due to full queues");
  b.option("--max_nof_ues", cfg.max_nof_ues, "Maximum number of Bearer Contexts allowed by the CU-UP")
      .range(1, static_cast<int>(MAX_NOF_CU_UP_UES));
}

static void declare_log_args(config::config_builder& b, cu_up_unit_logger_config& cfg)
{
  app_helpers::add_log_option(b, cfg.pdcp_level, "--pdcp_level", "PDCP log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.sdap_level, "--sdap_level", "SDAP log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.gtpu_level, "--gtpu_level", "GTPU log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.e1ap_level, "--e1ap_level", "E1AP log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.f1u_level, "--f1u_level", "F1-U log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.cu_level, "--cu_level", "Log level for the CU").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.sec_level, "--sec_level", "Security functions log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.rohc_level, "--rohc_level", "ROHC log level").fallback_from("--all_level");

  b.option("--hex_max_size",
           cfg.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);
  b.option("--e1ap_json_enabled", cfg.e1ap_json_enabled, "Enable JSON logging of E1AP PDUs");
}

static void declare_pcap_args(config::config_builder& b, cu_up_unit_pcap_config& cfg)
{
  b.option("--n3_filename", cfg.n3.filename, "N3 GTP-U PCAP file output path");
  b.option("--n3_enable", cfg.n3.enabled, "Enable N3 GTP-U packet capture");
  b.option("--f1u_filename", cfg.f1u.filename, "F1-U GTP-U PCAP file output path");
  b.option("--f1u_enable", cfg.f1u.enabled, "F1-U GTP-U PCAP");
  b.option("--e1ap_filename", cfg.e1ap.filename, "E1AP PCAP file output path");
  b.option("--e1ap_enable", cfg.e1ap.enabled, "E1AP PCAP");
}

static void declare_metrics_layers_args(config::config_builder& b, cu_up_unit_metrics_layer_config& cfg)
{
  b.option("--enable_e1ap", cfg.enable_e1ap, "Enable E1AP metrics");
  b.option("--enable_pdcp", cfg.enable_pdcp, "Enable PDCP metrics");
  b.option("--enable_nrup_cu", cfg.enable_nrup, "Enable NRUP metrics (CU side)");
  b.option("--skip_cu_up_executor",
           cfg.skip_cu_up_executor,
           "Whether to skip logging CU-UP executor metrics when executor logging is enabled application wide");
}

static void declare_metrics_args(config::config_builder& b, cu_up_unit_metrics_config& cfg)
{
  b.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& p) {
    p.option("--cu_up_report_period", cfg.cu_up_report_period, "CU-UP metrics report period in milliseconds");
  });
  b.group("layers", "Layer basis metrics configuration",
          [&](config::config_builder& l) { declare_metrics_layers_args(l, cfg.layers_cfg); });
}

static void declare_trace_args(config::config_builder& b, cu_up_unit_trace_config& cfg)
{
  b.group("layers", "Metrics configuration", [&](config::config_builder& l) {
    l.option("--cu_up_enable", cfg.cu_up_enable, "Enable tracing for CU-UP executors");
  });
}

static void declare_f1u_cu_up_args(config::config_builder& b, cu_cp_unit_f1u_config& cfg)
{
  b.option("--backoff_timer", cfg.t_notify, "F1-U backoff timer (ms)");
  b.option("--queue_size", cfg.queue_size, "F1-U backoff timer (ms)");
  b.option("--batch_size", cfg.batch_size, "F1-U backoff timer (ms)");
}

static void declare_qos_args(config::config_builder& b, cu_up_unit_qos_config& cfg)
{
  b.option("--five_qi", cfg.five_qi, "5QI").range(0, 255);
  b.group("f1u_cu_up", "F1-U parameters at CU_UP side",
          [&](config::config_builder& f1u) { declare_f1u_cu_up_args(f1u, cfg.f1u_cu_up); });
}

void ocudu::configure_cli11_with_cu_up_unit_config_schema(config::config_builder& b, cu_up_unit_config& unit_cfg)
{
  b.option("--gnb_id", unit_cfg.gnb_id.id, "gNodeB identifier");
  b.option("--gnb_id_bit_length", unit_cfg.gnb_id.bit_length, "gNodeB identifier length in bits").range(22, 32);
  b.option("--gnb_cu_up_id", unit_cfg.gnb_cu_up_id, "gNB-CU-UP Id")
      .range(0.0, static_cast<double>((uint64_t(1) << 36) - 1));

  b.group("cu_up", "CU-UP parameters", [&](config::config_builder& cu_up) { declare_cu_up_args(cu_up, unit_cfg); });
  b.group("expert_execution", "Execution parameters",
          [&](config::config_builder& exec) { declare_execution_args(exec, unit_cfg.exec_cfg); });
  b.group("log", "Logging configuration",
          [&](config::config_builder& log) { declare_log_args(log, unit_cfg.loggers); });
  b.group("pcap", "Logging configuration",
          [&](config::config_builder& pcap) { declare_pcap_args(pcap, unit_cfg.pcap_cfg); });
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { declare_metrics_args(m, unit_cfg.metrics); });
  // Common metrics options (enable_json/log/verbose) also live under "metrics".
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, unit_cfg.metrics.common_metrics_cfg);
  b.group("trace", "General tracer configuration",
          [&](config::config_builder& tr) { declare_trace_args(tr, unit_cfg.trace_cfg); });

  b.array_of("--qos",
             unit_cfg.qos_cfg,
             "Configures RLC and PDCP radio bearers on a per 5QI basis.",
             [](config::config_builder& el, cu_up_unit_qos_config& qos) { declare_qos_args(el, qos); })
      .key("five_qi");
}

void ocudu::configure_cli11_with_cu_up_unit_config_schema(CLI::App& app, cu_up_unit_config& unit_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_cu_up_unit_config_schema(b, unit_cfg);
}
