// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "ru_ofh_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "apps/services/worker_manager/cli11_cpu_affinities_parser_helper.h"
#include "ru_ofh_config.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"

using namespace ocudu;

static void configure_cli11_ru_ofh_base_cell_args(config::config_builder& b, ru_ofh_unit_base_cell_config& config)
{
  // TODO: legacy CLI11 binding parsed the bandwidth string into a bs_channel_bandwidth enum via
  // MHz_to_bs_channel_bandwidth() and required one of {5,10,15,20,25,30,40,50,60,70,80,90,100} MHz. The builder API
  // has no enum-from-numeric-MHz scalar so this option is not declared. Equivalent behaviour must be added via a
  // runtime parser.
  // (Original CLI11 option: "--ru_bandwidth_MHz")

  // Note: For the timing parameters, worst case is 2 slots for scs 15KHz and 14 symbols. Implementation defined.
  b.option("--t1a_max_cp_dl", config.T1a_max_cp_dl, "T1a maximum value for downlink Control-Plane").range(0, 1960);
  b.option("--t1a_min_cp_dl", config.T1a_min_cp_dl, "T1a minimum value for downlink Control-Plane").range(0, 1960);
  b.option("--t1a_max_cp_ul", config.T1a_max_cp_ul, "T1a maximum value for uplink Control-Plane").range(0, 1960);
  b.option("--t1a_min_cp_ul", config.T1a_min_cp_ul, "T1a minimum value for uplink Control-Plane").range(0, 1960);
  b.option("--t1a_max_up", config.T1a_max_up, "T1a maximum value for User-Plane").range(0, 1960);
  b.option("--t1a_min_up", config.T1a_min_up, "T1a minimum value for User-Plane").range(0, 1960);
  b.option("--ta4_max", config.Ta4_max, "Ta4 maximum value for User-Plane").range(0, 1960);
  b.option("--ta4_min", config.Ta4_min, "Ta4 minimum value for User-Plane").range(0, 1960);

  // TODO: legacy CLI11 binding called report_error() at declaration time when T1a_min_* > T1a_max_*. These pairwise
  // cross-field checks belong in a runtime validator and are not expressed by the builder taxonomy. (Original
  // checks: T1a_min_cp_dl <= T1a_max_cp_dl, T1a_min_cp_ul <= T1a_max_cp_ul, T1a_min_up <= T1a_max_up.)

  b.option("--is_prach_cp_enabled", config.is_prach_control_plane_enabled, "PRACH Control-Plane enabled flag");
  b.option("--ignore_ecpri_seq_id", config.ignore_ecpri_seq_id_field, "Ignore eCPRI sequence id field value");
  b.option("--ignore_ecpri_payload_size",
           config.ignore_ecpri_payload_size_field,
           "Ignore eCPRI payload size field value");
  b.option("--ignore_prach_start_symbol",
           config.ignore_prach_start_symbol,
           "Ignore the start symbol field in the PRACH U-Plane packets");

  b.option("--log_lates_as_warnings", config.enable_log_warnings_for_lates, "Log late events as warnings");

  b.enum_option("--warn_unreceived_ru_frames",
                config.log_unreceived_ru_frames,
                "Warn of unreceived Radio Unit frames",
                {{"never", ofh::warn_unreceived_ru_frames::never},
                 {"always", ofh::warn_unreceived_ru_frames::always},
                 {"after_traffic_detection", ofh::warn_unreceived_ru_frames::after_traffic_detection}});

  const std::vector<std::string> compression_methods = {
      "none", "bfp", "bfp selective", "block scaling", "mu law", "modulation", "modulation selective"};

  b.enumeration(
      "--compr_method_ul", config.compression_method_ul, "Uplink compression method", compression_methods);
  b.option("--compr_bitwidth_ul", config.compression_bitwidth_ul, "Uplink compression bit width").range(1, 16);
  b.enumeration(
      "--compr_method_dl", config.compression_method_dl, "Downlink compression method", compression_methods);
  b.option("--compr_bitwidth_dl", config.compression_bitwidth_dl, "Downlink compression bit width").range(1, 16);
  b.enumeration(
      "--compr_method_prach", config.compression_method_prach, "PRACH compression method", compression_methods);
  b.option("--compr_bitwidth_prach", config.compression_bitwidth_prach, "PRACH compression bit width").range(1, 16);
  b.option("--enable_ul_static_compr_hdr",
           config.is_uplink_static_comp_hdr_enabled,
           "Uplink static compression header enabled flag");
  b.option("--enable_dl_static_compr_hdr",
           config.is_downlink_static_comp_hdr_enabled,
           "Downlink static compression header enabled flag");

  // TODO: legacy CLI11 binding wrote these three values into the std::variant<std::monostate, ru_ofh_scaling_config,
  // ru_ofh_legacy_scaling_config> field config.iq_scaling_config, emplacing the appropriate alternative and
  // enforcing that the legacy and new scaling parameters were not mixed. The builder API has no variant-target
  // scalar yet, so these three options are not declared. Equivalent behaviour must be re-added via a runtime parser.
  // (Original CLI11 options: "--ru_reference_level_dBFS", "--subcarrier_rms_backoff_dB", "--iq_scaling")

  // TODO: legacy CLI11 binding mapped the integer C-Plane PRACH FFT size to an ofh::cplane_fft_size enum
  // (0 -> fft_noop, 1536 -> fft_1536, 3072 -> fft_3072, otherwise log2(value)). The builder API does not yet
  // expose this kind of numeric-keyed enum. The legal numeric values were [0, 128, 256, 512, 1024, 1536, 2048,
  // 3072, 4096]. Equivalent behaviour must be reintroduced via a runtime parser.
  // (Original CLI11 option: "--cplane_prach_fft_len")
  (void)config;

  // TODO: legacy CLI11 binding had an app.callback() that asserted that --compr_method_<dir> and
  // --compr_bitwidth_<dir> are both set or both unset, and that --iq_scaling is not mixed with --ru_reference_level
  // / --subcarrier_rms_backoff_dB. These post-parse cross-field checks belong in a runtime validator and are not
  // expressed by the builder taxonomy.
}

