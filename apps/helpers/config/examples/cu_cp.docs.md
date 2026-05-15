# CU-CP configuration reference

## Reusable types

### <a id="types-log-level"></a>`log-level`

- Type: string
- Constraints: enum: none, error, warning, info, debug

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dryrun` | boolean | `false` |  | Enable application dry run mode |
| `gnb_id` | integer | `411` |  | gNodeB identifier |
| `gnb_id_bit_length` | integer | `22` | 22..32 | gNodeB identifier length in bits |
| `ran_node_name` | string | `ocucp01` |  | RAN node name |


## log

Logging configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `/tmp/cu_cp.log` |  | Log file output path |
| `all_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Default log level for PHY, MAC, RLC, PDCP, RRC, SDAP, NGAP and GTPU |
| `lib_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Generic log level |
| `e2ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug; falls back to --all_level if unset | E2AP log level |
| `config_level` | [`log-level`](#types-log-level) | `none` | enum: none, error, warning, info, debug; falls back to --all_level if unset | Config log level |
| `hex_max_size` | integer | `0` | -1..1024 | Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes) |
| `pdcp_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | PDCP log level |
| `rrc_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | RRC log level |
| `ngap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | NGAP log level |
| `xnap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | XNAP log level |
| `nrppa_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | NRPPA log level |
| `e1ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | E1AP log level |
| `f1ap_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | F1AP log level |
| `cu_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Log level for the CU |
| `sec_level` | [`log-level`](#types-log-level) | `warning` | enum: none, error, warning, info, debug | Security functions log level |
| `e1ap_json_enabled` | boolean | `false` |  | Enable JSON logging of E1AP PDUs |
| `f1ap_json_enabled` | boolean | `false` |  | Enable JSON logging of F1AP PDUs |


## trace

General tracer configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `` |  | Set to a valid file path to enable tracing and write the trace to the file |
| `max_tracing_events_per_file` | integer | `1000000` |  | Maximum number of events per file. Set to zero for no limit |
| `nof_tracing_events_after_severe` | integer | `0` |  | Number of events to write prior to a severe event. Set to zero for writing all events |


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


### layers

Layer basis metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_app_usage` | boolean | `false` |  | Enable application usage metrics |
| `enable_ngap` | boolean | `false` |  | Enable NGAP metrics |
| `enable_pdcp` | boolean | `false` |  | Enable PDCP metrics |
| `enable_rrc` | boolean | `false` |  | Enable CU-CP RRC metrics |


### periodicity

Metrics periodicity configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `app_usage_report_period` | integer | `1000` |  | Application resource usage metrics report period (in milliseconds) |
| `cu_cp_report_period` | integer | `1000` |  | CU-CP metrics report period in milliseconds |


## expert_execution

Expert execution configuration


### affinities

Application CPU affinities configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `main_pool_cpus` | string | `` | comma-separated CPU ids or ranges, e.g. "0-3,5,7" | CPU cores assigned to main thread pool |
| `main_pool_pinning` | string | `mask` | one of: mask, round-robin | Policy used for assigning CPU cores to the main thread pool |


### threads

Threads configuration


#### main_pool

Main thread pool configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_threads` | integer |  |  | Number of threads for processing upper PHY and upper layers. |
| `task_queue_size` | integer | `2048` |  | Main thread pool task queue size. |
| `backoff_period` | integer | `50` |  | Main thread pool back-off period, in microseconds. |


## remote_control

Remote control configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enabled` | boolean | `false` |  | Enables the Remote Control Server |
| `bind_addr` | string | `127.0.0.1` |  | Remote Control Server bind address |
| `port` | integer | `8001` | 0..65535 | Port where the remote control server listens for incoming connections |


## cu_cp

CU-CP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `max_nof_dus` | integer | `6` |  | Maximum number of DU connections that the CU-CP may accept |
| `max_nof_cu_ups` | integer | `6` |  | Maximum number of CU-UP connections that the CU-CP may accept |
| `max_nof_ues` | integer | `8192` |  | Maximum number of UEs that the CU-CP may accept |
| `max_nof_drbs_per_ue` | integer | `8` | 1..29 | Maximum number of DRBs per UE |
| `inactivity_timer` | integer | `120` | 1..7200 | UE/PDU Session/DRB inactivity timer in seconds |
| `enable_rrc_inactive` | boolean | `false` |  | Enable RRC inactive state for UEs based on inactivity timer. When disabled, UEs will be released on inactivity |
| `ran_paging_cycle` | integer | `32` | enum: 32, 64, 128, 256 | RAN Paging cycle for RRC inactive UEs in nof. Radio Frames |
| `t380` | integer | `10` | enum: 5, 10, 20, 30, 60, 120, 360, 720 | RRC inactivity timer T380 in minutes. The timer is started when the UE recveives a RRC Release message including a suspend config and is stopped on the reception of RRCResume. |
| `nof_i_rnti_ue_bits` | integer | `13` | 1..18 | Number of bits used for the UE id in short and full I-RNTI |
| `request_pdu_session_timeout` | integer | `3` | must be larger than T310 | Timeout for requesting a PDU session after the InitialUeMessage was sent to the core, in seconds. The timeout must be larger than T310. If the value is reached, the UE will be released. |


### e1ap

E1AP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addrs` | array of string | `[127.0.20.1]` |  | E1 bind addresses |
| `procedure_timeout` | integer | `1000` |  | Time that the E1AP waits for a CU-UP response in milliseconds |


### f1ap

F1AP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addrs` | array of string | `[127.0.10.1]` |  | CU-CP F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing |
| `procedure_timeout` | integer | `1000` |  | Time that the F1AP waits for a DU response in milliseconds |


### amf

AMF configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `no_core` | boolean | `false` |  | Allow CU-CP to run without a core |
| `amf_reconnection_retry_time` | integer | `1000` |  | Time to wait after a failed AMF reconnection attempt in ms |
| `procedure_timeout` | integer | `5000` |  | Time that the NGAP waits for a response from the AMF in milliseconds |
| `addrs` | array of string | `[127.0.1.100]` | --addr is the legacy alias of --addrs; kept for backward compatibility | AMF addresses to be used for N2 interface. Multiple addresses can be specified for SCTP multi-homing |
| `port` | integer | `38412` | 20000..40000 | AMF port |
| `bind_addrs` | array of string | `[127.0.0.1]` | --bind_addr is the legacy alias of --bind_addrs; kept for backward compatibility | CU-CP bind addresses to be used for N2 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `bind_interface` | string | `auto` |  | Network device to bind for N2 interface |


#### supported_tracking_areas

Sets the list of tracking areas supported by this AMF

_List of objects with the following items:_


##### supported_tracking_areas[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `tac` | integer | `0` | 0..16777215; values 0 and 0xfffffe are reserved | TAC to be configured |


###### plmn_list

Sets the list of PLMN items for this tracking area

_List of objects with the following items:_


###### plmn_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `plmn` | string | `` |  | PLMN to be configured |


###### tai_slice_support_list

Sets the list of TAI slices for this PLMN

_List of objects with the following items:_


###### tai_slice_support_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sst` | integer | `0` | 0..255 | Slice Service Type |
| `sd` | integer | `16777215` | 0..16777215 | Service Differentiator |


### extra_amfs

Sets the list of extra AMFs for the CU-CP to connect to

_List of objects with the following items:_


#### extra_amfs[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `addrs` | array of string | `[127.0.1.100]` | --addr is the legacy alias of --addrs; kept for backward compatibility | AMF addresses to be used for N2 interface. Multiple addresses can be specified for SCTP multi-homing |
| `port` | integer | `38412` | 20000..40000 | AMF port |
| `bind_addrs` | array of string | `[127.0.0.1]` | --bind_addr is the legacy alias of --bind_addrs; kept for backward compatibility | CU-CP bind addresses to be used for N2 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `bind_interface` | string | `auto` |  | Network device to bind for N2 interface |


##### supported_tracking_areas

Sets the list of tracking areas supported by this AMF

_List of objects with the following items:_


###### supported_tracking_areas[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `tac` | integer | `0` | 0..16777215; values 0 and 0xfffffe are reserved | TAC to be configured |


###### plmn_list

Sets the list of PLMN items for this tracking area

_List of objects with the following items:_


###### plmn_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `plmn` | string | `` |  | PLMN to be configured |


###### tai_slice_support_list

Sets the list of TAI slices for this PLMN

_List of objects with the following items:_


###### tai_slice_support_list[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sst` | integer | `0` | 0..255 | Slice Service Type |
| `sd` | integer | `16777215` | 0..16777215 | Service Differentiator |


### xnap

XNAP configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `procedure_timeout` | integer | `5000` |  | Time that the XNAP waits for a response in milliseconds |
| `reconnect_timer` | integer | `10000` |  | Time that the XNAP waits before trying to reconnect in milliseconds |
| `no_connection_init` | boolean | `false` | hidden from --help in legacy CLI11 view | When true, the CU-CP will not initiate XNAP connections, but will only accept inbound ones |


#### connections

Sets the list of XN-C peer CU-CPs for the CU-CP to connect to

_List of objects with the following items:_


##### connections[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addrs` | array of string | `[127.0.30.1]` |  | Local IP addresses to bind for XNAP interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `peer_addrs` | array of string | `[]` |  | Peer IP addresses to connect for XNAP interface |


### mobility

Mobility configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `trigger_handover_from_measurements` | boolean | `false` |  | Whether to start HO if neighbor cells become stronger |
| `trigger_cho_on_ue_setup` | boolean | `false` |  | Whether to auto-trigger CHO after UE setup when readiness checks pass |
| `cho_timeout_ms` | integer | `10000` | 1..600000 | Timeout in milliseconds used for auto-triggered CHO and as default timeout for manual CHO command |


#### cells

Sets the list of cells known to the CU-CP

_List of objects with the following items:_


##### cells[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nr_cell_id` | integer | `0` | 0..68719476735 | Cell id to be configured |
| `periodic_report_cfg_id` | integer |  | 1..64 | Periodical report configuration for the serving cell |
| `band` | integer | `auto` | set to "auto" to auto-derive | NR frequency band |
| `gnb_id_bit_length` | integer |  | 22..32 | gNodeB identifier bit length. If not set, it will be automatically set to be equal to the gNodeB Id of the CU-CP |
| `pci` | integer |  | 0..1007 | Physical Cell Id |
| `ssb_arfcn` | string | `` | integer in [0, 3279165] | SSB ARFCN |
| `ssb_scs` | integer |  | enum: 15, 30, 60, 120, 240 | SSB subcarrier spacing |
| `ssb_period` | integer |  | enum: 5, 10, 20, 40, 80, 160 | SSB period in ms |
| `ssb_offset` | integer |  |  | SSB offset |
| `ssb_duration` | integer |  | enum: 1, 2, 3, 4, 5 | SSB duration |


###### ncells

Sets the list of neighbor cells known to the CU-CP

_List of objects with the following items:_


###### ncells[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nr_cell_id` | integer | `0` | 0..68719476735 | Neighbor cell id |
| `report_configs` | array of integer | `[]` |  | Report configurations to configure for this neighbor cell |


#### report_configs

Sets report configurations

_List of objects with the following items:_


##### report_configs[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `report_cfg_id` | integer | `0` | 1..64 | Report configuration id to be configured |
| `report_type` | string | `` | enum: periodical, event_triggered, cond_trigger | Type of the report configuration |
| `event_triggered_report_type` | string | `` | one of {a1, a2, a3, a4, a5, a6, d1, t1, d2} | Type of the event triggered report |
| `report_interval_ms` | integer | `0` | enum: 120, 240, 480, 640, 1024, 2048, 5120, 10240, 20480, 40960, 60000, 360000, 720000, 1800000 | Report interval in ms |
| `periodic_ho_rsrp_offset_db` | integer | `-1` | -1..30 | Measurement trigger quantity offset in dB used to trigger handovers by periodic measurement reports. When set to -1 no handover will be triggered from periodical measurements. Note the actual value is field value * 0.5 dB |
| `meas_trigger_quantity` | string | `` | one of {rsrp, rsrq, sinr} | Measurement trigger quantity (RSRP/RSRQ/SINR) |
| `meas_trigger_quantity_threshold_db` | integer |  | -156..40 | Measurement trigger quantity threshold in dB used for measurement report trigger of event A1/A2/A4/A5Valid ranges: RSRP [-156..-31] dBm, RSRQ [-43..20] dB, SINR [-23..40] dB |
| `meas_trigger_quantity_threshold_2_db` | integer |  | -156..40 | Measurement trigger quantity threshold 2 in dB used for measurement report trigger of event A5Valid ranges: RSRP [-156..-31] dBm, RSRQ [-43..20] dB, SINR [-23..40] dB |
| `meas_trigger_quantity_offset_db` | integer |  | -15..15 | Measurement trigger quantity offset in dB used for measurement report trigger of event A3/A6. |
| `hysteresis_db` | integer |  | 0..15 | Hysteresis in dB used for measurement report trigger. |
| `time_to_trigger_ms` | integer |  | enum: 0, 40, 64, 80, 100, 128, 160, 256, 320, 480, 512, 640, 1024, 1280, 2560, 5120 | Time in ms during which a condition must be met before measurement report trigger |
| `t312` | integer |  | enum: 0, 50, 100, 200, 300, 400, 500, 1000 | T312 timer in ms. This timer is started by the UE on event triggered measurement report, when T310 (out-of-sync) timer is already running and on its expiration triggers the RLF to speed up reestablishment to different cell. |
| `distance_thresh_from_ref1_km` | number |  | 0..3276.75 | D1/D2: distance threshold 1 in km [0..3276.75] (50m steps, D1 max is 3276.25) |
| `distance_thresh_from_ref2_km` | number |  | 0..3276.75 | D1/D2: distance threshold 2 in km [0..3276.75] (50m steps, D1 max is 3276.25) |
| `hysteresis_location_km` | number |  | 0..327.68 | D1/D2: location hysteresis in km [0..327.68] (10m steps) |
| `t1_thres` | string | `` | Unix time in ms or ISO 8601 YYYY-MM-DDTHH:MM:SS[.mmm] | T1: time threshold (Unix ms integer or YYYY-MM-DDTHH:MM:SS[.mmm]) |
| `duration_s` | string | `` | decimal number in [0.1, 600] | T1: duration in seconds (each step=100ms, range [0.1..600]) |


###### ref_location1

D1: reference location 1 (serving cell)

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `latitude` | string | `` | decimal number in [-90, 90] | Latitude [degrees, -90..90] |
| `longitude` | string | `` | decimal number in [-180, 180] | Longitude [degrees, -180..180] |


###### ref_location2

D1: reference location 2 (target cell)

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `latitude` | string | `` | decimal number in [-90, 90] | Latitude [degrees, -90..90] |
| `longitude` | string | `` | decimal number in [-180, 180] | Longitude [degrees, -180..180] |


### rrc

RRC specific configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `force_reestablishment_fallback` | boolean | `false` |  | Force RRC re-establishment fallback to RRC setup |
| `force_resume_fallback` | boolean | `false` |  | Force RRC resume fallback to RRC setup |
| `rrc_procedure_guard_time_ms` | integer | `1000` |  | Guard time in ms used for RRC message exchange with UE. This is added to the RRC procedure timeout. |


### security

Security configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `integrity` | string | `not_needed` | enum: required, preferred, not_needed | Default integrity protection indication for DRBs |
| `confidentiality` | string | `required` | enum: required, preferred, not_needed | Default confidentiality protection indication for DRBs |
| `nea_pref_list` | string | `nea0,nea2,nea1,nea3` |  | Ordered preference list for the selection of encryption algorithm (NEA) (default: NEA0, NEA2, NEA1) |
| `nia_pref_list` | string | `nia2,nia1,nia3` |  | Ordered preference list for the selection of encryption algorithm (NIA) (default: NIA2, NIA1) |


## pcap

PCAP configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `ngap_filename` | string | `/tmp/cucp_ngap.pcap` |  | N3 GTP-U PCAP file output path |
| `ngap_enable` | boolean | `false` |  | Enable N3 GTP-U packet capture |
| `xnap_filename` | string | `/tmp/cu_xnap.pcap` |  | XNAP PCAP file output path |
| `xnap_enable` | boolean | `false` |  | Enable XNAP packet capture |
| `f1ap_filename` | string | `/tmp/cucp_f1ap.pcap` |  | F1AP PCAP file output path |
| `f1ap_enable` | boolean | `false` |  | Enable F1AP packet capture |
| `e1ap_filename` | string | `/tmp/cucp_e1ap.pcap` |  | E1AP PCAP file output path |
| `e1ap_enable` | boolean | `false` |  | Enable E1AP packet capture |
| `e2ap_cu_cp_filename` | string | `/tmp/cu_cp_e2ap.pcap` |  | E2AP PCAP file output path |
| `e2ap_enable` | boolean | `false` |  | Enable E2AP packet capture |


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
| `queue-size` | integer | `0` |  | RLC UM TX SDU queue size |


###### rx

UM RX parameters

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
| `queue-size` | integer | `4096` |  | RLC AM TX SDU queue size |


###### rx

AM RX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | RLC AM RX SN |
| `t-reassembly` | integer | `0` |  | RLC AM RX t-Reassembly |
| `t-status-prohibit` | integer | `0` |  | RLC AM RX t-StatusProhibit |
| `max_sn_per_status` | integer | `0` |  | RLC AM RX status SN limit |


#### pdcp

PDCP parameters


##### rohc

Header compression parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `rohc_type` | string | `none` | enum: none, rohc, uplink_only_rohc | ROHC type (none/rohc/ul_only_rohc). Values: {none, rohc, ul_only_rohc}. Default: none |
| `max_cid` | integer | `15` |  | Maximum CID |
| `profile0x0001` | boolean | `false` |  | Configure profile0x0001 (ROHCv1 RTP/UDP/IP) |
| `profile0x0002` | boolean | `false` |  | Configure profile0x0002 (ROHCv1 UDP/IP) |
| `profile0x0003` | boolean | `false` |  | Configure profile0x0003 (ROHCv1 ESP/IP) |
| `profile0x0004` | boolean | `false` |  | Configure profile0x0004 (ROHCv1 IP) |
| `profile0x0006` | boolean | `false` |  | Configure profile0x0006 (ROHCv1 TCP/IP) |
| `profile0x0101` | boolean | `false` |  | Configure profile0x0101 (ROHCv2 RTP/UDP/IP) |
| `profile0x0102` | boolean | `false` |  | Configure profile0x0102 (ROHCv2 UDP/IP) |
| `profile0x0103` | boolean | `false` |  | Configure profile0x0103 (ROHCv2 ESP/IP) |
| `profile0x0104` | boolean | `false` |  | Configure profile0x0104 (ROHCv2 IP) |


##### tx

PDCP TX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | PDCP TX SN size |
| `discard_timer` | integer | `0` |  | PDCP TX discard timer (ms) |
| `status_report_required` | boolean | `false` |  | PDCP TX status report required |


##### rx

PDCP RX parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `sn` | integer | `0` |  | PDCP RX SN size |
| `t_reordering` | integer | `0` |  | PDCP RX t-Reordering (ms) |
| `out_of_order_delivery` | boolean | `false` |  | PDCP RX enable out-of-order delivery |


## e2

E2 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_cu_cp_e2` | boolean | `false` |  | Enable CU E2 agent |
| `addrs` | array of string | `[127.0.0.1]` |  | RIC addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing |
| `port` | integer | `36421` | 20000..40000 | RIC port |
| `bind_addrs` | array of string | `[127.0.0.1]` |  | Local bind addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `e2sm_kpm_enabled` | boolean | `false` |  | Enable KPM service module |
| `e2sm_rc_enabled` | boolean | `false` |  | Enable RC service module |
| `e2sm_ccc_enabled` | boolean | `false` |  | Enable CCC service module |


