// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_low_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "du_low_config.h"
#include "ocudu/adt/expected.h"
#include "ocudu/ran/slot_point.h"
#include "ocudu/ran/slot_point_extended.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"

using namespace ocudu;

static void configure_cli11_log_args(config::config_builder& b, du_low_unit_logger_config& log_params)
{
  app_helpers::add_log_option(b, log_params.phy_level, "--phy_level", "PHY log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.hal_level, "--hal_level", "HAL log level").fallback_from("--all_level");

  b.option("--broadcast_enabled",
           log_params.broadcast_enabled,
           "Enable logging in the physical and MAC layer of broadcast messages and all PRACH opportunities");
  b.option("--phy_rx_symbols_filename",
           log_params.phy_rx_symbols_filename,
           "Set to a valid file path to print the received symbols.");
  b.string_action(
      "--phy_rx_symbols_port",
      [&log_params](const std::string& value) {
        if (value.empty() || value == "all") {
          log_params.phy_rx_symbols_port.reset();
          return;
        }
        log_params.phy_rx_symbols_port = static_cast<unsigned>(std::stoul(value));
      },
      [&log_params]() -> std::string {
        return log_params.phy_rx_symbols_port.has_value() ? std::to_string(*log_params.phy_rx_symbols_port)
                                                          : std::string("all");
      },
      "Set to a valid receive port number to dump the IQ symbols from that port only, or set to \"all\" to dump "
      "the IQ symbols from all UL receive ports. Only works if \"phy_rx_symbols_filename\" is set.",
      "a non-negative port number or the sentinel \"all\"");
  b.option("--phy_rx_symbols_prach",
           log_params.phy_rx_symbols_prach,
           "Set to true to dump the IQ symbols from all the PRACH ports. Only works if "
           "\"phy_rx_symbols_filename\" is set.");

  b.option("--hex_max_size",
           log_params.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);
}

static void configure_cli11_trace_args(config::config_builder& b, du_low_unit_tracer_config& config)
{
  b.group("layers", "Layer basis tracing configuration", [&](config::config_builder& l) {
    l.option("--phy_enable", config.executor_tracing_enable, "Enable tracing for physical layer executors");
  });
}

static void configure_cli11_upper_phy_threads_args(config::config_builder& b, du_low_unit_expert_threads_config& config)
{
  b.enumeration("--pdsch_processor_type",
                config.pdsch_processor_type,
                "PDSCH processor type: auto, generic and flexible.",
                {"auto", "generic", "flexible"});

  // TODO: legacy CLI11 binding accepted "auto"/"default" (→ default_cb_batch_length) and "synchronous"
  // (→ synchronous_cb_batch_length) as sentinel strings via a transform. The builder-typed option<unsigned>
  // accepts only integers; sentinel keywords must be reintroduced by a runtime parser.
  b.option("--pdsch_cb_batch_length",
           config.pdsch_cb_batch_length,
           "PDSCH flexible processor codeblock-batch size.\n"
           "Set it to 'auto' to adapt the batch length to the number of threads dedicated to downlink processing,\n"
           "set it to 'synchronous' to disable batch-splitting and ensure that TB processing remains within the \n"
           "calling thread without parallelization.")
      .note("legal values: a non-negative integer; the sentinel strings \"auto\", \"default\" and \"synchronous\" are "
            "not yet supported by the builder API");

  b.option("--max_pucch_concurrency",
           config.max_pucch_concurrency,
           "Maximum PUCCH processing concurrency for all cells.\n"
           "Limits the maximum number of threads that can concurrently process Physical Uplink Control Channel\n"
           "(PUCCH). Set it to zero for no limit of threads.");
  b.option("--max_pusch_and_srs_concurrency",
           config.max_pusch_and_srs_concurrency,
           "Maximum PUSCH and SRS processing concurrency for all cells.\n"
           "Limits the maximum number of threads that can concurrently process Physical Uplink Shared Channel \n"
           "(PUSCH) and Sounding Reference Signals (SRS). Set it to zero for no limitation. If hardware \n"
           "acceleration is enabled, this parameter is set to the number of the accelerator queues.");
  b.option("--max_pdsch_concurrency",
           config.max_pdsch_concurrency,
           "Maximum concurrency level for PDSCH processing for all cells.\n"
           "Limits the number of threads that can concurrently process Physical Downlink Shared Channel (PDSCH).\n"
           "Set to zero for no limitation. If hardware acceleration is enabled, this parameter is set to the\n"
           "number of the accelerator queues.");
}

static void configure_cli11_expert_execution_args(config::config_builder& b, du_low_unit_expert_execution_config& config)
{
  b.group("threads", "Threads configuration", [&](config::config_builder& th) {
    th.group("upper_phy", "Upper PHY thread configuration", [&](config::config_builder& up) {
      configure_cli11_upper_phy_threads_args(up, config.threads);
    });
  });
}

static void configure_cli11_expert_phy_args(config::config_builder&              b,
                                            du_low_unit_expert_upper_phy_config& expert_phy_params)
{
  b.option("--max_proc_delay",
           expert_phy_params.max_processing_delay_slots,
           "Maximum allowed DL processing delay in slots.")
      .range(1, 30);
  b.option("--pusch_dec_max_iterations",
           expert_phy_params.pusch_decoder_max_iterations,
           "Maximum number of PUSCH LDPC decoder iterations");
  b.option("--pusch_dec_enable_early_stop",
           expert_phy_params.pusch_decoder_early_stop,
           "Enables PUSCH LDPC decoder early stop");
  b.option("--pusch_decoder_force_decoding",
           expert_phy_params.pusch_decoder_force_decoding,
           "Forces PUSCH LDPC decoder to decode always");
  b.enumeration("--pusch_sinr_calc_method",
                expert_phy_params.pusch_sinr_calc_method,
                "PUSCH SINR calculation method: channel_estimator, post_equalization and evm.",
                {"channel_estimator", "post_equalization", "evm"});
  b.enumeration("--pusch_channel_estimator_fd_strategy",
                expert_phy_params.pusch_channel_estimator_fd_strategy,
                "PUSCH channel estimator frequency-domain smoothing strategy: filter, mean and none.",
                {"filter", "mean", "none"});
  b.enumeration("--pusch_channel_estimator_td_strategy",
                expert_phy_params.pusch_channel_estimator_td_strategy,
                "PUSCH channel estimator time-domain strategy: average and interpolate.",
                {"average", "interpolate"});
  b.option("--pusch_channel_estimator_cfo_compensation",
           expert_phy_params.pusch_channel_estimator_cfo_compensation,
           "PUSCH channel estimator CFO compensation.");
  b.enumeration("--pusch_channel_equalizer_algorithm",
                expert_phy_params.pusch_channel_equalizer_algorithm,
                "PUSCH channel equalizer algorithm: zf and mmse.",
                {"zf", "mmse"});
  b.option("--max_request_headroom_slots",
           expert_phy_params.nof_slots_request_headroom,
           "Maximum request headroom size in slots.")
      .range(0, 30);
  b.option("--allow_request_on_empty_uplink_slot",
           expert_phy_params.allow_request_on_empty_uplink_slot,
           "Generates an uplink request in an uplink slot with no PUCCH/PUSCH/SRS PDUs");
  b.option("--enable_phy_tap",
           expert_phy_params.enable_phy_tap,
           "Enables or disables the PHY tap plugin if it is present while building the application.");
  b.option("--phy_tap_arguments",
           expert_phy_params.phy_tap_arguments,
           "PHY tap plugin argument string passed during construction.");
}

#ifdef DPDK_FOUND
static void configure_cli11_hwacc_pdsch_enc_args(config::config_builder& b, std::optional<hwacc_pdsch_appconfig>& config)
{
  config.emplace();

  b.option("--nof_hwacc", config->nof_hwacc, "Number of hardware-accelerated PDSCH encoding functions").range(0, 64);
  b.option("--cb_mode", config->cb_mode, "Operation mode of the PDSCH encoder (CB = true, TB = false [default])");
  b.option("--max_buffer_size",
           config->max_buffer_size,
           "Maximum supported buffer size in bytes (CB mode will be forced for larger TBs)");
  b.option("--dedicated_queue",
           config->dedicated_queue,
           "Hardware queue use for the PDSCH encoder (dedicated = true [default], shared = false)");
}
static void configure_cli11_hwacc_pusch_dec_args(config::config_builder& b, std::optional<hwacc_pusch_appconfig>& config)
{
  config.emplace();

  b.option("--nof_hwacc", config->nof_hwacc, "Number of hardware-accelerated PDSCH encoding functions").range(0, 64);
  b.option("--harq_context_size", config->harq_context_size, "Size of the HARQ context repository");
  b.option("--force_local_harq", config->force_local_harq, "Force using the host memory to implement the HARQ buffer");
  b.option("--dedicated_queue",
           config->dedicated_queue,
           "Hardware queue use for the PUSCH decoder (dedicated = true [default], shared = false)");
}

static void configure_cli11_bbdev_hwacc_args(config::config_builder& b, std::optional<bbdev_appconfig>& config)
{
  config.emplace();

  b.enumeration(
      "--hwacc_type", config->hwacc_type, "Type of BBDEV hardware-accelerator", {"acc100", "acc200", "vrb1"});
  b.option("--id", config->id, "ID of the BBDEV-based hardware-accelerator.").range(0, 65535);

  // (Optional) Hardware-accelerated PDSCH encoding functions configuration.
  b.group("pdsch_enc", "Hardware-accelerated PDSCH encoding functions configuration", [&](config::config_builder& enc) {
    configure_cli11_hwacc_pdsch_enc_args(enc, config->pdsch_enc);
  });

  // (Optional) Hardware-accelerated PUSCH decoding functions configuration.
  b.group("pusch_dec", "Hardware-accelerated PUSCH decoding functions configuration", [&](config::config_builder& dec) {
    configure_cli11_hwacc_pusch_dec_args(dec, config->pusch_dec);
  });

  b.option("--msg_mbuf_size",
           config->msg_mbuf_size,
           "Size of the mbufs storing unencoded and unrate-matched messages (in bytes)")
      .range(0, 64000);
  b.option("--rm_mbuf_size",
           config->rm_mbuf_size,
           "Size of the mbufs storing encoded and rate-matched messages (in bytes)")
      .range(0, 64000);
  b.option("--nof_mbuf", config->nof_mbuf, "Number of mbufs in the memory pool");
}

static void configure_cli11_hal_args(config::config_builder& b, std::optional<du_low_unit_hal_config>& config)
{
  config.emplace();

  // (Optional) BBDEV-based hardware-accelerator configuration.
  b.group("bbdev_hwacc",
          "BBDEV-based hardware-acceleration configuration parameters",
          [&](config::config_builder& bb) { configure_cli11_bbdev_hwacc_args(bb, config->bbdev_hwacc); });
}
#endif

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, du_low_unit_config& parsed_cfg)
{
  // Clean the HAL optional.
  if (app.get_subcommand("hal")->count_all() == 0) {
    parsed_cfg.hal_config.reset();

    return;
  }

  const auto& hal = app.get_subcommand("hal");
  if (hal->get_subcommand("bbdev_hwacc")->count_all() == 0) {
    parsed_cfg.hal_config->bbdev_hwacc.reset();
  }
}
#endif