static void configure_cli11_ru_ofh_cells_args(config::config_builder& b, ru_ofh_unit_cell_config& config)
{
  configure_cli11_ru_ofh_base_cell_args(b, config.cell);
  b.option("--network_interface",
           config.network_interface,
           "Network interface name for raw sockets. PCIe (or other bus) port identifier when using DPDK");
  b.option("--enable_promiscuous", config.enable_promiscuous_mode, "Promiscuous mode flag");
  b.option("--check_link_status", config.check_link_status, "Ethernet link status checking flag");
  // TODO: legacy CLI11 binding used a units::bytes wrapper as the target with check(CLI::Range(1500, 9600)). The
  // builder API does not yet expose a units::bytes scalar, so the option is not declared. Equivalent behaviour must
  // be reintroduced via a runtime parser.
  // (Original CLI11 option: "--mtu")
  b.option("--ru_mac_addr", config.ru_mac_address, "Radio Unit MAC address");
  b.option("--du_mac_addr", config.du_mac_address, "Distributed Unit MAC address");

  b.option("--vlan_tag_cp", config.vlan_tag_cp, "C-Plane VLAN identifier").range(1, 4094);
  b.option("--vlan_tag_up", config.vlan_tag_up, "U-Plane VLAN identifier").range(1, 4094);

  b.option("--vlan_pcp_cp", config.vlan_pcp_cp, "C-Plane VLAN PCP")
      .range(0, 7)
      .note("requires --vlan_tag_cp to be set as well");
  b.option("--vlan_pcp_up", config.vlan_pcp_up, "U-Plane VLAN PCP")
      .range(0, 7)
      .note("requires --vlan_tag_up to be set as well");

  b.option("--prach_port_id", config.ru_prach_port_id, "RU PRACH port identifier");
  b.option("--dl_port_id", config.ru_dl_port_id, "RU downlink port identifier");
  b.option("--ul_port_id", config.ru_ul_port_id, "RU uplink port identifier");
}

