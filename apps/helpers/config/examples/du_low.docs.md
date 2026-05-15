# DU-low configuration reference

## Reusable types

### <a id="types-log-level"></a>`log-level`

- Type: string
- Constraints: enum: none, error, warning, info, debug

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dryrun` | boolean | `false` |  | Enable application dry run mode |
| `start_time_jitter` | integer | `0` | 0..600 | Start time jitter in milliseconds. A value of 0 disables the start time calculation and the session starts it as soon as possible |


## log

Logging configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `/tmp/du_low.log` |  | Log file output path |
| `all_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Default log level for PHY, MAC, RLC, PDCP, RRC, SDAP, NGAP and GTPU |
| `lib_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Generic log level |
| `e2ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | E2AP log level |
| `config_level` | [`log-level`](#types-log-level) | `none` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Config log level |
| `hex_max_size` | integer | `0` | -1..1024 | Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes) |
| `phy_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | PHY log level |
| `hal_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | HAL log level |
| `broadcast_enabled` | boolean | `false` |  | Enable logging in the physical and MAC layer of broadcast messages and all PRACH opportunities |
| `phy_rx_symbols_filename` | string | `` |  | Set to a valid file path to print the received symbols. |
| `phy_rx_symbols_port` | string | `0` | a non-negative port number or the sentinel "all" | Set to a valid receive port number to dump the IQ symbols from that port only, or set to "all" to dump the IQ symbols from all UL receive ports. Only works if "phy_rx_symbols_filename" is set. |
| `phy_rx_symbols_prach` | boolean | `false` |  | Set to true to dump the IQ symbols from all the PRACH ports. Only works if "phy_rx_symbols_filename" is set. |
| `ofh_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Open Fronthaul log level |
| `radio_level` | [`log-level`](#types-log-level) | `info` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Radio log level |
| `fapi_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | FAPI log level |


## trace

General tracer configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `` |  | Set to a valid file path to enable tracing and write the trace to the file |
| `max_tracing_events_per_file` | integer | `1000000` |  | Maximum number of events per file. Set to zero for no limit |
| `nof_tracing_events_after_severe` | integer | `0` |  | Number of events to write prior to a severe event. Set to zero for writing all events |


### layers

Layer basis tracing configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `phy_enable` | boolean | `false` |  | Enable tracing for physical layer executors |


## expert_execution

Expert execution configuration


### affinities

Application CPU affinities configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `main_pool_cpus` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5,7" | CPU cores assigned to main thread pool |
| `main_pool_pinning` | string | `mask` | one of: mask, round-robin | Policy used for assigning CPU cores to the main thread pool |


#### ofh

Open Fronthaul CPU affinities configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `timing_cpu` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5" | CPU used for timing in the Radio Unit |


### threads

Threads configuration


#### main_pool

Main thread pool configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_threads` | integer |  |  | Number of threads for processing upper PHY and upper layers. |
| `task_queue_size` | integer | `2048` |  | Main thread pool task queue size. |
| `backoff_period` | integer | `50` |  | Main thread pool back-off period, in microseconds. |


#### upper_phy

Upper PHY thread configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pdsch_processor_type` | string | `auto` | enum: auto, generic, flexible | PDSCH processor type: auto, generic and flexible. |
| `pdsch_cb_batch_length` | integer | `4` | legal values: a non-negative integer; the sentinel strings "auto", "default" and "synchronous" are not yet supported by the builder API | PDSCH flexible processor codeblock-batch size.
Set it to 'auto' to adapt the batch length to the number of threads dedicated to downlink processing,
set it to 'synchronous' to disable batch-splitting and ensure that TB processing remains within the 
calling thread without parallelization. |
| `max_pucch_concurrency` | integer | `0` |  | Maximum PUCCH processing concurrency for all cells.
Limits the maximum number of threads that can concurrently process Physical Uplink Control Channel
(PUCCH). Set it to zero for no limit of threads. |
| `max_pusch_and_srs_concurrency` | integer | `4` |  | Maximum PUSCH and SRS processing concurrency for all cells.
Limits the maximum number of threads that can concurrently process Physical Uplink Shared Channel 
(PUSCH) and Sounding Reference Signals (SRS). Set it to zero for no limitation. If hardware 
acceleration is enabled, this parameter is set to the number of the accelerator queues. |
| `max_pdsch_concurrency` | integer | `0` |  | Maximum concurrency level for PDSCH processing for all cells.
Limits the number of threads that can concurrently process Physical Downlink Shared Channel (PDSCH).
Set to zero for no limitation. If hardware acceleration is enabled, this parameter is set to the
number of the accelerator queues. |