static void configure_cli11_metrics_args(config::config_builder& b, du_low_unit_metrics_config& metrics_params)
{
  b.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
    l.option("--enable_du_low", metrics_params.enable_du_low, "Enable DU low metrics (upper physical layer)");
  });
}

void ocudu::configure_cli11_with_du_low_config_schema(config::config_builder& b, du_low_unit_config& parsed_cfg)
{
  // Loggers section.
  b.group("log", "Logging configuration",
          [&](config::config_builder& log_b) { configure_cli11_log_args(log_b, parsed_cfg.loggers); });

  // Tracer section.
  b.group("trace", "General tracer configuration",
          [&](config::config_builder& tr) { configure_cli11_trace_args(tr, parsed_cfg.tracer); });

  // Expert upper PHY section.
  b.group("expert_phy", "Expert physical layer configuration",
          [&](config::config_builder& ep) { configure_cli11_expert_phy_args(ep, parsed_cfg.expert_phy_cfg); });

  // Expert execution section.
  b.group("expert_execution", "Expert execution configuration",
          [&](config::config_builder& ex) { configure_cli11_expert_execution_args(ex, parsed_cfg.expert_execution_cfg); });

#ifdef DPDK_FOUND
  // HAL section.
  b.group("hal", "HAL configuration",
          [&](config::config_builder& hal) { configure_cli11_hal_args(hal, parsed_cfg.hal_config); });
#endif

  // Metrics section.
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, parsed_cfg.metrics_cfg.common_metrics_cfg);
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { configure_cli11_metrics_args(m, parsed_cfg.metrics_cfg); });
}

