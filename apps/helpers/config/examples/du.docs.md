# DU configuration reference

## Reusable types

### <a id="types-log-level"></a>`log-level`

- Type: string
- Constraints: enum: none, error, warning, info, debug

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dryrun` | boolean | `false` |  | Enable application dry run mode |
| `gnb_id` | integer | `411` |  | gNodeB identifier |
| `gnb_id_bit_length` | integer | `22` | 22..32 | gNodeB identifier length in bits |
| `gnb_du_id` | integer | `0` | 0..68719476735 | gNB-DU Id |


## log

Logging configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `/tmp/du.log` |  | Log file output path |
| `all_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Default log level for PHY, MAC, RLC, PDCP, RRC, SDAP, NGAP and GTPU |
| `lib_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Generic log level |
| `e2ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | E2AP log level |
| `config_level` | [`log-level`](#types-log-level) | `none` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Config log level |
| `hex_max_size` | integer | `0` | -1..1024 | Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes) |
| `mac_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | MAC log level |
| `rlc_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | RLC log level |
| `f1ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | F1AP log level |
| `f1u_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | F1-U log level |
| `gtpu_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | GTPU log level |
| `ntn_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | NTN log level |
| `du_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Log level for the DU |
| `broadcast_enabled` | boolean | `false` |  | Enable logging in the physical and MAC layer of broadcast messages and all PRACH opportunities |
| `f1ap_json_enabled` | boolean | `false` |  | Enable JSON logging of F1AP PDUs |
| `high_latency_diagnostics_enabled` | boolean | `false` |  | Log performance diagnostics when high computational latencies are detected |
| `fapi_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | FAPI log level |
| `phy_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | PHY log level |
| `hal_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | HAL log level |
| `phy_rx_symbols_filename` | string | `` |  | Set to a valid file path to print the received symbols. |
| `phy_rx_symbols_port` | string | `0` | a non-negative port number or the sentinel "all" | Set to a valid receive port number to dump the IQ symbols from that port only, or set to "all" to dump the IQ symbols from all UL receive ports. Only works if "phy_rx_symbols_filename" is set. |
| `phy_rx_symbols_prach` | boolean | `false` |  | Set to true to dump the IQ symbols from all the PRACH ports. Only works if "phy_rx_symbols_filename" is set. |
| `ofh_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Open Fronthaul log level |
| `radio_level` | [`log-level`](#types-log-level) | `info` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Radio log level |


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
| `du_high_enable` | boolean | `false` |  | Enable tracing for DU-high executors |
| `phy_enable` | boolean | `false` |  | Enable tracing for physical layer executors |


## buffer_pool

Buffer pool configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_segments` | integer | `1048576` |  | Number of segments allocated by the buffer pool |
| `segment_size` | integer | `2048` |  | Size of each buffer pool segment in bytes |


## metrics

Metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_json` | boolean | `false` |  | Enable JSON metrics reporting |
| `enable_log` | boolean | `false` |  | Enable log metrics reporting |
| `enable_verbose` | boolean | `false` |  | Enable extended detail metrics reporting |
| `autostart_stdout_metrics` | boolean | `false` |  | Autostart stdout metrics reporting |


### layers

Layer basis metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_app_usage` | boolean | `false` |  | Enable application usage metrics |
| `enable_sched` | boolean | `true` |  | Enable DU scheduler metrics |
| `enable_rlc` | boolean | `false` |  | Enable RLC metrics |
| `enable_mac` | boolean | `false` |  | Enable MAC metrics |
| `enable_du_proc` | boolean | `false` |  | Enable DU management and control procedure metrics |
| `enable_du_low` | boolean | `false` |  | Enable DU low metrics (upper physical layer) |
| `enable_ru` | boolean | `false` |  | Enable Radio Unit metrics |


### periodicity

Metrics periodicity configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `app_usage_report_period` | integer | `1000` |  | Application resource usage metrics report period (in milliseconds) |
| `du_report_period` | integer | `1000` | 0..10485760 | DU statistics report period in milliseconds |


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


### queues

Task executor queue parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `du_ue_data_executor_queue_size` | integer | `8192` |  | DU's UE executor task queue size for PDU processing |


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


### cell_affinities

Sets the cell CPU affinities configuration on a per cell basis

_List of objects with the following items:_


#### cell_affinities[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ru_cpus` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5" | CPU cores used for the Radio Unit tasks |
| `ru_pinning` | string | `mask` | one of: mask, round-robin | Policy used for assigning CPU cores to the Radio Unit tasks |


## f1ap

F1AP interface configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `addrs` | array of string | `[127.0.10.1]` |  | CU-CP F1-C addresses to connect to. Multiple addresses can be specified for SCTP multi-homing |
| `bind_addrs` | array of string | `[127.0.10.2]` |  | DU F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |


## f1u

F1-U interface configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `queue_size` | integer | `2048` |  | F1-U PDU queue size |
| `bind_port` | integer | `2152` |  | F1-U bind port |
| `peer_port` | integer | `2152` |  | F1-U peer port |


### socket

Configures UDP/IP socket parameters of the F1-U interface

_List of objects with the following items:_


#### socket[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addr` | string | `127.0.10.1` | must be a valid IPv4 address | Default local IP address interfaces bind to, unless a specific bind address is specified |
| `sst` | integer |  | 0..255 | Slice Service Type |
| `sd` | integer |  | 0..16777214 | Service Differentiator |
| `five_qi` | integer |  | 0..255 | Assign this socket to a specific 5QI |
| `ext_addr` | string | `auto` | must be a valid IPv4 address or "auto" | External IP address that is advertised for receiving UDP packets. |


##### udp

UDP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `max_rx_msgs` | integer | `256` |  | Maximum amount of messages RX in a single syscall |
| `tx_qsize` | integer | `4096` |  | Size of TX queue used for batching SDUs. |
| `max_tx_msgs` | integer | `256` |  | Maximum amount of messages TX in a single syscall |
| `max_tx_segments` | integer | `256` |  | Maximum amount of segments TX in a single SDU |
| `pool_threshold` | number | `0.9` |  | Pool accupancy threshold after which packets are dropped |
| `reuse_addr` | boolean | `false` |  | Allow multiple sockets to bind to the same port. |
| `dscp` | integer |  | 0..63 | Differentiated Services Code Point value. |


## remote_control

Remote control configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enabled` | boolean | `false` |  | Enables the Remote Control Server |
| `bind_addr` | string | `127.0.0.1` |  | Remote Control Server bind address |
| `port` | integer | `8001` | 0..65535 | Port where the remote control server listens for incoming connections |


## pcap

PCAP configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `f1ap_filename` | string | `/tmp/du_f1ap.pcap` |  | F1AP PCAP file output path |
| `f1ap_enable` | boolean | `false` |  | Enable F1AP packet capture |
| `f1u_filename` | string | `/tmp/du_f1u.pcap` |  | F1-U PCAP file output path |
| `f1u_enable` | boolean | `false` |  | Enable F1-U packet capture |
| `rlc_filename` | string | `/tmp/du_rlc.pcap` |  | RLC PCAP file output path |
| `rlc_rb_type` | string | `all` |  | RLC PCAP RB type (all, srb, drb) |
| `rlc_enable` | boolean | `false` |  | Enable RLC packet capture |
| `mac_filename` | string | `/tmp/du_mac.pcap` |  | MAC PCAP file output path |
| `mac_type` | string | `udp` |  | MAC PCAP pcap type (dlt or udp) |
| `mac_enable` | boolean | `false` |  | Enable MAC packet capture |
| `e2ap_du_filename` | string | `/tmp/du_e2ap.pcap` |  | E2AP PCAP file output path |
| `e2ap_enable` | boolean | `false` |  | Enable E2AP packet capture |


## cell_cfg

Default cell configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pci` | integer | `1` | 0..1007 | PCI |
| `sector_id` | integer |  | 0..16383 | Sector ID (4-14 bits). This value is concatenated with the gNB Id to form the NR Cell Identity (NCI). If not specified, a unique value for the DU is automatically derived |
| `dl_arfcn` | string | `536020` | non-negative integer; valid range [0, 3279165] | Downlink ARFCN |
| `band` | integer | `auto` | set to "auto" to auto-derive | NR band |
| `common_scs` | string | `15kHz` | accepts SCS strings (e.g. "15kHz", "30kHz") | Cell common subcarrier spacing |
| `channel_bandwidth_MHz` | integer | `20` | legal values: {5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100, 200, 400} | Channel bandwidth in MHz |
| `nof_antennas_ul` | integer | `1` |  | Number of antennas in uplink |
| `nof_antennas_dl` | integer | `1` |  | Number of antennas in downlink |
| `plmn` | string | `00101` | pattern: `[0-9]{5,6}` | PLMN |
| `additional_plmns` | array of string | `[]` | each element must be a valid PLMN string | List of PLMNs |
| `tac` | integer | `7` | values 0 and 0xfffffe are reserved; value must fit in 24 bits | TAC |
| `enabled` | boolean | `true` |  | Automatically activate the cell on startup |
| `cell_barred` | boolean | `false` |  | MIB cellBarred: if true, UEs cannot camp on this cell |
| `intra_freq_reselection` | boolean | `true` |  | MIB intraFreqReselection: if true, intra-frequency cell reselection is allowed when cell is barred |
| `q_rx_lev_min` | integer | `-70` | -70..-22 | q-RxLevMin, required minimum received RSRP level for cell selection/re-selection, in dBm |
| `q_qual_min` | integer | `-20` | -43..-12 | q-QualMin, required minimum received RSRQ level for cell selection/re-selection, in dB |
| `pcg_p_nr_fr1` | integer |  | -30..23 | p-nr-fr1, maximum total TX power to be used by the UE in this NR cell group across in FR1 |


### mac_cell_group

MAC Cell Group parameters


#### bsr_cfg

Buffer status report configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `periodic_bsr_timer` | integer | `10` | legal values: {1, 5, 10, 16, 20, 32, 40, 64, 80, 128, 160, 320, 640, 1280, 2560, 0} | Periodic Buffer Status Report Timer value in nof. subframes. Value 0 equates to infinity |
| `retx_bsr_timer` | integer | `80` | legal values: {10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240} | Retransmission Buffer Status Report Timer value in nof. subframes |
| `lc_sr_delay_timer` | integer |  | legal values: {20, 40, 64, 128, 512, 1024, 2560} | Logical Channel SR delay timer in nof. subframes |


#### phr_cfg

Power Headroom report configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `phr_prohibit_timer` | integer | `10` | legal values: {0, 10, 20, 50, 100, 200, 500, 1000} | PHR prohibit timer in nof. subframes |


#### sr_cfg

Scheduling Request configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sr_trans_max` | integer | `64` | legal values: {4, 8, 16, 32, 64} | Maximum number of SR transmissions |
| `sr_prohibit_timer` | integer |  | legal values: {1, 2, 4, 8, 16, 32, 64, 128} | Timer for SR transmission on PUCCH in ms |


### ssb

SSB parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ssb_period` | integer | `10` | legal values: {5, 10, 20} | Period of SSB scheduling in milliseconds |
| `ssb_block_power_dbm` | integer | `-16` | -60..50 | SS_PBCH_power_block in dBm |
| `pss_to_sss_epre` | string | `0` | enum: 0, 3; legacy CLI11 accepted only the integer values 0 and 3; the builder API maps them to the string keys "0" and "3". | SSB PSS to SSS EPRE ratio in dB {0, 3} |


### sib

SIB configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `si_window_length` | integer | `160` | legal values: {5, 10, 20, 40, 80, 160, 320, 640, 1280} | The length of the SI scheduling window, in slots. It must be always shorter or equal to the period of the SI message. |
| `t300` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Establishment timer in ms. The timer starts upon transmission of RRCSetupRequest. |
| `t301` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Re-establishment timer in ms. The timer starts upon transmission of RRCReestablishmentRequest. |
| `t310` | integer | `1000` | legal values: {0, 50, 100, 200, 500, 1000, 2000} | Out-of-sync timer in ms. The timer starts upon detecting physical layer problems for the SpCell i.e. upon receiving N310 consecutive out-of-sync indications from lower layers. |
| `n310` | integer | `1` | legal values: {1, 2, 3, 4, 6, 8, 10, 20} | Out-of-sync counter. The counter is increased upon reception of "out-of-sync" from lower layer while the timer T310 is stopped. Starts the timer T310, when configured value is reached. |
| `t311` | integer | `3000` | legal values: {1000, 3000, 5000, 10000, 15000, 20000, 30000} | RRC Connection Re-establishment procedure timer in ms. The timer starts upon initiating the RRC connection re-establishment procedure. |
| `n311` | integer | `1` | legal values: {1, 2, 3, 4, 5, 6, 8, 10} | In-sync counter. The counter is increased upon reception of the "in-sync" from lower layer while the timer T310 is running. Stops the timer T310, when configured value is reached. |
| `t319` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Resume timer in ms. The timer starts upon transmission of RRCResumeRequest or RRCResumeRequest1. |