#### ofh

Open Fronthaul thread configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_busy_waiting` | boolean | `false` |  | Enable busy waiting of the RU timing worker |


#### lower_phy

Lower PHY thread configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `execution_profile` | string | `triple` | enum: blocking, single, dual, triple | Lower physical layer executor profile [single, dual, triple].
 - single: one task worker for all the lower physical layer task executors.
 - dual: two task workers - one for the downlink and one for the uplink.
 - triple: dedicated task workers for each of the subtasks (demodulation, reception and transmission). |


### cell_affinities

Sets the cell CPU affinities configuration on a per cell basis

_List of objects with the following items:_


#### cell_affinities[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ru_cpus` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5" | CPU cores used for the Radio Unit tasks |
| `ru_pinning` | string | `mask` | one of: mask, round-robin | Policy used for assigning CPU cores to the Radio Unit tasks |


### cell_affinities

Sets the cell CPU affinities configuration on a per cell basis

_List of objects with the following items:_


#### cell_affinities[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ru_cpus` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5" | CPU cores used for the Radio Unit tasks |
| `ru_pinning` | string | `mask` | one of: mask, round-robin | Policy used for assigning CPU cores to the Radio Unit tasks |


## metrics

Metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_json` | boolean | `false` |  | Enable JSON metrics reporting |
| `enable_log` | boolean | `false` |  | Enable log metrics reporting |
| `enable_verbose` | boolean | `false` |  | Enable extended detail metrics reporting |


### layers

Layer basis metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_executor` | boolean | `false` |  | Whether to log application executors metrics |
| `enable_app_usage` | boolean | `false` |  | Enable application usage metrics |
| `enable_du_low` | boolean | `false` |  | Enable DU low metrics (upper physical layer) |
| `enable_ru` | boolean | `false` |  | Enable Radio Unit metrics |


### periodicity

Metrics periodicity configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `executors_report_period` | integer | `1000` | 0..10485760 | Executors metrics report period in milliseconds |
| `app_usage_report_period` | integer | `1000` |  | Application resource usage metrics report period (in milliseconds) |
| `du_report_period` | integer | `1000` | 0..10485760 | DU statistics report period in milliseconds |


## remote_control

