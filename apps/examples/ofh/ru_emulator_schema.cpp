// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "ru_emulator_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "helpers.h"
#include "ru_emulator_appconfig.h"
#include "ocudu/support/error_handling.h"
#include <algorithm>
#include <sstream>

using namespace ocudu;

namespace {

/// Canonical name<->value mapping for ocudulog::basic_levels.
std::vector<std::pair<std::string, ocudulog::basic_levels>> basic_levels_mapping()
{
  return {
      {"none", ocudulog::basic_levels::none},
      {"error", ocudulog::basic_levels::error},
      {"warning", ocudulog::basic_levels::warning},
      {"info", ocudulog::basic_levels::info},
      {"debug", ocudulog::basic_levels::debug},
  };
}

} // namespace

static void declare_log_args(config::config_builder& b, ru_emulator_log_appconfig& log_params)
{
  b.option("--filename", log_params.filename, "Log file output path");
  b.enum_option("--level", log_params.level, "Log level", basic_levels_mapping()).type_name("log-level");
}

static void declare_ru_emu_dpdk_args(config::config_builder& b, std::optional<ru_emulator_dpdk_appconfig>& config)
{
  config.emplace();
  b.option("--eal_args", config->eal_args, "EAL configuration parameters used to initialize DPDK");
}

static void declare_ru_emu_args(config::config_builder& b, ru_emulator_ofh_appconfig& config)
{
  // Channel bandwidth: integer-in-MHz mapped to a bs_channel_bandwidth enum.
  // Until the builder gains a numeric->enum primitive, declared as a string
  // action that accepts the same MHz set the legacy validator allowed.
  b.string_action(
      "--bandwidth",
      [&config](const std::string& value) {
        std::stringstream ss(value);
        unsigned          bw = 0;
        ss >> bw;
        if (!is_valid_bw(bw)) {
          report_error(
              "Error in the channel bandwidth property. Valid values [5,10,15,20,25,30,40,50,60,70,80,90,100]");
        }
        config.bandwidth = MHz_to_bs_channel_bandwidth(bw);
      },
      [&config]() -> std::string { return std::to_string(bs_channel_bandwidth_to_MHz(config.bandwidth)); },
      "Channel bandwidth in MHz",
      "one of: 5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100");

  b.option("--compr_method_ul", config.ul_compr_method, "Uplink compression method").enum_values({"none", "bfp"});
  b.option("--compr_bitwidth_ul", config.ul_compr_bitwidth, "Uplink compression bit width")
      .note("integer; accepted values 9 or 16");
  b.option("--network_interface", config.network_interface, "PCIe identifier of network device");
  b.option("--ru_mac_addr", config.ru_mac_address, "Radio Unit MAC address");
  b.option("--du_mac_addr", config.du_mac_address, "Distributed Unit MAC address");
  b.option("--vlan_tag", config.vlan_tag, "V-LAN identifier").range(1, 65536);
  b.option("--enable_promiscuous", config.enable_promiscuous, "Promiscuous mode flag");
  b.option("--ul_port_id", config.ru_ul_port_id, "RU uplink port identifier");
  b.option("--dl_port_id", config.ru_dl_port_id, "RU downlink port identifier");
  b.option("--prach_port_id", config.ru_prach_port_id, "RU PRACH port identifier");

  b.option("--t2a_max_cp_dl", config.T2a_max_cp_dl, "T2a maximum value for downlink Control-Plane").range(0, 1960);
  b.option("--t2a_min_cp_dl", config.T2a_min_cp_dl, "T2a minimum value for downlink Control-Plane").range(0, 1960);
  b.option("--t2a_max_cp_ul", config.T2a_max_cp_ul, "T2a maximum value for uplink Control-Plane").range(0, 1960);
  b.option("--t2a_min_cp_ul", config.T2a_min_cp_ul, "T2a minimum value for uplink Control-Plane").range(0, 1960);
  b.option("--t2a_max_up", config.T2a_max_up, "T2a maximum value for User-Plane").range(0, 1960);
  b.option("--t2a_min_up", config.T2a_min_up, "T2a minimum value for User-Plane").range(0, 1960);

  b.enum_option("--prach_format",
                config.prach_format,
                "PRACH format. Set to 'long' to use format 0, or 'short' to use format B4",
                {{"long", ru_emulator_prach_format::LONG_F0}, {"short", ru_emulator_prach_format::SHORT_B4}});
}

void ocudu::declare_ru_emulator_appconfig_schema(config::config_builder& root, ru_emulator_appconfig& ru_emu_parsed_cfg)
{
  root.group("log", "Logging configuration",
             [&](config::config_builder& log_b) { declare_log_args(log_b, ru_emu_parsed_cfg.log_cfg); });

  root.group("ru_emu", "Open Fronthaul Radio Unit emulator configuration", [&](config::config_builder& ru) {
    ru.array_of("--cells",
                ru_emu_parsed_cfg.ru_cfg,
                "Sets the RU emulator configuration",
                [](config::config_builder& el, ru_emulator_ofh_appconfig& cfg) { declare_ru_emu_args(el, cfg); });
  });

  root.group("dpdk", "DPDK configuration",
             [&](config::config_builder& dpdk) { declare_ru_emu_dpdk_args(dpdk, ru_emu_parsed_cfg.dpdk_config); });
}