void ocudu::configure_cli11_with_du_low_config_schema(CLI::App& app, du_low_unit_config& parsed_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_du_low_config_schema(b, parsed_cfg);
}

void ocudu::autoderive_du_low_parameters_after_parsing(CLI::App& app, du_low_unit_config& parsed_cfg, duplex_mode mode)
{
  // If max proc delay property is not present in the config, configure the default value.
  CLI::App* expert_cmd = app.get_subcommand("expert_phy");
  if (expert_cmd->count_all() == 0 || expert_cmd->count("--max_proc_delay") == 0) {
    switch (mode) {
      case duplex_mode::TDD:
        parsed_cfg.expert_phy_cfg.max_processing_delay_slots = 5;
        break;
      case duplex_mode::FDD:
        parsed_cfg.expert_phy_cfg.max_processing_delay_slots = 2;
        break;
      default:
        break;
    }
  }

  // If max request headroom slots property is present in the config, do nothing.
  if (expert_cmd->count_all() == 0 || expert_cmd->count("--max_request_headroom_slots") == 0) {
    parsed_cfg.expert_phy_cfg.nof_slots_request_headroom = parsed_cfg.expert_phy_cfg.max_processing_delay_slots;
  }

#ifdef DPDK_FOUND
  manage_hal_optional(app, parsed_cfg);
#endif
}
