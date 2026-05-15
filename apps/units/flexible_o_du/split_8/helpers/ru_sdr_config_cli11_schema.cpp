// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "ru_sdr_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "apps/services/worker_manager/cli11_cpu_affinities_parser_helper.h"
#include "ru_sdr_config.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"

using namespace ocudu;

static void configure_cli11_amplitude_control_args(config::config_builder&        b,
                                                   amplitude_control_unit_config& amplitude_params)
{
  b.option("--tx_gain_backoff",
           amplitude_params.gain_backoff_dB,
           "Gain back-off to accommodate the signal PAPR in decibels");
  b.option("--enable_clipping", amplitude_params.enable_clipping, "Signal clipping");
  b.option("--ceiling", amplitude_params.power_ceiling_dBFS, "Clipping ceiling referenced to full scale");
}

static void configure_cli11_ru_sdr_expert_args(config::config_builder& b, ru_sdr_unit_expert_config& config)
{
  b.option("--low_phy_dl_throttling",
           config.lphy_dl_throttling,
           "System time-based throttling.\n"
           "Determines a minimum baseband processor period time between downlink packets. It is expressed as a \n"
           "fraction of the time equivalent to the number of samples in the baseband buffer. Set to 0.9 to ensure \n"
           "that the downlink packets are processed with a minimum period of 90% of the buffer duration.\n"
           "Set to zero to disable this feature.");
  b.enumeration("--tx_mode",
                config.transmission_mode,
                "Selects a radio transmission mode. Discontinuous modes are not supported by all radios.\n"
                "  continuous:    the TX chain is always active.\n"
                "  discontinuous: the transmitter stops when there is no data to transmit.\n"
                "  same-port:     the radio transmits and receives from the same antenna port.\n",
                {"continuous", "discontinuous", "same-port"});

  b.option("--power_ramping_time_us",
           config.power_ramping_time_us,
           "Specifies the power ramping time in microseconds, it proactively initiates the transmission and \n"
           "mitigates transient effects.");
}

static void configure_cli11_ru_sdr_args(config::config_builder& b, ru_sdr_unit_config& config)
{
  b.option("--srate", config.srate_MHz, "Sample rate in MHz");
  b.option("--device_driver", config.device_driver, "Device driver name");
  b.option("--device_args", config.device_arguments, "Optional device arguments");
  b.option("--tx_gain", config.tx_gain_dB, "Transmit gain in decibels");
  b.option("--rx_gain", config.rx_gain_dB, "Receive gain in decibels");
  b.option("--freq_offset", config.center_freq_offset_Hz, "Center frequency offset in hertz");
  b.option("--clock_ppm", config.calibrate_clock_ppm, "Clock calibration in PPM.");
  b.option("--lo_offset", config.lo_offset_MHz, "LO frequency offset in MHz");
  b.option("--clock", config.clock_source, "Clock source");
  b.option("--sync", config.synch_source, "Time synchronization source");
  b.option("--otw_format", config.otw_format, "Over-the-wire format");

  b.string_action(
      "--time_alignment_calibration",
      [&config](const std::string& value) {
        if (value.empty() || value == "auto") {
          config.time_alignment_calibration.reset();
          return;
        }
        config.time_alignment_calibration = std::stoi(value);
      },
      [&config]() -> std::string {
        return config.time_alignment_calibration.has_value() ? std::to_string(*config.time_alignment_calibration)
                                                             : std::string("auto");
      },
      "Rx to Tx radio time alignment calibration in samples.\n"
      "Positive values reduce the RF transmission delay with respect\n"
      "to the RF reception, while negative values increase it",
      "a signed integer or the sentinel \"auto\" (skip calibration)");

  // TODO: legacy CLI11 binding parsed "%Y-%m-%d %H:%M:%S" formatted strings into a std::chrono::system_clock
  // time_point. The builder API has no scalar type for time_point, so this option is not declared on the builder.
  // The option must be reintroduced via a runtime parser when builder support exists.
  // (Original CLI11 option: "--start_time")

  b.option("--dl_freq_Hz",
           config.dl_freq_override_Hz,
           "Downlink frequency in Hz. If present, it overrides the one derived by DL ARFCN and NR Band.");

  b.option("--ul_freq_Hz",
           config.ul_freq_override_Hz,
           "Uplink frequency in Hz. If present, it overrides the one derived by UL ARFCN and NR Band.");

  // Amplitude control configuration.
  b.group("amplitude_control", "Amplitude control parameters", [&](config::config_builder& amp) {
    configure_cli11_amplitude_control_args(amp, config.amplitude_cfg);
  });

  // Expert configuration.
  b.group("expert_cfg", "Generic Radio Unit expert configuration", [&](config::config_builder& exp) {
    configure_cli11_ru_sdr_expert_args(exp, config.expert_cfg);
  });
}