static void configure_cli11_ru_ofh_args(config::config_builder& b, ru_ofh_unit_parsed_config& config)
{
  ru_ofh_unit_config& ofh_cfg = config.config;
  b.option("--gps_alpha", ofh_cfg.gps_Alpha, "GPS Alpha").range(0.0, 1.2288e7);
  b.option("--gps_beta", ofh_cfg.gps_Beta, "GPS Beta").range(-32768, 32767);

  // Common cell parameters. In legacy CLI11 this was an option group with a parse_complete_callback that copied the
  // base cell defaults into each entry of --cells. The builder API does not yet expose option-group semantics nor
  // parse-complete hooks, so the propagation must be re-introduced via a runtime step. The base_cell options are
  // declared here so existing YAML configs that set them still parse, but the values written into
  // config.base_cell_cfg are not automatically propagated to config.config.cells anymore.
  b.group("base_cell", "Base cell parameters that propagate to each entry in --cells", [&](config::config_builder& bc) {
    configure_cli11_ru_ofh_base_cell_args(bc, config.base_cell_cfg);
  });

  // Cell parameters.
  b.array_of("--cells",
             ofh_cfg.cells,
             "Sets the cell configuration on a per cell basis, overwriting the default configuration defined by "
             "cell_cfg",
             [](config::config_builder& el, ru_ofh_unit_cell_config& cell) {
               configure_cli11_ru_ofh_cells_args(el, cell);
             });
}

static void configure_cli11_log_args(config::config_builder& b, ru_ofh_unit_logger_config& log_params)
{
  app_helpers::add_log_option(b, log_params.ofh_level, "--ofh_level", "Open Fronthaul log level")
      .fallback_from("--all_level");
}

static void configure_cli11_cell_affinity_args(config::config_builder& b, ru_ofh_unit_cpu_affinities_cell_config& config)
{
  // TODO: legacy CLI11 binding parsed CPU affinity masks via parse_affinity_mask() (accepting forms like "0-3",
  // "0,2", etc.). The builder API has no CPU-mask scalar yet. The options below are bound to throwaway string
  // buffers so the schema records their existence; the values are NOT applied to the affinity config and must be
  // re-wired through a runtime parser once the builder gains support.
  static thread_local std::string ru_cpus_buffer;
  b.option("--ru_cpus", ru_cpus_buffer, "Number of CPUs used for the Radio Unit tasks")
      .note("legal value: a CPU mask string (e.g. \"0-3\", \"0,2\"); not yet parsed into the affinity bitmask by the "
            "builder API");

  static thread_local std::string ru_pinning_buffer;
  b.option("--ru_pinning", ru_pinning_buffer, "Policy used for assigning CPU cores to the Radio Unit tasks")
      .note("legal value: a pinning-policy string; not yet applied to the affinity config by the builder API");
  (void)config;
}