Remote control configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enabled` | boolean | `false` |  | Enables the Remote Control Server |
| `bind_addr` | string | `127.0.0.1` |  | Remote Control Server bind address |
| `port` | integer | `8001` | 0..65535 | Port where the remote control server listens for incoming connections |


## expert_phy

Expert physical layer configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `max_proc_delay` | integer | `5` | 1..30 | Maximum allowed DL processing delay in slots. |
| `pusch_dec_max_iterations` | integer | `6` |  | Maximum number of PUSCH LDPC decoder iterations |
| `pusch_dec_enable_early_stop` | boolean | `true` |  | Enables PUSCH LDPC decoder early stop |
| `pusch_decoder_force_decoding` | boolean | `false` |  | Forces PUSCH LDPC decoder to decode always |
| `pusch_sinr_calc_method` | string | `post_equalization` | enum: channel_estimator, post_equalization, evm | PUSCH SINR calculation method: channel_estimator, post_equalization and evm. |
| `pusch_channel_estimator_fd_strategy` | string | `filter` | enum: filter, mean, none | PUSCH channel estimator frequency-domain smoothing strategy: filter, mean and none. |
| `pusch_channel_estimator_td_strategy` | string | `average` | enum: average, interpolate | PUSCH channel estimator time-domain strategy: average and interpolate. |
| `pusch_channel_estimator_cfo_compensation` | boolean | `true` |  | PUSCH channel estimator CFO compensation. |
| `pusch_channel_equalizer_algorithm` | string | `zf` | enum: zf, mmse | PUSCH channel equalizer algorithm: zf and mmse. |
| `max_request_headroom_slots` | integer | `0` | 0..30 | Maximum request headroom size in slots. |
| `allow_request_on_empty_uplink_slot` | boolean | `false` |  | Generates an uplink request in an uplink slot with no PUCCH/PUSCH/SRS PDUs |
| `enable_phy_tap` | boolean | `false` |  | Enables or disables the PHY tap plugin if it is present while building the application. |
| `phy_tap_arguments` | string | `` |  | PHY tap plugin argument string passed during construction. |


## ru_ofh

Open Fronthaul Radio Unit configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `gps_alpha` | integer | `0` | 0..12288000 | GPS Alpha |
| `gps_beta` | integer | `0` | -32768..32767 | GPS Beta |


### base_cell

Base cell parameters that propagate to each entry in --cells

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `t1a_max_cp_dl` | integer | `500` | 0..1960 | T1a maximum value for downlink Control-Plane |
| `t1a_min_cp_dl` | integer | `258` | 0..1960 | T1a minimum value for downlink Control-Plane |
| `t1a_max_cp_ul` | integer | `500` | 0..1960 | T1a maximum value for uplink Control-Plane |
| `t1a_min_cp_ul` | integer | `285` | 0..1960 | T1a minimum value for uplink Control-Plane |
| `t1a_max_up` | integer | `300` | 0..1960 | T1a maximum value for User-Plane |
| `t1a_min_up` | integer | `85` | 0..1960 | T1a minimum value for User-Plane |
| `ta4_max` | integer | `500` | 0..1960 | Ta4 maximum value for User-Plane |
| `ta4_min` | integer | `85` | 0..1960 | Ta4 minimum value for User-Plane |
| `is_prach_cp_enabled` | boolean | `true` |  | PRACH Control-Plane enabled flag |
| `ignore_ecpri_seq_id` | boolean | `false` |  | Ignore eCPRI sequence id field value |
| `ignore_ecpri_payload_size` | boolean | `false` |  | Ignore eCPRI payload size field value |
| `ignore_prach_start_symbol` | boolean | `false` |  | Ignore the start symbol field in the PRACH U-Plane packets |
| `log_lates_as_warnings` | boolean | `true` |  | Log late events as warnings |
| `warn_unreceived_ru_frames` | string | `after_traffic_detection` | enum: never, always, after_traffic_detection | Warn of unreceived Radio Unit frames |
| `compr_method_ul` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | Uplink compression method |
| `compr_bitwidth_ul` | integer | `9` | 1..16 | Uplink compression bit width |
| `compr_method_dl` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | Downlink compression method |
| `compr_bitwidth_dl` | integer | `9` | 1..16 | Downlink compression bit width |
| `compr_method_prach` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | PRACH compression method |
| `compr_bitwidth_prach` | integer | `9` | 1..16 | PRACH compression bit width |
| `enable_ul_static_compr_hdr` | boolean | `true` |  | Uplink static compression header enabled flag |
| `enable_dl_static_compr_hdr` | boolean | `true` |  | Downlink static compression header enabled flag |


### cells

Sets the cell configuration on a per cell basis, overwriting the default configuration defined by cell_cfg

_List of objects with the following items:_


#### cells[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `t1a_max_cp_dl` | integer | `500` | 0..1960 | T1a maximum value for downlink Control-Plane |
| `t1a_min_cp_dl` | integer | `258` | 0..1960 | T1a minimum value for downlink Control-Plane |
| `t1a_max_cp_ul` | integer | `500` | 0..1960 | T1a maximum value for uplink Control-Plane |
| `t1a_min_cp_ul` | integer | `285` | 0..1960 | T1a minimum value for uplink Control-Plane |
| `t1a_max_up` | integer | `300` | 0..1960 | T1a maximum value for User-Plane |
| `t1a_min_up` | integer | `85` | 0..1960 | T1a minimum value for User-Plane |
| `ta4_max` | integer | `500` | 0..1960 | Ta4 maximum value for User-Plane |
| `ta4_min` | integer | `85` | 0..1960 | Ta4 minimum value for User-Plane |
| `is_prach_cp_enabled` | boolean | `true` |  | PRACH Control-Plane enabled flag |
| `ignore_ecpri_seq_id` | boolean | `false` |  | Ignore eCPRI sequence id field value |
| `ignore_ecpri_payload_size` | boolean | `false` |  | Ignore eCPRI payload size field value |
| `ignore_prach_start_symbol` | boolean | `false` |  | Ignore the start symbol field in the PRACH U-Plane packets |
| `log_lates_as_warnings` | boolean | `true` |  | Log late events as warnings |
| `warn_unreceived_ru_frames` | string | `after_traffic_detection` | enum: never, always, after_traffic_detection | Warn of unreceived Radio Unit frames |
| `compr_method_ul` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | Uplink compression method |
| `compr_bitwidth_ul` | integer | `9` | 1..16 | Uplink compression bit width |
| `compr_method_dl` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | Downlink compression method |
| `compr_bitwidth_dl` | integer | `9` | 1..16 | Downlink compression bit width |
| `compr_method_prach` | string | `bfp` | enum: none, bfp, bfp selective, block scaling, mu law, modulation, modulation selective | PRACH compression method |
| `compr_bitwidth_prach` | integer | `9` | 1..16 | PRACH compression bit width |
| `enable_ul_static_compr_hdr` | boolean | `true` |  | Uplink static compression header enabled flag |
| `enable_dl_static_compr_hdr` | boolean | `true` |  | Downlink static compression header enabled flag |
| `network_interface` | string | `enp1s0f0` |  | Network interface name for raw sockets. PCIe (or other bus) port identifier when using DPDK |
| `enable_promiscuous` | boolean | `false` |  | Promiscuous mode flag |
| `check_link_status` | boolean | `false` |  | Ethernet link status checking flag |
| `ru_mac_addr` | string | `70:b3:d5:e1:5b:06` |  | Radio Unit MAC address |
| `du_mac_addr` | string | `00:11:22:33:00:77` |  | Distributed Unit MAC address |
| `vlan_tag_cp` | integer |  | 1..4094 | C-Plane VLAN identifier |
| `vlan_tag_up` | integer |  | 1..4094 | U-Plane VLAN identifier |
| `vlan_pcp_cp` | integer |  | 0..7; requires --vlan_tag_cp to be set as well | C-Plane VLAN PCP |
| `vlan_pcp_up` | integer |  | 0..7; requires --vlan_tag_up to be set as well | U-Plane VLAN PCP |
| `prach_port_id` | array of integer | `[4]` |  | RU PRACH port identifier |
| `dl_port_id` | array of integer | `[0]` |  | RU downlink port identifier |
| `ul_port_id` | array of integer | `[0]` |  | RU uplink port identifier |


## ru_sdr

SDR Radio Unit configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `srate` | number | `61.44` |  | Sample rate in MHz |
| `device_driver` | string | `uhd` |  | Device driver name |
| `device_args` | string | `` |  | Optional device arguments |
| `tx_gain` | number | `50` |  | Transmit gain in decibels |
| `rx_gain` | number | `60` |  | Receive gain in decibels |
| `freq_offset` | number | `0` |  | Center frequency offset in hertz |
| `clock_ppm` | number | `0` |  | Clock calibration in PPM. |
| `lo_offset` | number | `0` |  | LO frequency offset in MHz |
| `clock` | string | `default` |  | Clock source |
| `sync` | string | `default` |  | Time synchronization source |
| `otw_format` | string | `default` |  | Over-the-wire format |
| `time_alignment_calibration` | string | `auto` | a signed integer or the sentinel "auto" (skip calibration) | Rx to Tx radio time alignment calibration in samples.
Positive values reduce the RF transmission delay with respect
to the RF reception, while negative values increase it |
| `dl_freq_Hz` | number |  |  | Downlink frequency in Hz. If present, it overrides the one derived by DL ARFCN and NR Band. |
| `ul_freq_Hz` | number |  |  | Uplink frequency in Hz. If present, it overrides the one derived by UL ARFCN and NR Band. |


### amplitude_control

Amplitude control parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `tx_gain_backoff` | number | `12` |  | Gain back-off to accommodate the signal PAPR in decibels |
| `enable_clipping` | boolean | `false` |  | Signal clipping |
| `ceiling` | number | `-0.1` |  | Clipping ceiling referenced to full scale |


### expert_cfg

Generic Radio Unit expert configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `low_phy_dl_throttling` | number | `0` |  | System time-based throttling.
Determines a minimum baseband processor period time between downlink packets. It is expressed as a 
fraction of the time equivalent to the number of samples in the baseband buffer. Set to 0.9 to ensure 
that the downlink packets are processed with a minimum period of 90% of the buffer duration.
Set to zero to disable this feature. |
| `tx_mode` | string | `continuous` | enum: continuous, discontinuous, same-port | Selects a radio transmission mode. Discontinuous modes are not supported by all radios.
  continuous:    the TX chain is always active.
  discontinuous: the transmitter stops when there is no data to transmit.
  same-port:     the radio transmits and receives from the same antenna port.
 |
| `power_ramping_time_us` | number | `0` |  | Specifies the power ramping time in microseconds, it proactively initiates the transmission and 
mitigates transient effects. |