static void configure_cli11_log_args(config::config_builder& b, ru_sdr_unit_logger_config& log_params)
{
  app_helpers::add_log_option(b, log_params.radio_level, "--radio_level", "Radio log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, log_params.phy_level, "--phy_level", "PHY log level").fallback_from("--all_level");
}

static void configure_cli11_cell_affinity_args(config::config_builder& b, ru_sdr_unit_cpu_affinities_cell_config& config)
{
  b.string_action(
      "--ru_cpus",
      [&config](const std::string& value) { parse_affinity_mask(config.ru_cpu_cfg.mask, value, "ru_cpus"); },
      [&config]() -> std::string {
        return fmt::format("{:,}", span<const size_t>(config.ru_cpu_cfg.mask.get_cpu_ids()));
      },
      "CPU cores used for the Radio Unit tasks",
      "comma-separated CPU ids or ranges, e.g. \"0-3,5\"");

  b.string_action(
      "--ru_pinning",
      [&config](const std::string& value) {
        config.ru_cpu_cfg.pinning_policy = to_affinity_mask_policy(value);
        if (config.ru_cpu_cfg.pinning_policy == sched_affinity_mask_policy::last) {
          report_error("Incorrect value={} used in {} property", value, "ru_pinning");
        }
      },
      [&config]() -> std::string { return to_string(config.ru_cpu_cfg.pinning_policy); },
      "Policy used for assigning CPU cores to the Radio Unit tasks",
      "one of: mask, round-robin");
}

static void configure_cli11_lower_phy_threads_args(config::config_builder& b, lower_phy_thread_profile& execution_profile)
{
  b.enum_option("--execution_profile",
                execution_profile,
                "Lower physical layer executor profile [single, dual, triple].\n"
                " - single: one task worker for all the lower physical layer task executors.\n"
                " - dual: two task workers - one for the downlink and one for the uplink.\n"
                " - triple: dedicated task workers for each of the subtasks (demodulation, reception and "
                "transmission).",
                {{"blocking", lower_phy_thread_profile::blocking},
                 {"single", lower_phy_thread_profile::single},
                 {"dual", lower_phy_thread_profile::dual},
                 {"triple", lower_phy_thread_profile::triple}});
}

static void configure_cli11_expert_execution_args(config::config_builder&              b,
                                                  ru_sdr_unit_expert_execution_config& config)
{
  b.group("threads", "Threads configuration", [&](config::config_builder& th) {
    th.group("lower_phy", "Lower PHY thread configuration", [&](config::config_builder& lp) {
      configure_cli11_lower_phy_threads_args(lp, config.threads.execution_profile);
    });
  });

  // Cell affinity section.
  b.array_of("--cell_affinities",
             config.cell_affinities,
             "Sets the cell CPU affinities configuration on a per cell basis",
             [](config::config_builder& el, ru_sdr_unit_cpu_affinities_cell_config& cell) {
               configure_cli11_cell_affinity_args(el, cell);
             });
}

static void configure_cli11_metrics_args(config::config_builder& b, ru_sdr_unit_metrics_config& config)
{
  b.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
    l.option("--enable_ru", config.enable_ru_metrics, "Enable Radio Unit metrics");
  });
}

void ocudu::configure_cli11_with_ru_sdr_config_schema(config::config_builder& b, ru_sdr_unit_config& parsed_cfg)
{
  /// RU SDR section.
  b.group("ru_sdr", "SDR Radio Unit configuration",
          [&](config::config_builder& ru) { configure_cli11_ru_sdr_args(ru, parsed_cfg); });

  // Loggers section.
  b.group("log", "Logging configuration",
          [&](config::config_builder& log) { configure_cli11_log_args(log, parsed_cfg.loggers); });

  // Expert execution section.
  b.group("expert_execution", "Expert execution configuration", [&](config::config_builder& ex) {
    configure_cli11_expert_execution_args(ex, parsed_cfg.expert_execution_cfg);
  });

  // Metrics section.
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, parsed_cfg.metrics_cfg.metrics_cfg);
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { configure_cli11_metrics_args(m, parsed_cfg.metrics_cfg); });
}

void ocudu::configure_cli11_with_ru_sdr_config_schema(CLI::App& app, ru_sdr_unit_config& parsed_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_ru_sdr_config_schema(b, parsed_cfg);
}

void ocudu::autoderive_ru_sdr_parameters_after_parsing(CLI::App&           app,
                                                       ru_sdr_unit_config& parsed_cfg,
                                                       unsigned            nof_cells)
{
  if (parsed_cfg.expert_execution_cfg.cell_affinities.size() < nof_cells) {
    parsed_cfg.expert_execution_cfg.cell_affinities.resize(nof_cells);
  }

  // Set the lower PHY to blocking for ZMQ.
  if (parsed_cfg.device_driver == "zmq") {
    parsed_cfg.expert_execution_cfg.threads.execution_profile = lower_phy_thread_profile::blocking;
  }
}