static void configure_cli11_expert_execution_args(config::config_builder&              b,
                                                  ru_ofh_unit_expert_execution_config& config)
{
  // Affinities section.
  b.group("affinities", "gNB CPU affinities configuration", [&](config::config_builder& af) {
    af.group("ofh", "Open Fronthaul CPU affinities configuration", [&](config::config_builder& ofh) {
      // TODO: legacy CLI11 binding parsed --timing_cpu via parse_affinity_mask(). The builder API has no CPU-mask
      // scalar yet, so the option is bound to a throwaway string buffer for schema visibility; the value is NOT
      // applied to config.ru_timing_cpu and must be reintroduced via a runtime parser.
      static thread_local std::string timing_cpu_buffer;
      ofh.option("--timing_cpu", timing_cpu_buffer, "CPU used for timing in the Radio Unit")
          .note("legal value: a CPU mask string; not yet parsed into the affinity bitmask by the builder API");
      (void)config;

      // TODO: legacy CLI11 binding parsed --txrx_cpus as a vector of CPU affinity bitmasks (each entry passed
      // through parse_affinity_mask()). The builder API has no list-of-CPU-mask scalar yet, so the option is not
      // declared. Equivalent behaviour must be reintroduced via a runtime parser.
      // (Original CLI11 option: "--txrx_cpus")
    });
  });

  // Cell affinity section.
  b.array_of("--cell_affinities",
             config.cell_affinities,
             "Sets the cell CPU affinities configuration on a per cell basis",
             [](config::config_builder& el, ru_ofh_unit_cpu_affinities_cell_config& cell) {
               configure_cli11_cell_affinity_args(el, cell);
             });

  // Threads section.
  b.group("threads", "Threads configuration", [&](config::config_builder& th) {
    th.group("ofh", "Open Fronthaul thread configuration", [&](config::config_builder& ofh) {
      ofh.option("--enable_busy_waiting",
                 config.enable_busy_waiting,
                 "Enable busy waiting of the RU timing worker");
    });
  });
}

#ifdef DPDK_FOUND
static void configure_cli11_hal_args(config::config_builder& b, std::optional<ru_ofh_unit_hal_config>& config)
{
  config.emplace();

  b.option("--eal_args", config->eal_args, "EAL configuration parameters used to initialize DPDK");
}
#endif

static void configure_cli11_metrics_args(config::config_builder& b, ru_ofh_unit_metrics_config& config)
{
  b.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
    l.option("--enable_ru", config.enable_ru_metrics, "Enable Radio Unit metrics");
  });
}

void ocudu::configure_cli11_with_ru_ofh_config_schema(config::config_builder& b, ru_ofh_unit_parsed_config& parsed_cfg)
{
  // OFH RU section.
  b.group("ru_ofh", "Open Fronthaul Radio Unit configuration",
          [&](config::config_builder& ru) { configure_cli11_ru_ofh_args(ru, parsed_cfg); });

  // Loggers section.
  b.group("log", "Logging configuration",
          [&](config::config_builder& log) { configure_cli11_log_args(log, parsed_cfg.config.loggers); });

  // Expert execution section.
  b.group("expert_execution", "Expert execution configuration", [&](config::config_builder& ex) {
    configure_cli11_expert_execution_args(ex, parsed_cfg.config.expert_execution_cfg);
  });

  // HAL section only available when DPDK is present.
#ifdef DPDK_FOUND
  b.group("hal", "HAL configuration",
          [&](config::config_builder& hal) { configure_cli11_hal_args(hal, parsed_cfg.config.hal_config); });
#endif

  // Metrics section.
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, parsed_cfg.config.metrics_cfg.metrics_cfg);
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { configure_cli11_metrics_args(m, parsed_cfg.config.metrics_cfg); });
}

void ocudu::configure_cli11_with_ru_ofh_config_schema(CLI::App& app, ru_ofh_unit_parsed_config& parsed_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_ru_ofh_config_schema(b, parsed_cfg);
}

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, std::optional<ru_ofh_unit_hal_config>& hal_config)
{
  // Clean the HAL optional.
  if (auto subcmd = app.get_subcommand("hal"); subcmd->count_all() == 0) {
    hal_config.reset();
    // As HAL configuration is optional, disable the command when it is not present in the configuration.
    subcmd->disabled();
  }
}
#endif

void ocudu::autoderive_ru_ofh_parameters_after_parsing(CLI::App& app, ru_ofh_unit_parsed_config& parsed_cfg)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, parsed_cfg.config.hal_config);
#endif
}