#### si_sched_info

Configures the scheduling for each of the SI-messages broadcast by the gNB

_List of objects with the following items:_


##### si_sched_info[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `si_period` | integer | `32` | legal values: {8, 16, 32, 64, 128, 256, 512} | SI message scheduling period in radio frames |
| `sib_mapping` | array of integer | `[]` | each element must be in {2, 3, 4, 5, 6, 7, 8, 19} | Mapping of SIB types to SI-messages. SIB numbers should not be repeated |
| `si_window_position` | integer |  | 1..256 | SI window position of the associated SI-message |


#### sib2

SIB2 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `q_hyst` | integer | `3` | legal values: {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | Hysteresis value for ranking criteria. |
| `thresh_serving_low_p` | integer | `0` | 0..31 | Rx level threshold used by the UE on the serving cell when reselecting towards a lower priority RAT/frequency. |
| `cell_reselection_priority` | integer | `6` | 0..7 | Integer part of the cell reselection priority for the frequency of this cell |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `s_intra_search_p` | integer | `62` | must be an even value within [0, 62] | Rx level threshold for intra frequency measurements in dB |
| `t_reselection_nr` | integer | `1` | 0..7 | Cell reselection timer value in seconds |


#### sib3

SIB3 parameters


##### intra_freq_neigh_cell_list

Intra frequency neighbor cell list

_List of objects with the following items:_


###### intra_freq_neigh_cell_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pci` | integer | `0` | 0..1007 | PCI |
| `q_offset_cell` | integer | `0` | legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | PCI |


##### intra_freq_excluded_cell_list

Intra frequency excluded cell list

_List of objects with the following items:_


###### intra_freq_excluded_cell_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `start` | integer | `0` | 0..1007; **required** | Range start |
| `size` | integer | `1` | legal values: {1, 4, 8, 12, 16, 24, 32, 48, 64, 84, 96, 128, 168, 252, 504, 1008}; **required** | Range size |


#### sib4

SIB4 parameters


##### inter_freq_carrier_freq_list

Inter frequency carrier frequency list

_List of objects with the following items:_


###### inter_freq_carrier_freq_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `arfcn` | integer | `0` |  | ARFCN |
| `ssb_scs` | string | `15kHz` | accepts SCS strings (e.g. "15kHz", "30kHz") | SSB subcarrier spacing |
| `derive_ssb_index_from_cell` | boolean | `false` |  | Derive SSB index from cell |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `thresh_x_high_p` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB |
| `thresh_x_low_p` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB |
| `q_offset_freq` | integer | `0` | legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | Frequency specific offset in dB for equal priority NR frequencies. |


#### sib5

SIB5 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `t_reselection_eutra` | integer | `0` | 0..7 | Cell reselection timer value in seconds |


##### carrier_freq_list_eutra

EUTRA carrier frequency list

_List of objects with the following items:_


###### carrier_freq_list_eutra[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `earfcn` | integer | `0` |  | EARFCN |
| `allowed_meas_bandwidth` | integer | `0` | legal values: {6, 15, 25, 50, 75, 100} | Allowed measurement bandwidth |
| `presence_antenna_port1` | boolean | `false` |  | Whether all neighbor cells use Antenna Port 1 |
| `cell_reselection_priority` | integer |  | 0..7 | Integer part of the cell reselection priority for the frequency of this cell |
| `thresh_x_high` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB |
| `thresh_x_low` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `q_qual_min` | integer | `-34` | -34..-3 | Minimum required quality level in the cell in dB |
| `p_max_eutra` | integer | `33` | -30..33 | Maximum allowed transmission power in dBm on the (uplink) carrier frequency. |


#### sib16

SIB16 parameters


##### freq_prio_list_slicing

Frequency priority slicing list entries

_List of objects with the following items:_


###### freq_prio_list_slicing[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_implicit_carrier_freq` | integer | `0` | 0..8 | DL implicit carrier frequency index for this slicing entry |


###### slice_info_list

Slice info list entries

_List of objects with the following items:_


###### slice_info_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nsag_id` | integer | `0` | 0..255 | NSAG ID |
| `allowed` | boolean | `true` |  | Whether the list of cells in this slice info is allowed (true) or excluded (false) |
| `reselection_priority` | number | `0` | must be a multiple of 0.2 within [0.0, 7.8] | Priority associated with this cell reselection slice |


###### cells_allowed

Slice cell list entries

_List of objects with the following items:_


###### cells_allowed[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `start` | integer | `0` | 0..1007; **required** | Range start |
| `size` | integer | `1` | legal values: {1, 4, 8, 12, 16, 24, 32, 48, 64, 84, 96, 128, 168, 252, 504, 1008}; **required** | Range size |


#### etws

ETWS configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `message_id` | integer | `4356` | 0..65535 | ETWS message ID. |
| `serial_num` | integer | `12288` | 0..65535 | ETWS message serial number. |
| `warning_type` | integer | `2432` | 0..65535 | ETWS warning type. |
| `data_coding_scheme` | integer | `0` | 0..255 | ETWS message CBS coding scheme. |
| `warning_message` | string |  |  | ETWS warning message. Max. Length and character support depends on the chosen coding scheme. |


#### cmas

CMAS configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `message_id` | integer | `4370` | 0..65535 | CMAS message ID. |
| `serial_num` | integer | `12291` | 0..65535 | CMAS message serial number. |
| `data_coding_scheme` | integer | `0` | 0..255 | CMAS message CBS coding scheme. |
| `warning_message` | string | `` |  | CMAS warning message. Max. Length and character support depends on the chosen coding scheme. |


### ul_common

UL common parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `p_max` | integer |  | -30..23 | Maximum transmit power allowed in this serving cell |
| `max_pucchs_per_slot` | integer | `31` | 1..128 | Maximum number of PUCCH grants that can be allocated per slot |
| `max_ul_grants_per_slot` | integer | `32` | 1..144 | Maximum number of UL grants that can be allocated per slot |
| `min_pucch_pusch_prb_distance` | integer | `1` | 0..137 | Minimum PRB distance between PUCCH and UE-dedicated PUSCH grants |


### pdcch

PDCCH parameters


#### common

PDCCH Common configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `coreset0_index` | integer |  | 0..15 | CORESET#0 index |
| `ss1_n_candidates` | string | `[0,0,1,0,0]` | 5-element list of uint8_t (e.g. "[0,0,1,0,0]"). Each element must be in {0,1,2,3,4,5,6,8}.; legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string. | Number of PDCCH candidates per aggregation level for SearchSpace#1. Default: {0, 0, 1, 0, 0} |
| `ss0_index` | integer | `0` | 0..15 | SearchSpace#0 index |
| `max_coreset0_duration` | integer |  | 1..2 | Maximum CORESET#0 duration in OFDM symbols to consider when deriving CORESET#0 index |


#### dedicated

PDCCH Dedicated configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `coreset1_rb_start` | integer |  | 0..275 | Starting Common Resource Block (CRB) number for CORESET 1 relative to CRB 0. Default: CRB0 |
| `coreset1_l_crb` | integer |  | 0..275 | Length of CORESET 1 in number of CRBs. Default: Across entire BW of cell |
| `coreset1_duration` | integer |  | 1..2 | Duration of CORESET 1 in number of OFDM symbols. Default: Max(2, Nof. CORESET#0 symbols) |
| `ss2_n_candidates` | string | `[0,0,0,0,0]` | 5-element list of uint8_t (e.g. "[0,0,0,0,0]"). Each element must be in {0,1,2,3,4,5,6,8}.; legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string. | Number of PDCCH candidates per aggregation level for SearchSpace#2. Default: {0, 0, 0, 0, 0} i.e. auto-compute nof. candidates |
| `dci_format_0_1_and_1_1` | boolean | `true` |  | DCI format to use in UE dedicated SearchSpace#2 |
| `ss2_type` | string | `ue_dedicated` | enum: common, ue_dedicated | SearchSpace type for UE dedicated SearchSpace#2 |
| `al_cqi_offset` | number | `0` | -15..15 | Offset to apply to the CQI value used in the PDCCH aggregation level calculation. |


### pdsch

PDSCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_ue_mcs` | integer | `0` | 0..28 | Minimum UE MCS |
| `max_ue_mcs` | integer | `28` | 0..28 | Maximum UE MCS |
| `fixed_rar_mcs` | integer | `0` | 0..28 | Fixed RAR MCS |
| `fixed_sib1_mcs` | integer | `5` | 0..28 | Fixed SIB1 MCS |
| `harq_feedback_disabled` | string | `false` | accepts "true", "false" or a 32-bit bitmap ("0x..." / "0b..."). | Disable DL HARQ Feedback (only for NTN cells).
If set to true, applies the mask 0x0fffffff and disables HARQ feedback for all except the first four HARQs.
If set to a string, it must be a 32-bit bitmap (0x… or 0b…) of the HARQ processes to disable.
The bit set to 1 indicates HARQ processes with disabled DL HARQ feedback and the bit set to 0,
identify HARQ processes with enabled DL HARQ feedback.The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are ignored.
 |
| `nof_harqs` | integer | `16` | legal values: {2, 4, 6, 8, 10, 12, 16, 32} | Number of DL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; otherwise, it defaults to 16. |
| `max_nof_harq_retxs` | integer | `4` | 0..64 | Maximum number of times a DL HARQ can be retransmitted, before it gets discarded. |
| `harq_retx_timeout` | integer | `100` | 10..500 | Maximum time, in milliseconds, between a HARQ NACK and the scheduler allocating the respective HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of HARQ-ACK consecutive KOs before an Radio Link Failure is reported |
| `rv_sequence` | array of integer | `[0, 2, 3, 1]` | each element must be in {0, 1, 2, 3} | RV sequence for PUSCH. (e.g. [0 2 3 1] |
| `mcs_table` | string | `qam256` | enum: qam64, qam256, qam64lowse | MCS table to use PDSCH |
| `min_rb_size` | integer | `1` | 1..275 | Minimum RB size for UE PDSCH resource allocation |
| `max_rb_size` | integer | `275` | 1..275 | Maximum RB size for UE PDSCH resource allocation |
| `start_rb` | integer | `0` | 0..275 | Start RB for resource allocation of UE PDSCHs |
| `end_rb` | integer | `275` | 0..275 | End RB for resource allocation of UE PDSCHs |
| `max_pdschs_per_slot` | integer | `35` | 1..35 | Maximum number of PDSCH grants per slot, including SIB, RAR, Paging and UE data grants. |
| `max_alloc_attempts` | integer | `35` | 1..35 | Maximum number of DL or UL PDCCH grant allocation attempts per slot before scheduler skips the slot |
| `olla_cqi_inc_step` | number | `0.001` | 0..1 | Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled |
| `olla_target_bler` | number | `0.01` | 0..0.5 | Target DL BLER set in Outer-loop link adaptation (OLLA) algorithm |
| `olla_max_cqi_offset` | number | `4` | ≥ 0 | Maximum offset that the Outer-loop link adaptation (OLLA) can apply to CQI |
| `dc_offset` | string | `center` | accepts an integer in [min,max] or one of {"outside","undetermined","center"}. | Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for carrier spacing, and the center of the gNB DL carrier as DC offset value 0. The user can additionally set "outside" to define that the DC offset falls outside the DL carrier or "undetermined" in the case the DC offset is unknown. |
| `harq_la_cqi_drop_threshold` | integer | `3` | 0..15 | Link Adaptation (LA) threshold for drop in CQI of the first HARQ transmission above which HARQ retransmissions are cancelled. Set this value to 0 to disable this feature |
| `harq_la_ri_drop_threshold` | integer | `1` | 0..4 | Link Adaptation (LA) threshold for drop in nof. layers of the first HARQ transmission above which HARQ retransmission is cancelled. Set this value to 0 to disable this feature |
| `dmrs_additional_position` | integer | `2` | 0..3 | PDSCH DMRS additional position |
| `interleaving_bundle_size` | integer | `0` | legal values: {0, 2, 4} | PDSCH interleaving bundle size. Valid values: [0, 2, 4] |
| `max_rank` | integer |  | ≥ 0 | Maximum number of PDSCH transmission layers. The actual maximum is limited by the number of DL antennas. |
| `enable_csi_rs_pdsch_multiplexing` | boolean | `true` |  | Enable multiplexing of CSI-RS and PDSCH |


### pusch

PUSCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_ue_mcs` | integer | `0` | 0..28 | Minimum UE MCS |
| `max_ue_mcs` | integer | `28` | 0..28 | Maximum UE MCS |
| `harq_mode_b` | string | `false` | accepts "true", "false" or a 32-bit bitmap ("0x..." / "0b..."). | Set HARQ Mode B (only for NTN cells).
If set to true, applies the mask 0x0fffffff to set HARQ Mode B for all except the first four HARQ processes.
If set to a string, it must be a 32-bit bitmap (0x… or 0b…) indicating which HARQ processes use Mode B.
A bit set to 1 indicates HARQ Mode B; a bit set to 0 indicates HARQ Mode A.
The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are ignored.
 |
| `nof_harqs` | integer | `16` | legal values: {16, 32} | Number of UL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; otherwise, it defaults to 16. |
| `max_nof_harq_retxs` | integer | `4` | 0..64 | Maximum number of times a UL HARQ can be retransmitted, before it gets discarded. |
| `harq_retx_timeout` | integer | `100` | 10..500 | Maximum time, in milliseconds, between a CRC=KO and the scheduler allocating the respective HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of CRC consecutive KOs before an Radio Link Failure is reported |
| `rv_sequence` | array of integer | `[0]` | each element must be in {0, 1, 2, 3} | RV sequence for PUSCH. (e.g. [0 2 3 1] |
| `mcs_table` | string | `qam256` | enum: qam64, qam256, qam64lowse | MCS table to use PUSCH |
| `max_rank` | integer | `4` | 1..4 | Maximum number of PUSCH transmission layers. The actual maximum is limited by the number of receive ports and UE capabilities. |
| `msg3_delta_preamble` | integer | `6` | -1..6 | msg3-DeltaPreamble, Power offset between msg3 and RACH preamble transmission |
| `p0_nominal_with_grant` | integer | `-76` | must be a multiple of 2 within [-202, 24] | P0 value for PUSCH with grant (except msg3). Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval.  Default: -76 |
| `msg3_delta_power` | integer | `8` | must be a multiple of 2 within [-6, 8] | Target power level at the network receiver side, in dBm. Valid values must be multiple of 2 and within the [-6, 8] interval. Default: 8 |
| `max_puschs_per_slot` | integer | `16` | 1..16 | Maximum number of PUSCH grants per slot |
| `beta_offset_ack_idx_1` | integer | `11` | 0..31 | betaOffsetACK-Index1 part of UCI-OnPUSCH |
| `beta_offset_ack_idx_2` | integer | `6` | 0..31 | betaOffsetACK-Index2 part of UCI-OnPUSCH |
| `beta_offset_ack_idx_3` | integer | `4` | 0..31 | betaOffsetACK-Index3 part of UCI-OnPUSCH |
| `beta_offset_csi_p1_idx_1` | integer | `13` | 0..31 | betaOffsetCSI-Part1-Index1 part of UCI-OnPUSCH |
| `beta_offset_csi_p1_idx_2` | integer | `10` | 0..31 | betaOffsetCSI-Part1-Index2 part of UCI-OnPUSCH |
| `beta_offset_csi_p2_idx_1` | integer | `13` | 0..31 | betaOffsetCSI-Part2-Index1 part of UCI-OnPUSCH |
| `beta_offset_csi_p2_idx_2` | integer | `10` | 0..31 | betaOffsetCSI-Part2-Index2 part of UCI-OnPUSCH |
| `min_k2` | integer | `4` | 1..4 | Minimum value of K2 (difference in slots between PDCCH and PUSCH). |
| `dc_offset` | string | `center` | accepts an integer in [min,max] or one of {"outside","undetermined","center"}. | Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for carrier spacing, and the center of the gNB UL carrier as DC offset value 0. The user can additionally set "outside" to define that the DC offset falls outside the UL carrier or "undetermined" in the case the DC offset is unknown. |
| `olla_snr_inc_step` | number | `0.001` | 0..1 | Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled |
| `olla_target_bler` | number | `0.01` | 0..0.5 | Target UL BLER set in Outer-loop link adaptation (OLLA) algorithm |
| `olla_max_snr_offset` | number | `5` | ≥ 0 | Maximum offset that the Outer-loop link adaptation (OLLA) can apply to the estimated UL SINR |
| `dmrs_additional_position` | integer | `2` | 0..3 | PUSCH DMRS additional position |
| `min_rb_size` | integer | `1` | 1..275 | Minimum RB size for UE PUSCH resource allocation |
| `max_rb_size` | integer | `275` | 1..275 | Maximum RB size for UE PUSCH resource allocation |
| `start_rb` | integer | `0` | 0..275 | Start RB for resource allocation of UE PUSCHs |
| `end_rb` | integer | `275` | 0..275 | End RB for resource allocation of UE PUSCHs |
| `enable_cl_loop_pw_control` | boolean | `false` |  | Enable closed-loop power control for PUSCH |
| `enable_phr_bw_adaptation` | boolean | `false` |  | Enable bandwidth adaptation to prevent negative PHR |
| `target_sinr` | number | `10` | -5..30 | Target PUSCH SINR in dB |
| `ref_path_loss` | number | `70` | 50..120 | Reference path-loss for target PUSCH SINR in dB |
| `pl_compensation_factor` | number | `1` | legal values: {0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0} | Fractional path-loss compensation factor in PUSCH power control |
| `enable_transform_precoding` | boolean | `false` |  | Enable transform precoding for PUSCH. |


### pucch

PUCCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `p0_nominal` | integer | `-90` | must be a multiple of 2 within [-202, 24] | Power control parameter P0 for PUCCH transmissions. Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval. Default: -90 |
| `pucch_resource_common` | integer |  | 0..15 | Index of PUCCH resource set for the common configuration |
| `sr_period_ms` | number | `20` | legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 40, 80, 160, 320} | SR period in msec |
| `formats` | string | `f1_and_f2` | enum: f0_and_f2, f1_and_f2, f1_and_f3, f1_and_f4 | PUCCH formats combination to use. Values: {f0_and_f2, f1_and_f2, f1_and_f3, f1_and_f4}. Default: f1_and_f2 |
| `resource_set_size` | integer | `8` | 1..8 | Number of PUCCH resources in each PUCCH resource set |
| `nof_cell_res_set_configs` | integer | `2` | 1..10 | Number of PUCCH Resource Set configurations that are available per cell. NOTE: the higher the number of configurations, the lower the chances UEs have to share the same PUCCH resources for HARQ-ACK. |
| `nof_cell_sr_res` | integer | `8` | 1..100 | Number of PUCCH F0/F1 resources available per cell for SR |
| `nof_cell_csi_res` | integer | `8` | 0..100 | Number of PUCCH F2/F3/F4 resources available per cell for CSI |
| `f0_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F0 |
| `f1_enable_occ` | boolean | `false` |  | Enable OCC for PUCCH F1 |
| `f1_nof_cyclic_shifts` | integer | `2` | legal values: {1, 2, 3, 4, 6, 12} | Number of possible cyclic shifts available for PUCCH F1 resources |
| `f1_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F1 |
| `f2_max_nof_rbs` | integer | `1` | 1..16 | Max number of RBs for PUCCH F2 resources |
| `f2_max_payload` | integer |  | 4..40 | Min required payload capacity in bits for PUCCH F2 resources |
| `f2_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F2 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f2_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F2 |
| `f3_max_nof_rbs` | integer | `1` | legal values: {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16} | Max number of RBs for PUCCH F3 resources |
| `f3_max_payload` | integer |  | 4..40 | Min required payload capacity in bits for PUCCH F3 resources |
| `f3_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F3 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f3_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F3 |
| `f3_additional_dmrs` | boolean | `false` |  | Enable additional DM-RS for PUCCH F3 |
| `f3_pi2_bpsk` | boolean | `false` |  | Enable pi/2-BPSK modulation for PUCCH F3 |
| `f4_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F4 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f4_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F4 |
| `f4_additional_dmrs` | boolean | `false` |  | Enable additional DM-RS for PUCCH F4 |
| `f4_pi2_bpsk` | boolean | `false` |  | Enable pi/2-BPSK modulation for PUCCH F4 |
| `f4_occ_length` | integer | `2` | legal values: {2, 4} | OCC length for PUCCH F4 |
| `f4_enable_occ` | boolean | `false` |  | Enable OCC multiplexing for PUCCH F4 |
| `min_k1` | integer | `4` | 1..4 | Minimum value of K1 (difference in slots between PDSCH and HARQ-ACK). Lower k1 values will reduce latency, but place a stricter requirement on the UE decode latency. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of consecutive undecoded PUCCH F2 for CSI before an Radio Link Failure is reported |
| `enable_cl_loop_pw_control` | boolean | `false` |  | Enable closed-loop power control for PUCCH |
| `target_sinr_f0` | number | `10` | -10..20 | Target PUCCH F0 SINR in dB |
| `target_sinr_f2` | number | `6` | -10..20 | Target PUCCH F2 SINR in dB |
| `target_sinr_f3` | number | `1` | -15..10 | Target PUCCH F3 SINR in dB |


### srs

SRS parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `type_enabled` | string | `disabled` | enum: disabled, periodic, aperiodic | Enable/disable SRS and set resource type |
| `period_ms` | number | `20` | legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560} | SRS period in ms. For aperiodic SRS, it indicates a tentative timing, and should not be interpreted as a precise period. The SRS period needs to be compatible with the subcarrier spacing |
| `max_nof_sym_per_slot` | integer | `2` | 1..6 | Number of symbols for UL slot that are reserved for the SRS cell resources |
| `nof_sym_per_resource` | integer | `1` | legal values: {1, 2, 4} | Number of symbols per SRS resource |
| `c_srs` | integer |  | 0..63 | C_SRS parameter for SRS. If not set, it's computed automatically from the cell parameters |
| `freq_domain_shift` | integer | `0` | 0..268 | SRS frequency domain shift. Only applies if c_srs is set |
| `tx_comb` | integer | `4` | legal values: {2, 4} | SRS TX comb size |
| `cyclic_shift_reuse` | integer | `1` | legal values: {1, 2, 3, 4, 6} | SRS cyclic shift reuse factor. It needs to be compatible with the TX comb and number of UL antenna ports |
| `sequence_id_reuse` | integer | `1` | legal values: {1, 2, 3, 5, 6, 10, 15, 30} | Enable the reuse of SRS sequence id with the set reuse factor |
| `p0` | integer | `-84` | must be a multiple of 2 within [-202, 24] | P0 value for SRS. Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval. Default: -84 |


### prach

PRACH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `prach_config_index` | integer |  | 0..255 | PRACH configuration index. If not set, the value is derived, so that the PRACH fits in an UL slot |
| `prach_root_sequence_index` | integer | `1` | 0..837 | PRACH root sequence index. NOTE: values: [0, 837] for PRACH format 0, 1, 2, 3. [0, 137] for other formats |
| `zero_correlation_zone` | integer | `0` | 0..15 | Zero correlation zone index |
| `fixed_msg3_mcs` | integer | `0` | 0..28 | Fixed message 3 MCS |
| `max_msg3_harq_retx` | integer | `4` | 0..4 | Maximum number of message 3 HARQ retransmissions |
| `total_nof_ra_preambles` | integer | `64` | 1..64 | Number of different contention-based PRACH preambles per occasion. If less than 64 preambles are used, the remaining preambles can be used for contention-free PRACHs |
| `cfra_enabled` | boolean | `false` |  | Whether to enable Contention-free Random Access (CFRA). If enabled, the total_nof_ra_preambles must be lower than 64 |
| `prach_frequency_start` | integer |  | 0..274 | PRACH message frequency offset in PRBs. NOTE: When setting this parameter, it's up to user the ensure the PRACH opportunities do not overlap with the PUCCH resources |
| `preamble_rx_target_pw` | integer | `-100` | must be a multiple of 2 within [-202, -60] | Target power level at the network receiver side, in dBm |
| `preamble_trans_max` | integer | `7` | legal values: {3, 4, 5, 6, 7, 8, 10, 20, 50, 100, 200} | Max number of RA preamble transmissions performed before declaring a failure |
| `power_ramping_step_db` | integer | `4` | legal values: {0, 2, 4, 6} | Power ramping steps for PRACH |
| `ports` | array of integer | `[0]` |  | List of antenna ports |
| `nof_ssb_per_ro` | integer | `3` | legal values: {1} | Number of SSBs per RACH occasion |
| `nof_cb_preambles_per_ssb` | integer | `64` | 1..64 | Number of Contention Based preambles per SSB |
| `ra_resp_window` | integer |  | legal values: {1, 2, 4, 8, 10, 20, 40, 80} | RA-Response window length in number of slots. |
| `nof_prach_guardbands_rbs` | integer | `3` | 1..10 | Number of RBs that are used as guardband on each side of the PRACH RBs interval for short PRACH formats. |


#### slice_based_ra_prioritization

List of configurations for slice-based RA prioritization

_List of objects with the following items:_


##### slice_based_ra_prioritization[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `power_ramp_step_high_priority` | integer | `0` | legal values: {0, 2, 4, 6} | Power ramping step applied for prioritized random access procedure [dB]. |
| `scaling_factor_bi` | number |  | legal values: {0.0, 0.25, 0.5, 0.75} | Scaling factor for backoff indicator (BI) for the prioritized RA procedure. |
| `nsag_ids` | array of integer | `[]` |  | NSAGs associated with this prioritized RA configuration. |


#### two_step

Two-step RACH (MsgA/MsgB) configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cb_preambles_per_ssb_per_shared_ro` | integer | `4` | 1..60 | Number of CB preambles per SSB per shared RACH occasion for 2-step RA |
| `msgA_rsrp_thres_dbm` | integer | `-100` | -156..-29 | RSRP threshold in dBm above which the UE selects 2-step RA over 4-step RA |
| `msgB_response_window_slots` | integer | `40` | legal values: {1, 2, 4, 8, 10, 20, 40, 80, 160, 320} | MsgB response window length in slots |
| `td_offset` | integer | `1` | 1..32 | Time-domain offset in slots from the PRACH slot to the MsgA PUSCH slot |
| `pusch_td_res_index` | integer | `0` |  | Index into the PUSCH-TimeDomainAllocationResource table for MsgA PUSCH scheduling |
| `mcs` | integer | `0` | 0..28 | MCS index for MsgA PUSCH transmission |
| `nof_prbs_per_msgA_po` | integer | `3` | 1..32 | Number of PRBs per MsgA PUSCH occasion |
| `prb_start` | integer | `0` |  | Frequency offset in PRBs of the lowest MsgA PUSCH occasion from PRB 0 |
| `po_fdm` | integer | `1` | legal values: {1, 2, 4, 8} | Number of MsgA PUSCH occasions FDMed in one time instance |


### tdd_ul_dl_cfg

TDD UL DL configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_ul_tx_period` | integer | `10` | 2..80 | TDD pattern periodicity in slots. The combination of this value and the chosen numerology must lead to a TDD periodicity of 0.5, 0.625, 1, 1.25, 2, 2.5, 3, 4, 5 or 10 milliseconds. |
| `nof_dl_slots` | integer | `6` | 0..80 | TDD pattern nof. consecutive full DL slots |
| `nof_dl_symbols` | integer | `8` | 0..13 | TDD pattern nof. DL symbols at the beginning of the slot following full DL slots |
| `nof_ul_slots` | integer | `3` | 0..80 | TDD pattern nof. consecutive full UL slots |
| `nof_ul_symbols` | integer | `0` | 0..13 | TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot |


#### pattern2

TDD UL DL pattern2 configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_ul_tx_period` | integer | `10` | 2..80 | TDD pattern periodicity in slots. The combination of this value and the chosen numerology must lead to a TDD periodicity of 0.5, 0.625, 1, 1.25, 2, 2.5, 3, 4, 5 or 10 milliseconds. |
| `nof_dl_slots` | integer | `6` | 0..80 | TDD pattern nof. consecutive full DL slots |
| `nof_dl_symbols` | integer | `8` | 0..13 | TDD pattern nof. DL symbols at the beginning of the slot following full DL slots |
| `nof_ul_slots` | integer | `3` | 0..80 | TDD pattern nof. consecutive full UL slots |
| `nof_ul_symbols` | integer | `0` | 0..13 | TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot |


### paging

Paging parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pg_search_space_id` | integer | `1` | legal values: {0, 1} | SearchSpace to use for Paging |
| `default_pg_cycle_in_rf` | integer | `128` | legal values: {32, 64, 128, 256} | Default Paging cycle in nof. Radio Frames |
| `nof_pf_per_paging_cycle` | string | `oneT` | enum: oneT, halfT, quarterT, oneEighthT, oneSixteethT | Number of paging frames per DRX cycle {oneT, halfT, quarterT, oneEighthT, oneSixteethT}. Default: oneT |
| `pf_offset` | integer | `0` |  | Paging frame offset |
| `nof_po_per_pf` | integer | `1` | legal values: {1, 2, 4} | Number of paging occasions per paging frame |
| `edrx_enabled` | boolean | `false` |  | Enable eDRX |


### csi

CSI-Meas parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `csi_rs_enabled` | boolean | `true` |  | Enable CSI-RS resources and CSI reporting |
| `csi_rs_period` | integer | `20` | legal values: {10, 20, 40, 80} | CSI-RS period in milliseconds |
| `report_type` | string | `periodic` | enum: periodic, aperiodic | Type of CSI reporting configuration to use |
| `meas_csi_rs_slot_offset` | integer |  |  | Slot offset of first CSI-RS resource used for measurement |
| `tracking_csi_rs_slot_offset` | integer |  |  | Slot offset of first CSI-RS resource used for tracking |
| `zp_csi_rs_slot_offset` | integer |  |  | Slot offset of the ZP CSI-RS resources |
| `pwr_ctrl_offset` | integer | `0` | -8..15 | powerControlOffset, Power offset of PDSCH RE to NZP CSI-RS RE in dB |


### scheduler

Scheduler parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_preselected_newtx_ues` | integer | `1024` | 1..8192 | Number of UEs pre-selected for potential newTx allocations in a slot. The scheduling policy will only be applied to the pre-selected UEs. |


#### policy

Scheduler policy configuration. By default, time-domain QoS-aware policy is used.


##### qos_sched

Time-domain QoS-aware policy configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `combine_function` | string | `gbr_prioritized` | enum: gbr_prioritized, geometric_mean | QoS-aware scheduler policy weight combining function |
| `pf_fairness_coeff` | number | `2` |  | Fairness Coefficient to use in Proportional Fair (PF) weight |
| `prio_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow priority in QoS-aware scheduling |
| `pdb_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling |
| `gbr_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling |


##### rr_sched

Time-domain Round-robin policy configuration


### ta

Time Advance (TA) parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ta_measurement_slot_period` | integer | `80` |  | Measurements periodicity in number of slots over which the new Timing Advance Command is computed |
| `ta_measurement_slot_prohibit_period` | integer | `0` | 0..10000 | Delay in number of slots between issuing the TA_CMD and starting TA measurements. |
| `ta_cmd_offset_threshold` | integer | `1` | -1..31 | Timing Advance Command (T_A) offset threshold above which Timing Advance Command is triggered. If set to less than zero, issuing of TA Command is disabled |
| `ta_target` | number | `1` | -30..30 | Timing Advance target in units of TA |
| `ta_update_measurement_ul_sinr_threshold` | number | `0` |  | UL SINR threshold (in dB) above which reported N_TA update measurement is considered valid |
| `ta_outlier_detection_zscore_threshold` | number | `1.75` | 0..5 | Z-score threshold for outlier detection in N_TA measurements. Controls the sensitivity of the outlier detection algorithm. A lower value makes the filter more aggressive (rejects more measurements), while a higher value makes it more permissive. Typical values range from 1.5 to 3.0. Setting to 0.0 disables outlier detection. |


### drx

DRX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `on_duration_timer` | integer | `10` | legal values: drx_helper::valid_on_duration_timer_values() | Minimum duration in milliseconds that the UE stays in active mode, when DRX is configured. |
| `inactivity_timer` | integer | `0` | legal values: drx_helper::valid_inactivity_timer_values() | Duration in milliseconds that the UE stays active after PDCCH reception, when DRX is configured. |
| `retx_timer_dl` | integer | `0` | legal values: drx_helper::valid_retx_timer_values() | Maximum duration in slots until a DL ReTX is received by the UE, when DRX is configured. |
| `retx_timer_ul` | integer | `0` | legal values: drx_helper::valid_retx_timer_values() | Maximum duration in slots until a grant for UL ReTX is received by the UE, when DRX is configured. |
| `long_cycle` | integer | `0` | legal values: drx_helper::valid_long_cycle_values() ∪ {0} | Duration in milliseconds between UE DRX long cycles. The value 0 is used to disable DRX |


### slicing

Network slicing configuration

_List of objects with the following items:_


#### slicing[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sst` | integer | `0` | 0..255 | Slice Service Type |
| `sd` | integer | `16777215` | 0..16777215 | Service Differentiator |


##### sched_cfg

Slice scheduling configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_prb_policy_ratio` | integer | `0` | 0..100 | Minimum percentage of PRBs to be allocated to the slice |
| `max_prb_policy_ratio` | integer | `100` | 1..100 | Maximum percentage of PRBs to be allocated to the slice |
| `ded_prb_policy_ratio` | integer | `0` | 1..100 | Dedicated percentage of PRBs to be allocated to the slice |
| `priority` | integer | `0` | 0..254 | Slice priority |


###### policy

Scheduler policy configuration for the slice. If not specified, the policy configured for the cell is used


###### qos_sched

Time-domain QoS-aware policy configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `combine_function` | string | `gbr_prioritized` | enum: gbr_prioritized, geometric_mean | QoS-aware scheduler policy weight combining function |
| `pf_fairness_coeff` | number | `2` |  | Fairness Coefficient to use in Proportional Fair (PF) weight |
| `prio_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow priority in QoS-aware scheduling |
| `pdb_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling |
| `gbr_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling |


###### rr_sched

Time-domain Round-robin policy configuration


### ntn

NTN configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cell_specific_koffset` | integer | `0` | 1..1023 | Cell-specific k-offset to be used for NTN [ms]. |
| `ntn_ul_sync_validity_dur` | integer |  | legal values: {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 120, 180, 240, 900} | An UL sync validity duration |


#### epoch_time

Epoch time for the NTN assistance information

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sfn` | integer | `0` | 0..1023 | SFN Part |
| `subframe_number` | integer | `0` | 0..9 | Sub-frame number Part |


#### ta_info

TA Info for the NTN assistance information

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ta_common` | number | `0` | 0..270730 | TA common |
| `ta_common_drift` | number | `0` | -51.4606..51.4606 | Drift rate of the common TA |
| `ta_common_drift_variant` | number | `0` | 0..0.57898 | Drift rate variation of the common TA |
| `ta_common_offset` | number | `0` | 0..10000 | Constant offset added to TA common |


#### ephemeris_info_ecef

Ephermeris information of the satellite in ecef coordinates

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pos_x` | number | `0` | -43620761.6..43620759.3 | X Position of the satellite [m] |
| `pos_y` | number | `0` | -43620761.6..43620759.3 | Y Position of the satellite [m] |
| `pos_z` | number | `0` | -43620761.6..43620759.3 | Z Position of the satellite [m] |
| `vel_x` | number | `0` | -7864.32..7864.26 | X Velocity of the satellite [m/s] |
| `vel_y` | number | `0` | -7864.32..7864.26 | Y Velocity of the satellite [m/s] |
| `vel_z` | number | `0` | -7864.32..7864.26 | Z Velocity of the satellite [m/s] |


#### ephemeris_orbital

Ephermeris information of the satellite in orbital coordinates

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `semi_major_axis` | number | `0` | 6500000..42998632.07 | Semi-major axis of the satellite [m] |
| `eccentricity` | number | `0` | 0..0.01500510825 | Eccentricity of the satellite [-] |
| `periapsis` | number | `0` | 0..6.28407400155 | Periapsis of the satellite [rad] |
| `longitude` | number | `0` | 0..6.28407400155 | Longitude of the satellites angle of ascending node [rad] |
| `inclination` | number | `0` | -1.57101850624..1.57101848283 | Inclination of the satellite [rad] |
| `mean_anomaly` | number | `0` | 0..6.28407400155 | Mean anomaly of the satellite [rad] |


### rlm

Radio Link Monitoring parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `rlm_resource_type` | string | `default_type` | enum: default_type, ssb, csi_rs, ssb_and_csi_rs | Radio Link Monitoring resource detection type {default_type, ssb, csi_rs, ssb_and_csi_rs}. Default: default_type |


## du

DU parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `warn_on_drop` | boolean | `false` |  | Log a warning for dropped packets in F1-U, RLC and MAC due to full queues |


## cells

Sets the cell configuration on a per cell basis, overwriting the default configuration defined by cell_cfg

_List of objects with the following items:_


### cells[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pci` | integer | `1` | 0..1007 | PCI |
| `sector_id` | integer |  | 0..16383 | Sector ID (4-14 bits). This value is concatenated with the gNB Id to form the NR Cell Identity (NCI). If not specified, a unique value for the DU is automatically derived |
| `dl_arfcn` | string | `536020` | non-negative integer; valid range [0, 3279165] | Downlink ARFCN |
| `band` | integer | `auto` | set to "auto" to auto-derive | NR band |
| `common_scs` | string | `15kHz` | accepts SCS strings (e.g. "15kHz", "30kHz") | Cell common subcarrier spacing |
| `channel_bandwidth_MHz` | integer | `20` | legal values: {5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100, 200, 400} | Channel bandwidth in MHz |
| `nof_antennas_ul` | integer | `1` |  | Number of antennas in uplink |
| `nof_antennas_dl` | integer | `1` |  | Number of antennas in downlink |
| `plmn` | string | `00101` | pattern: `[0-9]{5,6}` | PLMN |
| `additional_plmns` | array of string | `[]` | each element must be a valid PLMN string | List of PLMNs |
| `tac` | integer | `7` | values 0 and 0xfffffe are reserved; value must fit in 24 bits | TAC |
| `enabled` | boolean | `true` |  | Automatically activate the cell on startup |
| `cell_barred` | boolean | `false` |  | MIB cellBarred: if true, UEs cannot camp on this cell |
| `intra_freq_reselection` | boolean | `true` |  | MIB intraFreqReselection: if true, intra-frequency cell reselection is allowed when cell is barred |
| `q_rx_lev_min` | integer | `-70` | -70..-22 | q-RxLevMin, required minimum received RSRP level for cell selection/re-selection, in dBm |
| `q_qual_min` | integer | `-20` | -43..-12 | q-QualMin, required minimum received RSRQ level for cell selection/re-selection, in dB |
| `pcg_p_nr_fr1` | integer |  | -30..23 | p-nr-fr1, maximum total TX power to be used by the UE in this NR cell group across in FR1 |


#### mac_cell_group

MAC Cell Group parameters


##### bsr_cfg

Buffer status report configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `periodic_bsr_timer` | integer | `10` | legal values: {1, 5, 10, 16, 20, 32, 40, 64, 80, 128, 160, 320, 640, 1280, 2560, 0} | Periodic Buffer Status Report Timer value in nof. subframes. Value 0 equates to infinity |
| `retx_bsr_timer` | integer | `80` | legal values: {10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240} | Retransmission Buffer Status Report Timer value in nof. subframes |
| `lc_sr_delay_timer` | integer |  | legal values: {20, 40, 64, 128, 512, 1024, 2560} | Logical Channel SR delay timer in nof. subframes |


##### phr_cfg

Power Headroom report configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `phr_prohibit_timer` | integer | `10` | legal values: {0, 10, 20, 50, 100, 200, 500, 1000} | PHR prohibit timer in nof. subframes |


##### sr_cfg

Scheduling Request configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sr_trans_max` | integer | `64` | legal values: {4, 8, 16, 32, 64} | Maximum number of SR transmissions |
| `sr_prohibit_timer` | integer |  | legal values: {1, 2, 4, 8, 16, 32, 64, 128} | Timer for SR transmission on PUCCH in ms |


#### ssb

SSB parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ssb_period` | integer | `10` | legal values: {5, 10, 20} | Period of SSB scheduling in milliseconds |
| `ssb_block_power_dbm` | integer | `-16` | -60..50 | SS_PBCH_power_block in dBm |
| `pss_to_sss_epre` | string | `0` | enum: 0, 3; legacy CLI11 accepted only the integer values 0 and 3; the builder API maps them to the string keys "0" and "3". | SSB PSS to SSS EPRE ratio in dB {0, 3} |


#### sib

SIB configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `si_window_length` | integer | `160` | legal values: {5, 10, 20, 40, 80, 160, 320, 640, 1280} | The length of the SI scheduling window, in slots. It must be always shorter or equal to the period of the SI message. |
| `t300` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Establishment timer in ms. The timer starts upon transmission of RRCSetupRequest. |
| `t301` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Re-establishment timer in ms. The timer starts upon transmission of RRCReestablishmentRequest. |
| `t310` | integer | `1000` | legal values: {0, 50, 100, 200, 500, 1000, 2000} | Out-of-sync timer in ms. The timer starts upon detecting physical layer problems for the SpCell i.e. upon receiving N310 consecutive out-of-sync indications from lower layers. |
| `n310` | integer | `1` | legal values: {1, 2, 3, 4, 6, 8, 10, 20} | Out-of-sync counter. The counter is increased upon reception of "out-of-sync" from lower layer while the timer T310 is stopped. Starts the timer T310, when configured value is reached. |
| `t311` | integer | `3000` | legal values: {1000, 3000, 5000, 10000, 15000, 20000, 30000} | RRC Connection Re-establishment procedure timer in ms. The timer starts upon initiating the RRC connection re-establishment procedure. |
| `n311` | integer | `1` | legal values: {1, 2, 3, 4, 5, 6, 8, 10} | In-sync counter. The counter is increased upon reception of the "in-sync" from lower layer while the timer T310 is running. Stops the timer T310, when configured value is reached. |
| `t319` | integer | `1000` | legal values: {100, 200, 300, 400, 600, 1000, 1500, 2000} | RRC Connection Resume timer in ms. The timer starts upon transmission of RRCResumeRequest or RRCResumeRequest1. |


##### si_sched_info

Configures the scheduling for each of the SI-messages broadcast by the gNB

_List of objects with the following items:_


###### si_sched_info[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `si_period` | integer | `32` | legal values: {8, 16, 32, 64, 128, 256, 512} | SI message scheduling period in radio frames |
| `sib_mapping` | array of integer | `[]` | each element must be in {2, 3, 4, 5, 6, 7, 8, 19} | Mapping of SIB types to SI-messages. SIB numbers should not be repeated |
| `si_window_position` | integer |  | 1..256 | SI window position of the associated SI-message |


##### sib2

SIB2 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `q_hyst` | integer | `3` | legal values: {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | Hysteresis value for ranking criteria. |
| `thresh_serving_low_p` | integer | `0` | 0..31 | Rx level threshold used by the UE on the serving cell when reselecting towards a lower priority RAT/frequency. |
| `cell_reselection_priority` | integer | `6` | 0..7 | Integer part of the cell reselection priority for the frequency of this cell |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `s_intra_search_p` | integer | `62` | must be an even value within [0, 62] | Rx level threshold for intra frequency measurements in dB |
| `t_reselection_nr` | integer | `1` | 0..7 | Cell reselection timer value in seconds |


##### sib3

SIB3 parameters


###### intra_freq_neigh_cell_list

Intra frequency neighbor cell list

_List of objects with the following items:_


###### intra_freq_neigh_cell_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pci` | integer | `0` | 0..1007 | PCI |
| `q_offset_cell` | integer | `0` | legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | PCI |


###### intra_freq_excluded_cell_list

Intra frequency excluded cell list

_List of objects with the following items:_


###### intra_freq_excluded_cell_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `start` | integer | `0` | 0..1007; **required** | Range start |
| `size` | integer | `1` | legal values: {1, 4, 8, 12, 16, 24, 32, 48, 64, 84, 96, 128, 168, 252, 504, 1008}; **required** | Range size |


##### sib4

SIB4 parameters


###### inter_freq_carrier_freq_list

Inter frequency carrier frequency list

_List of objects with the following items:_


###### inter_freq_carrier_freq_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `arfcn` | integer | `0` |  | ARFCN |
| `ssb_scs` | string | `15kHz` | accepts SCS strings (e.g. "15kHz", "30kHz") | SSB subcarrier spacing |
| `derive_ssb_index_from_cell` | boolean | `false` |  | Derive SSB index from cell |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `thresh_x_high_p` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB |
| `thresh_x_low_p` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB |
| `q_offset_freq` | integer | `0` | legal values: {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24} | Frequency specific offset in dB for equal priority NR frequencies. |


##### sib5

SIB5 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `t_reselection_eutra` | integer | `0` | 0..7 | Cell reselection timer value in seconds |


###### carrier_freq_list_eutra

EUTRA carrier frequency list

_List of objects with the following items:_


###### carrier_freq_list_eutra[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `earfcn` | integer | `0` |  | EARFCN |
| `allowed_meas_bandwidth` | integer | `0` | legal values: {6, 15, 25, 50, 75, 100} | Allowed measurement bandwidth |
| `presence_antenna_port1` | boolean | `false` |  | Whether all neighbor cells use Antenna Port 1 |
| `cell_reselection_priority` | integer |  | 0..7 | Integer part of the cell reselection priority for the frequency of this cell |
| `thresh_x_high` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a higher priority RAT/frequency in dB |
| `thresh_x_low` | integer | `0` | must be an even value within [0, 62] | Rx level threshold in dB used when reselecting to a lower priority RAT/frequency in dB |
| `q_rx_lev_min` | integer | `-140` | must be an even value within [-140, -44] | Minimum required Rx level in the cell in dBm |
| `q_qual_min` | integer | `-34` | -34..-3 | Minimum required quality level in the cell in dB |
| `p_max_eutra` | integer | `33` | -30..33 | Maximum allowed transmission power in dBm on the (uplink) carrier frequency. |


##### sib16

SIB16 parameters


###### freq_prio_list_slicing

Frequency priority slicing list entries

_List of objects with the following items:_


###### freq_prio_list_slicing[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_implicit_carrier_freq` | integer | `0` | 0..8 | DL implicit carrier frequency index for this slicing entry |


###### slice_info_list

Slice info list entries

_List of objects with the following items:_


###### slice_info_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nsag_id` | integer | `0` | 0..255 | NSAG ID |
| `allowed` | boolean | `true` |  | Whether the list of cells in this slice info is allowed (true) or excluded (false) |
| `reselection_priority` | number | `0` | must be a multiple of 0.2 within [0.0, 7.8] | Priority associated with this cell reselection slice |


###### cells_allowed

Slice cell list entries

_List of objects with the following items:_


###### cells_allowed[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `start` | integer | `0` | 0..1007; **required** | Range start |
| `size` | integer | `1` | legal values: {1, 4, 8, 12, 16, 24, 32, 48, 64, 84, 96, 128, 168, 252, 504, 1008}; **required** | Range size |


##### etws

ETWS configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `message_id` | integer | `4356` | 0..65535 | ETWS message ID. |
| `serial_num` | integer | `12288` | 0..65535 | ETWS message serial number. |
| `warning_type` | integer | `2432` | 0..65535 | ETWS warning type. |
| `data_coding_scheme` | integer | `0` | 0..255 | ETWS message CBS coding scheme. |
| `warning_message` | string |  |  | ETWS warning message. Max. Length and character support depends on the chosen coding scheme. |


##### cmas

CMAS configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `message_id` | integer | `4370` | 0..65535 | CMAS message ID. |
| `serial_num` | integer | `12291` | 0..65535 | CMAS message serial number. |
| `data_coding_scheme` | integer | `0` | 0..255 | CMAS message CBS coding scheme. |
| `warning_message` | string | `` |  | CMAS warning message. Max. Length and character support depends on the chosen coding scheme. |


#### ul_common

UL common parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `p_max` | integer |  | -30..23 | Maximum transmit power allowed in this serving cell |
| `max_pucchs_per_slot` | integer | `31` | 1..128 | Maximum number of PUCCH grants that can be allocated per slot |
| `max_ul_grants_per_slot` | integer | `32` | 1..144 | Maximum number of UL grants that can be allocated per slot |
| `min_pucch_pusch_prb_distance` | integer | `1` | 0..137 | Minimum PRB distance between PUCCH and UE-dedicated PUSCH grants |


#### pdcch

PDCCH parameters


##### common

PDCCH Common configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `coreset0_index` | integer |  | 0..15 | CORESET#0 index |
| `ss1_n_candidates` | string | `[0,0,1,0,0]` | 5-element list of uint8_t (e.g. "[0,0,1,0,0]"). Each element must be in {0,1,2,3,4,5,6,8}.; legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string. | Number of PDCCH candidates per aggregation level for SearchSpace#1. Default: {0, 0, 1, 0, 0} |
| `ss0_index` | integer | `0` | 0..15 | SearchSpace#0 index |
| `max_coreset0_duration` | integer |  | 1..2 | Maximum CORESET#0 duration in OFDM symbols to consider when deriving CORESET#0 index |


##### dedicated

PDCCH Dedicated configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `coreset1_rb_start` | integer |  | 0..275 | Starting Common Resource Block (CRB) number for CORESET 1 relative to CRB 0. Default: CRB0 |
| `coreset1_l_crb` | integer |  | 0..275 | Length of CORESET 1 in number of CRBs. Default: Across entire BW of cell |
| `coreset1_duration` | integer |  | 1..2 | Duration of CORESET 1 in number of OFDM symbols. Default: Max(2, Nof. CORESET#0 symbols) |
| `ss2_n_candidates` | string | `[0,0,0,0,0]` | 5-element list of uint8_t (e.g. "[0,0,0,0,0]"). Each element must be in {0,1,2,3,4,5,6,8}.; legacy CLI11 accepted a YAML list of integers; the builder API stores the value as a single string. | Number of PDCCH candidates per aggregation level for SearchSpace#2. Default: {0, 0, 0, 0, 0} i.e. auto-compute nof. candidates |
| `dci_format_0_1_and_1_1` | boolean | `true` |  | DCI format to use in UE dedicated SearchSpace#2 |
| `ss2_type` | string | `ue_dedicated` | enum: common, ue_dedicated | SearchSpace type for UE dedicated SearchSpace#2 |
| `al_cqi_offset` | number | `0` | -15..15 | Offset to apply to the CQI value used in the PDCCH aggregation level calculation. |


#### pdsch

PDSCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_ue_mcs` | integer | `0` | 0..28 | Minimum UE MCS |
| `max_ue_mcs` | integer | `28` | 0..28 | Maximum UE MCS |
| `fixed_rar_mcs` | integer | `0` | 0..28 | Fixed RAR MCS |
| `fixed_sib1_mcs` | integer | `5` | 0..28 | Fixed SIB1 MCS |
| `harq_feedback_disabled` | string | `false` | accepts "true", "false" or a 32-bit bitmap ("0x..." / "0b..."). | Disable DL HARQ Feedback (only for NTN cells).
If set to true, applies the mask 0x0fffffff and disables HARQ feedback for all except the first four HARQs.
If set to a string, it must be a 32-bit bitmap (0x… or 0b…) of the HARQ processes to disable.
The bit set to 1 indicates HARQ processes with disabled DL HARQ feedback and the bit set to 0,
identify HARQ processes with enabled DL HARQ feedback.The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are ignored.
 |
| `nof_harqs` | integer | `16` | legal values: {2, 4, 6, 8, 10, 12, 16, 32} | Number of DL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; otherwise, it defaults to 16. |
| `max_nof_harq_retxs` | integer | `4` | 0..64 | Maximum number of times a DL HARQ can be retransmitted, before it gets discarded. |
| `harq_retx_timeout` | integer | `100` | 10..500 | Maximum time, in milliseconds, between a HARQ NACK and the scheduler allocating the respective HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of HARQ-ACK consecutive KOs before an Radio Link Failure is reported |
| `rv_sequence` | array of integer | `[0, 2, 3, 1]` | each element must be in {0, 1, 2, 3} | RV sequence for PUSCH. (e.g. [0 2 3 1] |
| `mcs_table` | string | `qam256` | enum: qam64, qam256, qam64lowse | MCS table to use PDSCH |
| `min_rb_size` | integer | `1` | 1..275 | Minimum RB size for UE PDSCH resource allocation |
| `max_rb_size` | integer | `275` | 1..275 | Maximum RB size for UE PDSCH resource allocation |
| `start_rb` | integer | `0` | 0..275 | Start RB for resource allocation of UE PDSCHs |
| `end_rb` | integer | `275` | 0..275 | End RB for resource allocation of UE PDSCHs |
| `max_pdschs_per_slot` | integer | `35` | 1..35 | Maximum number of PDSCH grants per slot, including SIB, RAR, Paging and UE data grants. |
| `max_alloc_attempts` | integer | `35` | 1..35 | Maximum number of DL or UL PDCCH grant allocation attempts per slot before scheduler skips the slot |
| `olla_cqi_inc_step` | number | `0.001` | 0..1 | Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled |
| `olla_target_bler` | number | `0.01` | 0..0.5 | Target DL BLER set in Outer-loop link adaptation (OLLA) algorithm |
| `olla_max_cqi_offset` | number | `4` | ≥ 0 | Maximum offset that the Outer-loop link adaptation (OLLA) can apply to CQI |
| `dc_offset` | string | `center` | accepts an integer in [min,max] or one of {"outside","undetermined","center"}. | Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for carrier spacing, and the center of the gNB DL carrier as DC offset value 0. The user can additionally set "outside" to define that the DC offset falls outside the DL carrier or "undetermined" in the case the DC offset is unknown. |
| `harq_la_cqi_drop_threshold` | integer | `3` | 0..15 | Link Adaptation (LA) threshold for drop in CQI of the first HARQ transmission above which HARQ retransmissions are cancelled. Set this value to 0 to disable this feature |
| `harq_la_ri_drop_threshold` | integer | `1` | 0..4 | Link Adaptation (LA) threshold for drop in nof. layers of the first HARQ transmission above which HARQ retransmission is cancelled. Set this value to 0 to disable this feature |
| `dmrs_additional_position` | integer | `2` | 0..3 | PDSCH DMRS additional position |
| `interleaving_bundle_size` | integer | `0` | legal values: {0, 2, 4} | PDSCH interleaving bundle size. Valid values: [0, 2, 4] |
| `max_rank` | integer |  | ≥ 0 | Maximum number of PDSCH transmission layers. The actual maximum is limited by the number of DL antennas. |
| `enable_csi_rs_pdsch_multiplexing` | boolean | `true` |  | Enable multiplexing of CSI-RS and PDSCH |


#### pusch

PUSCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_ue_mcs` | integer | `0` | 0..28 | Minimum UE MCS |
| `max_ue_mcs` | integer | `28` | 0..28 | Maximum UE MCS |
| `harq_mode_b` | string | `false` | accepts "true", "false" or a 32-bit bitmap ("0x..." / "0b..."). | Set HARQ Mode B (only for NTN cells).
If set to true, applies the mask 0x0fffffff to set HARQ Mode B for all except the first four HARQ processes.
If set to a string, it must be a 32-bit bitmap (0x… or 0b…) indicating which HARQ processes use Mode B.
A bit set to 1 indicates HARQ Mode B; a bit set to 0 indicates HARQ Mode A.
The leftmost bit corresponds to HARQ process ID 0; bits for unconfigured HARQ process IDs are ignored.
 |
| `nof_harqs` | integer | `16` | legal values: {16, 32} | Number of UL HARQ processes. The value 32 is applied only for NTN cells when supported by the UE; otherwise, it defaults to 16. |
| `max_nof_harq_retxs` | integer | `4` | 0..64 | Maximum number of times a UL HARQ can be retransmitted, before it gets discarded. |
| `harq_retx_timeout` | integer | `100` | 10..500 | Maximum time, in milliseconds, between a CRC=KO and the scheduler allocating the respective HARQ for retransmission. If this timeout is exceeded, the HARQ process is discarded. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of CRC consecutive KOs before an Radio Link Failure is reported |
| `rv_sequence` | array of integer | `[0]` | each element must be in {0, 1, 2, 3} | RV sequence for PUSCH. (e.g. [0 2 3 1] |
| `mcs_table` | string | `qam256` | enum: qam64, qam256, qam64lowse | MCS table to use PUSCH |
| `max_rank` | integer | `4` | 1..4 | Maximum number of PUSCH transmission layers. The actual maximum is limited by the number of receive ports and UE capabilities. |
| `msg3_delta_preamble` | integer | `6` | -1..6 | msg3-DeltaPreamble, Power offset between msg3 and RACH preamble transmission |
| `p0_nominal_with_grant` | integer | `-76` | must be a multiple of 2 within [-202, 24] | P0 value for PUSCH with grant (except msg3). Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval.  Default: -76 |
| `msg3_delta_power` | integer | `8` | must be a multiple of 2 within [-6, 8] | Target power level at the network receiver side, in dBm. Valid values must be multiple of 2 and within the [-6, 8] interval. Default: 8 |
| `max_puschs_per_slot` | integer | `16` | 1..16 | Maximum number of PUSCH grants per slot |
| `beta_offset_ack_idx_1` | integer | `11` | 0..31 | betaOffsetACK-Index1 part of UCI-OnPUSCH |
| `beta_offset_ack_idx_2` | integer | `6` | 0..31 | betaOffsetACK-Index2 part of UCI-OnPUSCH |
| `beta_offset_ack_idx_3` | integer | `4` | 0..31 | betaOffsetACK-Index3 part of UCI-OnPUSCH |
| `beta_offset_csi_p1_idx_1` | integer | `13` | 0..31 | betaOffsetCSI-Part1-Index1 part of UCI-OnPUSCH |
| `beta_offset_csi_p1_idx_2` | integer | `10` | 0..31 | betaOffsetCSI-Part1-Index2 part of UCI-OnPUSCH |
| `beta_offset_csi_p2_idx_1` | integer | `13` | 0..31 | betaOffsetCSI-Part2-Index1 part of UCI-OnPUSCH |
| `beta_offset_csi_p2_idx_2` | integer | `10` | 0..31 | betaOffsetCSI-Part2-Index2 part of UCI-OnPUSCH |
| `min_k2` | integer | `4` | 1..4 | Minimum value of K2 (difference in slots between PDCCH and PUSCH). |
| `dc_offset` | string | `center` | accepts an integer in [min,max] or one of {"outside","undetermined","center"}. | Direct Current (DC) Offset in number of subcarriers, using the common SCS as reference for carrier spacing, and the center of the gNB UL carrier as DC offset value 0. The user can additionally set "outside" to define that the DC offset falls outside the UL carrier or "undetermined" in the case the DC offset is unknown. |
| `olla_snr_inc_step` | number | `0.001` | 0..1 | Outer-loop link adaptation (OLLA) increment value. The value 0 means that OLLA is disabled |
| `olla_target_bler` | number | `0.01` | 0..0.5 | Target UL BLER set in Outer-loop link adaptation (OLLA) algorithm |
| `olla_max_snr_offset` | number | `5` | ≥ 0 | Maximum offset that the Outer-loop link adaptation (OLLA) can apply to the estimated UL SINR |
| `dmrs_additional_position` | integer | `2` | 0..3 | PUSCH DMRS additional position |
| `min_rb_size` | integer | `1` | 1..275 | Minimum RB size for UE PUSCH resource allocation |
| `max_rb_size` | integer | `275` | 1..275 | Maximum RB size for UE PUSCH resource allocation |
| `start_rb` | integer | `0` | 0..275 | Start RB for resource allocation of UE PUSCHs |
| `end_rb` | integer | `275` | 0..275 | End RB for resource allocation of UE PUSCHs |
| `enable_cl_loop_pw_control` | boolean | `false` |  | Enable closed-loop power control for PUSCH |
| `enable_phr_bw_adaptation` | boolean | `false` |  | Enable bandwidth adaptation to prevent negative PHR |
| `target_sinr` | number | `10` | -5..30 | Target PUSCH SINR in dB |
| `ref_path_loss` | number | `70` | 50..120 | Reference path-loss for target PUSCH SINR in dB |
| `pl_compensation_factor` | number | `1` | legal values: {0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0} | Fractional path-loss compensation factor in PUSCH power control |
| `enable_transform_precoding` | boolean | `false` |  | Enable transform precoding for PUSCH. |


#### pucch

PUCCH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `p0_nominal` | integer | `-90` | must be a multiple of 2 within [-202, 24] | Power control parameter P0 for PUCCH transmissions. Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval. Default: -90 |
| `pucch_resource_common` | integer |  | 0..15 | Index of PUCCH resource set for the common configuration |
| `sr_period_ms` | number | `20` | legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 40, 80, 160, 320} | SR period in msec |
| `formats` | string | `f1_and_f2` | enum: f0_and_f2, f1_and_f2, f1_and_f3, f1_and_f4 | PUCCH formats combination to use. Values: {f0_and_f2, f1_and_f2, f1_and_f3, f1_and_f4}. Default: f1_and_f2 |
| `resource_set_size` | integer | `8` | 1..8 | Number of PUCCH resources in each PUCCH resource set |
| `nof_cell_res_set_configs` | integer | `2` | 1..10 | Number of PUCCH Resource Set configurations that are available per cell. NOTE: the higher the number of configurations, the lower the chances UEs have to share the same PUCCH resources for HARQ-ACK. |
| `nof_cell_sr_res` | integer | `8` | 1..100 | Number of PUCCH F0/F1 resources available per cell for SR |
| `nof_cell_csi_res` | integer | `8` | 0..100 | Number of PUCCH F2/F3/F4 resources available per cell for CSI |
| `f0_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F0 |
| `f1_enable_occ` | boolean | `false` |  | Enable OCC for PUCCH F1 |
| `f1_nof_cyclic_shifts` | integer | `2` | legal values: {1, 2, 3, 4, 6, 12} | Number of possible cyclic shifts available for PUCCH F1 resources |
| `f1_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F1 |
| `f2_max_nof_rbs` | integer | `1` | 1..16 | Max number of RBs for PUCCH F2 resources |
| `f2_max_payload` | integer |  | 4..40 | Min required payload capacity in bits for PUCCH F2 resources |
| `f2_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F2 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f2_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F2 |
| `f3_max_nof_rbs` | integer | `1` | legal values: {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16} | Max number of RBs for PUCCH F3 resources |
| `f3_max_payload` | integer |  | 4..40 | Min required payload capacity in bits for PUCCH F3 resources |
| `f3_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F3 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f3_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F3 |
| `f3_additional_dmrs` | boolean | `false` |  | Enable additional DM-RS for PUCCH F3 |
| `f3_pi2_bpsk` | boolean | `false` |  | Enable pi/2-BPSK modulation for PUCCH F3 |
| `f4_max_code_rate` | string | `dot35` | enum: dot08, dot15, dot25, dot35, dot45, dot60, dot80 | PUCCH F4 max code rate {dot08, dot15, dot25, dot35, dot45, dot60, dot80}. Default: dot35 |
| `f4_intraslot_freq_hop` | boolean | `false` |  | Enable intra-slot frequency hopping for PUCCH F4 |
| `f4_additional_dmrs` | boolean | `false` |  | Enable additional DM-RS for PUCCH F4 |
| `f4_pi2_bpsk` | boolean | `false` |  | Enable pi/2-BPSK modulation for PUCCH F4 |
| `f4_occ_length` | integer | `2` | legal values: {2, 4} | OCC length for PUCCH F4 |
| `f4_enable_occ` | boolean | `false` |  | Enable OCC multiplexing for PUCCH F4 |
| `min_k1` | integer | `4` | 1..4 | Minimum value of K1 (difference in slots between PDSCH and HARQ-ACK). Lower k1 values will reduce latency, but place a stricter requirement on the UE decode latency. |
| `max_consecutive_kos` | integer | `100` |  | Maximum number of consecutive undecoded PUCCH F2 for CSI before an Radio Link Failure is reported |
| `enable_cl_loop_pw_control` | boolean | `false` |  | Enable closed-loop power control for PUCCH |
| `target_sinr_f0` | number | `10` | -10..20 | Target PUCCH F0 SINR in dB |
| `target_sinr_f2` | number | `6` | -10..20 | Target PUCCH F2 SINR in dB |
| `target_sinr_f3` | number | `1` | -15..10 | Target PUCCH F3 SINR in dB |


#### srs

SRS parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `type_enabled` | string | `disabled` | enum: disabled, periodic, aperiodic | Enable/disable SRS and set resource type |
| `period_ms` | number | `20` | legal values: {1, 2, 2.5, 4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560} | SRS period in ms. For aperiodic SRS, it indicates a tentative timing, and should not be interpreted as a precise period. The SRS period needs to be compatible with the subcarrier spacing |
| `max_nof_sym_per_slot` | integer | `2` | 1..6 | Number of symbols for UL slot that are reserved for the SRS cell resources |
| `nof_sym_per_resource` | integer | `1` | legal values: {1, 2, 4} | Number of symbols per SRS resource |
| `c_srs` | integer |  | 0..63 | C_SRS parameter for SRS. If not set, it's computed automatically from the cell parameters |
| `freq_domain_shift` | integer | `0` | 0..268 | SRS frequency domain shift. Only applies if c_srs is set |
| `tx_comb` | integer | `4` | legal values: {2, 4} | SRS TX comb size |
| `cyclic_shift_reuse` | integer | `1` | legal values: {1, 2, 3, 4, 6} | SRS cyclic shift reuse factor. It needs to be compatible with the TX comb and number of UL antenna ports |
| `sequence_id_reuse` | integer | `1` | legal values: {1, 2, 3, 5, 6, 10, 15, 30} | Enable the reuse of SRS sequence id with the set reuse factor |
| `p0` | integer | `-84` | must be a multiple of 2 within [-202, 24] | P0 value for SRS. Value in dBm. Valid values must be multiple of 2 and within the [-202, 24] interval. Default: -84 |


#### prach

PRACH parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `prach_config_index` | integer |  | 0..255 | PRACH configuration index. If not set, the value is derived, so that the PRACH fits in an UL slot |
| `prach_root_sequence_index` | integer | `1` | 0..837 | PRACH root sequence index. NOTE: values: [0, 837] for PRACH format 0, 1, 2, 3. [0, 137] for other formats |
| `zero_correlation_zone` | integer | `0` | 0..15 | Zero correlation zone index |
| `fixed_msg3_mcs` | integer | `0` | 0..28 | Fixed message 3 MCS |
| `max_msg3_harq_retx` | integer | `4` | 0..4 | Maximum number of message 3 HARQ retransmissions |
| `total_nof_ra_preambles` | integer | `64` | 1..64 | Number of different contention-based PRACH preambles per occasion. If less than 64 preambles are used, the remaining preambles can be used for contention-free PRACHs |
| `cfra_enabled` | boolean | `false` |  | Whether to enable Contention-free Random Access (CFRA). If enabled, the total_nof_ra_preambles must be lower than 64 |
| `prach_frequency_start` | integer |  | 0..274 | PRACH message frequency offset in PRBs. NOTE: When setting this parameter, it's up to user the ensure the PRACH opportunities do not overlap with the PUCCH resources |
| `preamble_rx_target_pw` | integer | `-100` | must be a multiple of 2 within [-202, -60] | Target power level at the network receiver side, in dBm |
| `preamble_trans_max` | integer | `7` | legal values: {3, 4, 5, 6, 7, 8, 10, 20, 50, 100, 200} | Max number of RA preamble transmissions performed before declaring a failure |
| `power_ramping_step_db` | integer | `4` | legal values: {0, 2, 4, 6} | Power ramping steps for PRACH |
| `ports` | array of integer | `[0]` |  | List of antenna ports |
| `nof_ssb_per_ro` | integer | `3` | legal values: {1} | Number of SSBs per RACH occasion |
| `nof_cb_preambles_per_ssb` | integer | `64` | 1..64 | Number of Contention Based preambles per SSB |
| `ra_resp_window` | integer |  | legal values: {1, 2, 4, 8, 10, 20, 40, 80} | RA-Response window length in number of slots. |
| `nof_prach_guardbands_rbs` | integer | `3` | 1..10 | Number of RBs that are used as guardband on each side of the PRACH RBs interval for short PRACH formats. |


##### slice_based_ra_prioritization

List of configurations for slice-based RA prioritization

_List of objects with the following items:_


###### slice_based_ra_prioritization[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `power_ramp_step_high_priority` | integer | `0` | legal values: {0, 2, 4, 6} | Power ramping step applied for prioritized random access procedure [dB]. |
| `scaling_factor_bi` | number |  | legal values: {0.0, 0.25, 0.5, 0.75} | Scaling factor for backoff indicator (BI) for the prioritized RA procedure. |
| `nsag_ids` | array of integer | `[]` |  | NSAGs associated with this prioritized RA configuration. |


##### two_step

Two-step RACH (MsgA/MsgB) configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cb_preambles_per_ssb_per_shared_ro` | integer | `4` | 1..60 | Number of CB preambles per SSB per shared RACH occasion for 2-step RA |
| `msgA_rsrp_thres_dbm` | integer | `-100` | -156..-29 | RSRP threshold in dBm above which the UE selects 2-step RA over 4-step RA |
| `msgB_response_window_slots` | integer | `40` | legal values: {1, 2, 4, 8, 10, 20, 40, 80, 160, 320} | MsgB response window length in slots |
| `td_offset` | integer | `1` | 1..32 | Time-domain offset in slots from the PRACH slot to the MsgA PUSCH slot |
| `pusch_td_res_index` | integer | `0` |  | Index into the PUSCH-TimeDomainAllocationResource table for MsgA PUSCH scheduling |
| `mcs` | integer | `0` | 0..28 | MCS index for MsgA PUSCH transmission |
| `nof_prbs_per_msgA_po` | integer | `3` | 1..32 | Number of PRBs per MsgA PUSCH occasion |
| `prb_start` | integer | `0` |  | Frequency offset in PRBs of the lowest MsgA PUSCH occasion from PRB 0 |
| `po_fdm` | integer | `1` | legal values: {1, 2, 4, 8} | Number of MsgA PUSCH occasions FDMed in one time instance |


#### tdd_ul_dl_cfg

TDD UL DL configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_ul_tx_period` | integer | `10` | 2..80 | TDD pattern periodicity in slots. The combination of this value and the chosen numerology must lead to a TDD periodicity of 0.5, 0.625, 1, 1.25, 2, 2.5, 3, 4, 5 or 10 milliseconds. |
| `nof_dl_slots` | integer | `6` | 0..80 | TDD pattern nof. consecutive full DL slots |
| `nof_dl_symbols` | integer | `8` | 0..13 | TDD pattern nof. DL symbols at the beginning of the slot following full DL slots |
| `nof_ul_slots` | integer | `3` | 0..80 | TDD pattern nof. consecutive full UL slots |
| `nof_ul_symbols` | integer | `0` | 0..13 | TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot |


##### pattern2

TDD UL DL pattern2 configuration parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_ul_tx_period` | integer | `10` | 2..80 | TDD pattern periodicity in slots. The combination of this value and the chosen numerology must lead to a TDD periodicity of 0.5, 0.625, 1, 1.25, 2, 2.5, 3, 4, 5 or 10 milliseconds. |
| `nof_dl_slots` | integer | `6` | 0..80 | TDD pattern nof. consecutive full DL slots |
| `nof_dl_symbols` | integer | `8` | 0..13 | TDD pattern nof. DL symbols at the beginning of the slot following full DL slots |
| `nof_ul_slots` | integer | `3` | 0..80 | TDD pattern nof. consecutive full UL slots |
| `nof_ul_symbols` | integer | `0` | 0..13 | TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot |


#### paging

Paging parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pg_search_space_id` | integer | `1` | legal values: {0, 1} | SearchSpace to use for Paging |
| `default_pg_cycle_in_rf` | integer | `128` | legal values: {32, 64, 128, 256} | Default Paging cycle in nof. Radio Frames |
| `nof_pf_per_paging_cycle` | string | `oneT` | enum: oneT, halfT, quarterT, oneEighthT, oneSixteethT | Number of paging frames per DRX cycle {oneT, halfT, quarterT, oneEighthT, oneSixteethT}. Default: oneT |
| `pf_offset` | integer | `0` |  | Paging frame offset |
| `nof_po_per_pf` | integer | `1` | legal values: {1, 2, 4} | Number of paging occasions per paging frame |
| `edrx_enabled` | boolean | `false` |  | Enable eDRX |


#### csi

CSI-Meas parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `csi_rs_enabled` | boolean | `true` |  | Enable CSI-RS resources and CSI reporting |
| `csi_rs_period` | integer | `20` | legal values: {10, 20, 40, 80} | CSI-RS period in milliseconds |
| `report_type` | string | `periodic` | enum: periodic, aperiodic | Type of CSI reporting configuration to use |
| `meas_csi_rs_slot_offset` | integer |  |  | Slot offset of first CSI-RS resource used for measurement |
| `tracking_csi_rs_slot_offset` | integer |  |  | Slot offset of first CSI-RS resource used for tracking |
| `zp_csi_rs_slot_offset` | integer |  |  | Slot offset of the ZP CSI-RS resources |
| `pwr_ctrl_offset` | integer | `0` | -8..15 | powerControlOffset, Power offset of PDSCH RE to NZP CSI-RS RE in dB |


#### scheduler

Scheduler parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_preselected_newtx_ues` | integer | `1024` | 1..8192 | Number of UEs pre-selected for potential newTx allocations in a slot. The scheduling policy will only be applied to the pre-selected UEs. |


##### policy

Scheduler policy configuration. By default, time-domain QoS-aware policy is used.


###### qos_sched

Time-domain QoS-aware policy configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `combine_function` | string | `gbr_prioritized` | enum: gbr_prioritized, geometric_mean | QoS-aware scheduler policy weight combining function |
| `pf_fairness_coeff` | number | `2` |  | Fairness Coefficient to use in Proportional Fair (PF) weight |
| `prio_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow priority in QoS-aware scheduling |
| `pdb_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling |
| `gbr_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling |


###### rr_sched

Time-domain Round-robin policy configuration


#### ta

Time Advance (TA) parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ta_measurement_slot_period` | integer | `80` |  | Measurements periodicity in number of slots over which the new Timing Advance Command is computed |
| `ta_measurement_slot_prohibit_period` | integer | `0` | 0..10000 | Delay in number of slots between issuing the TA_CMD and starting TA measurements. |
| `ta_cmd_offset_threshold` | integer | `1` | -1..31 | Timing Advance Command (T_A) offset threshold above which Timing Advance Command is triggered. If set to less than zero, issuing of TA Command is disabled |
| `ta_target` | number | `1` | -30..30 | Timing Advance target in units of TA |
| `ta_update_measurement_ul_sinr_threshold` | number | `0` |  | UL SINR threshold (in dB) above which reported N_TA update measurement is considered valid |
| `ta_outlier_detection_zscore_threshold` | number | `1.75` | 0..5 | Z-score threshold for outlier detection in N_TA measurements. Controls the sensitivity of the outlier detection algorithm. A lower value makes the filter more aggressive (rejects more measurements), while a higher value makes it more permissive. Typical values range from 1.5 to 3.0. Setting to 0.0 disables outlier detection. |


#### drx

DRX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `on_duration_timer` | integer | `10` | legal values: drx_helper::valid_on_duration_timer_values() | Minimum duration in milliseconds that the UE stays in active mode, when DRX is configured. |
| `inactivity_timer` | integer | `0` | legal values: drx_helper::valid_inactivity_timer_values() | Duration in milliseconds that the UE stays active after PDCCH reception, when DRX is configured. |
| `retx_timer_dl` | integer | `0` | legal values: drx_helper::valid_retx_timer_values() | Maximum duration in slots until a DL ReTX is received by the UE, when DRX is configured. |
| `retx_timer_ul` | integer | `0` | legal values: drx_helper::valid_retx_timer_values() | Maximum duration in slots until a grant for UL ReTX is received by the UE, when DRX is configured. |
| `long_cycle` | integer | `0` | legal values: drx_helper::valid_long_cycle_values() ∪ {0} | Duration in milliseconds between UE DRX long cycles. The value 0 is used to disable DRX |


#### slicing

Network slicing configuration

_List of objects with the following items:_


##### slicing[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sst` | integer | `0` | 0..255 | Slice Service Type |
| `sd` | integer | `16777215` | 0..16777215 | Service Differentiator |


###### sched_cfg

Slice scheduling configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `min_prb_policy_ratio` | integer | `0` | 0..100 | Minimum percentage of PRBs to be allocated to the slice |
| `max_prb_policy_ratio` | integer | `100` | 1..100 | Maximum percentage of PRBs to be allocated to the slice |
| `ded_prb_policy_ratio` | integer | `0` | 1..100 | Dedicated percentage of PRBs to be allocated to the slice |
| `priority` | integer | `0` | 0..254 | Slice priority |


###### policy

Scheduler policy configuration for the slice. If not specified, the policy configured for the cell is used


###### qos_sched

Time-domain QoS-aware policy configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `combine_function` | string | `gbr_prioritized` | enum: gbr_prioritized, geometric_mean | QoS-aware scheduler policy weight combining function |
| `pf_fairness_coeff` | number | `2` |  | Fairness Coefficient to use in Proportional Fair (PF) weight |
| `prio_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow priority in QoS-aware scheduling |
| `pdb_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling |
| `gbr_enabled` | boolean | `true` |  | Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling |


###### rr_sched

Time-domain Round-robin policy configuration


#### ntn

NTN configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cell_specific_koffset` | integer | `0` | 1..1023 | Cell-specific k-offset to be used for NTN [ms]. |
| `ntn_ul_sync_validity_dur` | integer |  | legal values: {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 120, 180, 240, 900} | An UL sync validity duration |


##### epoch_time

Epoch time for the NTN assistance information

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sfn` | integer | `0` | 0..1023 | SFN Part |
| `subframe_number` | integer | `0` | 0..9 | Sub-frame number Part |


##### ta_info

TA Info for the NTN assistance information

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ta_common` | number | `0` | 0..270730 | TA common |
| `ta_common_drift` | number | `0` | -51.4606..51.4606 | Drift rate of the common TA |
| `ta_common_drift_variant` | number | `0` | 0..0.57898 | Drift rate variation of the common TA |
| `ta_common_offset` | number | `0` | 0..10000 | Constant offset added to TA common |


##### ephemeris_info_ecef

Ephermeris information of the satellite in ecef coordinates

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `pos_x` | number | `0` | -43620761.6..43620759.3 | X Position of the satellite [m] |
| `pos_y` | number | `0` | -43620761.6..43620759.3 | Y Position of the satellite [m] |
| `pos_z` | number | `0` | -43620761.6..43620759.3 | Z Position of the satellite [m] |
| `vel_x` | number | `0` | -7864.32..7864.26 | X Velocity of the satellite [m/s] |
| `vel_y` | number | `0` | -7864.32..7864.26 | Y Velocity of the satellite [m/s] |
| `vel_z` | number | `0` | -7864.32..7864.26 | Z Velocity of the satellite [m/s] |


##### ephemeris_orbital

Ephermeris information of the satellite in orbital coordinates

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `semi_major_axis` | number | `0` | 6500000..42998632.07 | Semi-major axis of the satellite [m] |
| `eccentricity` | number | `0` | 0..0.01500510825 | Eccentricity of the satellite [-] |
| `periapsis` | number | `0` | 0..6.28407400155 | Periapsis of the satellite [rad] |
| `longitude` | number | `0` | 0..6.28407400155 | Longitude of the satellites angle of ascending node [rad] |
| `inclination` | number | `0` | -1.57101850624..1.57101848283 | Inclination of the satellite [rad] |
| `mean_anomaly` | number | `0` | 0..6.28407400155 | Mean anomaly of the satellite [rad] |


#### rlm

Radio Link Monitoring parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `rlm_resource_type` | string | `default_type` | enum: default_type, ssb, csi_rs, ssb_and_csi_rs | Radio Link Monitoring resource detection type {default_type, ssb, csi_rs, ssb_and_csi_rs}. Default: default_type |


## qos

Configures RLC and PDCP radio bearers on a per 5QI basis.

_List of objects with the following items:_


### qos[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `five_qi` | integer | `9` | 0..255 | 5QI |


#### rlc

RLC parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `mode` | string | `am` |  | RLC mode |


##### um-bidir

UM parameters


###### tx

UM TX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC UM TX SN |
| `queue-size` | integer | `0` |  | RLC UM TX SDU queue limit in PDUs |
| `queue-bytes` | integer | `0` |  | RLC UM TX SDU queue limit in bytes |


###### rx

UM TX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC UM RX SN |
| `t-reassembly` | integer | `0` |  | RLC UM t-Reassembly |


##### am

AM parameters


###### tx

AM TX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC AM TX SN size |
| `t-poll-retransmit` | integer | `0` |  | RLC AM TX t-PollRetransmit (ms) |
| `max-retx-threshold` | integer | `0` |  | RLC AM max retx threshold |
| `poll-pdu` | integer | `0` |  | RLC AM TX PollPdu |
| `poll-byte` | integer | `0` |  | RLC AM TX PollByte |
| `max_window` | integer | `0` |  | Non-standard parameter that limits the tx window size. Can be used for limiting memory usage with large windows. 0 means no limits other than the SN size (i.e. 2^[sn_size-1]). |
| `queue-size` | integer | `4096` |  | RLC AM TX SDU queue size in PDUs |
| `queue-bytes` | integer | `6172672` |  | RLC AM TX SDU queue size in bytes |


###### rx

AM RX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC AM RX SN |
| `t-reassembly` | integer | `0` |  | RLC AM RX t-Reassembly |
| `t-status-prohibit` | integer | `0` |  | RLC AM RX t-StatusProhibit |
| `max_sn_per_status` | integer | `0` |  | RLC AM RX status SN limit |


#### f1u_du

F1-U parameters at DU side

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `backoff_timer` | integer | `0` |  | F1-U backoff timer (ms) |
| `ul_buffer_size` | integer | `0` |  | F1-U handover buffer size |


## srbs

Configures signaling radio bearers.

_List of objects with the following items:_


### srbs[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `srb_id` | integer | `0` | legal values: {1, 2} | SRB Id |


#### rlc

RLC parameters


##### tx

AM TX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC AM TX SN size |
| `t-poll-retransmit` | integer | `0` |  | RLC AM TX t-PollRetransmit (ms) |
| `max-retx-threshold` | integer | `0` |  | RLC AM max retx threshold |
| `poll-pdu` | integer | `0` |  | RLC AM TX PollPdu |
| `poll-byte` | integer | `0` |  | RLC AM TX PollByte |
| `max_window` | integer | `0` |  | Non-standard parameter that limits the tx window size. Can be used for limiting memory usage with large windows. 0 means no limits other than the SN size (i.e. 2^[sn_size-1]). |
| `queue-size` | integer | `4096` |  | RLC AM TX SDU queue size in PDUs |
| `queue-bytes` | integer | `6172672` |  | RLC AM TX SDU queue size in bytes |


##### rx

AM RX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC AM RX SN |
| `t-reassembly` | integer | `0` |  | RLC AM RX t-Reassembly |
| `t-status-prohibit` | integer | `0` |  | RLC AM RX t-StatusProhibit |
| `max_sn_per_status` | integer | `0` |  | RLC AM RX status SN limit |


## test_mode

Test mode configuration


### test_ue

automatically created UE for testing purposes

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `rnti` | integer | `0` | 0..65519 | C-RNTI (0x0 if not configured) |
| `nof_ues` | integer | `1` | 1..8192 | Number of test UE(s) to create. |
| `ue_creation_stagger_slots` | integer | `10` | 0..10240 | Number of slots between consecutive test mode UE creations |
| `auto_ack_indication_delay` | integer |  |  | Delay before the UL and DL HARQs are automatically ACKed. This feature should only be used if the UL PHY is not operational |
| `attach_detach_duration_ms` | integer |  | 100..10000 | Duration in milliseconds of active traffic after all UEs are established before they are released and recreated. When set, UEs cycle indefinitely through attach, traffic, and detach. Unset disables cycling. |
| `attach_detach_guard_duration_ms` | integer | `1000` | 100..60000 | Guard period duration in milliseconds between a release cycle and the next creation cycle. |
| `pdsch_active` | boolean | `true` |  | PDSCH enabled |
| `pusch_active` | boolean | `true` |  | PUSCH enabled |
| `cqi` | integer | `15` | 1..15 | Channel Quality Information (CQI) to be forwarded to test UE. |
| `ri` | integer | `1` | 1..4 | Rank Indicator (RI) to be forwarded to test UE. |
| `pmi` | integer | `0` | 0..3 | Precoder Matrix Indicator (PMI) to be forwarded to test UE. |
| `i_1_1` | integer | `0` | 0..7 | Precoder Matrix codebook index "i_1_1" to be forwarded to test UE, in the case of more than 2 antennas. |
| `i_1_3` | integer | `0` | 0..1 | Precoder Matrix codebook index "i_1_3" to be forwarded to test UE, in the case of more than 2 antennas. |
| `i_2` | integer | `0` | 0..3 | Precoder Matrix codebook index "i_2" to be forwarded to test UE, in the case of more than 2 antennas. |


## e2

E2 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_du_e2` | boolean | `false` |  | Enable DU E2 agent |
| `addrs` | array of string | `[127.0.0.1]` |  | RIC addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing |
| `port` | integer | `36421` | 20000..40000 | RIC port |
| `bind_addrs` | array of string | `[127.0.0.1]` |  | Local bind addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `e2sm_kpm_enabled` | boolean | `false` |  | Enable KPM service module |
| `e2sm_rc_enabled` | boolean | `false` |  | Enable RC service module |
| `e2sm_ccc_enabled` | boolean | `false` |  | Enable CCC service module |


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


## ru_dummy

Dummy Radio Unit configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dl_processing_delay` | integer | `1` |  | DL processing processing delay in slots |
| `time_scaling` | number | `1` | ≥ 0 | Time scaling factor applied to the slot duration. Must be greater than zero. A value greater than one slows down the RU, while a value between zero and one speeds it up. |


