# CU-UP configuration reference
| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `dryrun` | boolean | `false` |  | Enable application dry run mode |
| `gnb_id` | integer | `411` |  | gNodeB identifier |
| `gnb_id_bit_length` | integer | `22` | 22..32 | gNodeB identifier length in bits |
| `gnb_cu_up_id` | integer | `0` | 0..68719476735 | gNB-CU-UP Id |


## log

Logging configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `/tmp/cu_up.log` |  | Log file output path |
| `hex_max_size` | integer | `0` | -1..1024 | Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes) |
| `e1ap_json_enabled` | boolean | `false` |  | Enable JSON logging of E1AP PDUs |


## trace

General tracer configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `filename` | string | `` |  | Set to a valid file path to enable tracing and write the trace to the file |
| `max_tracing_events_per_file` | integer | `1000000` |  | Maximum number of events per file. Set to zero for no limit |
| `nof_tracing_events_after_severe` | integer | `0` |  | Number of events to write prior to a severe event. Set to zero for writing all events |


### layers

Metrics configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cu_up_enable` | boolean | `false` |  | Enable tracing for CU-UP executors |


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
| `enable_e1ap` | boolean | `false` |  | Enable E1AP metrics |
| `enable_pdcp` | boolean | `false` |  | Enable PDCP metrics |
| `enable_nrup_cu` | boolean | `false` |  | Enable NRUP metrics (CU side) |
| `skip_cu_up_executor` | boolean | `true` |  | Whether to skip logging CU-UP executor metrics when executor logging is enabled application wide |


### periodicity

Metrics periodicity configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `app_usage_report_period` | integer | `1000` |  | Application resource usage metrics report period (in milliseconds) |
| `cu_up_report_period` | integer | `1000` |  | CU-UP metrics report period in milliseconds |


## expert_execution

Expert execution configuration


### affinities

Application CPU affinities configuration


### threads

Threads configuration


#### main_pool

Main thread pool configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `nof_threads` | integer |  |  | Number of threads for processing upper PHY and upper layers. |
| `task_queue_size` | integer | `2048` |  | Main thread pool task queue size. |
| `backoff_period` | integer | `50` |  | Main thread pool back-off period, in microseconds. |


### queues

Task executor queue parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `cu_up_dl_ue_executor_queue_size` | integer | `8192` |  | CU-UP's DL UE executor queue size |
| `cu_up_ul_ue_executor_queue_size` | integer | `8192` |  | CU-UP's UL UE executor queue size |
| `cu_up_ctrl_ue_executor_queue_size` | integer | `8192` |  | CU-UP's CTRL UE executor queue size |
| `cu_up_strand_batch_size` | integer | `256` |  | CU-UP's strands batch size |


## remote_control

Remote control configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enabled` | boolean | `false` |  | Enables the Remote Control Server |
| `bind_addr` | string | `127.0.0.1` |  | Remote Control Server bind address |
| `port` | integer | `8001` | 0..65535 | Port where the remote control server listens for incoming connections |


## cu_up

CU-UP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `warn_on_drop` | boolean | `false` |  | Log a warning for dropped packets in GTP-U, SDAP, PDCP and F1-U due to full queues |
| `max_nof_ues` | integer | `16384` | 1..65536 | Maximum number of Bearer Contexts allowed by the CU-UP |


### e1ap

E1AP parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `addrs` | array of string | `[127.0.20.1]` |  | CU-CP addresses to be used for E1 interface. Multiple addresses can be specified for SCTP multi-homing |
| `bind_addrs` | array of string | `[127.0.20.2]` |  | CU-UP bind addresses to be used for E1 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `sctp_rto_initial` | integer | `120` |  | SCTP initial RTO value in milliseconds (-1 to use system default) |
| `sctp_rto_min` | integer | `120` |  | SCTP RTO min in milliseconds (-1 to use system default) |
| `sctp_rto_max` | integer | `500` |  | SCTP RTO max in milliseconds (-1 to use system default) |
| `sctp_init_max_attempts` | integer | `3` |  | SCTP init max attempts (-1 to use system default) |
| `sctp_max_init_timeo` | integer | `500` |  | SCTP max init timeout in milliseconds (-1 to use system default) |
| `sctp_hb_interval` | integer | `30000` |  | SCTP heartbeat interval in milliseconds (-1 to use system default) |
| `sctp_assoc_max_retx` | integer | `10` |  | SCTP association max retransmissions (-1 to use system default) |
| `sctp_nodelay` | boolean | `false` |  | Send SCTP messages as soon as possible without any Nagle-like algorithm |


### f1u

F1-U parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_port` | integer | `2152` |  | F1-U bind port |
| `peer_port` | integer | `2152` |  | F1-U peer port |


#### socket

Configures UDP/IP socket parameters of the F1-U interface

_List of objects with the following items:_


##### socket[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addr` | string | `127.0.10.1` | must be a valid IPv4 address | Default local IP address interfaces bind to, unless a specific bind address is specified |
| `sst` | integer |  | 0..255 | Slice Service Type |
| `sd` | integer |  | 0..16777214 | Service Differentiator |
| `five_qi` | integer |  | 0..255 | Assign this socket to a specific 5QI |
| `ext_addr` | string | `auto` | must be a valid IPv4 address or "auto" | External IP address that is advertised for receiving UDP packets. |


###### udp

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


### ngu

NG-U parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `no_core` | boolean | `false` |  | Allow gNB to run without a core |


#### gtpu

CU-UP NG-U GTP-U parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `queue_size` | integer | `2046` |  | GTP-U queue size, in PDUs |
| `batch_size` | integer | `256` |  | Maximum number of GTP-U PDUs processed in a batch |
| `reordering_timer` | integer | `0` |  | GTP-U RX reordering timer (in milliseconds) |
| `rate_limiter_period` | integer | `100` |  | GTP-U RX rate limiter period (in milliseconds) |
| `teid_release_linger_time` | integer | `100` |  | Error indication suppression time for released TEIDs (in milliseconds) |
| `ignore_ue_ambr` | boolean | `true` |  | Ignore GTP-U DL UE-AMBR rate limiter |


#### socket

Configures UDP/IP socket parameters of the N3 interface

_List of objects with the following items:_


##### socket[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `bind_addr` | string | `127.0.0.1` | must be a valid IPv4 address or "auto" | Local IP address to bind for N3 interface |
| `bind_interface` | string | `auto` |  | Network device to bind for N3 interface |
| `ext_addr` | string | `auto` | must be a valid IPv4 address or "auto"; must be a valid IPv4 address or "auto" | External IP address that is advertised to receive GTP-U packets from UPF via N3 interface |


###### udp

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


### test_mode

CU-UP test mode parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable` | boolean | `false` |  | Enable or disable CU-UP test mode |
| `integrity_enable` | boolean | `true` |  | Enable or disable PDCP integrity testing |
| `ciphering_enable` | boolean | `true` |  | Enable or disable PDCP ciphering testing |
| `nea_algo` | integer | `2` | 0..3 | NEA algo to use for testing. Valid values {0, 1, 2, 3}. |
| `nia_algo` | integer | `2` | 1..3 | NIA algo to use for testing. Valid values {1, 2, 3}. |
| `ue_ambr` | integer | `40000000000` |  | DL UE-AMBR used for testing in bps |
| `attach_detach_period` | integer | `0` |  | Attach/detach period for test mode. 0 means always attached |
| `reestablish_period` | integer | `0` |  | Reestablish period for test mode. 0 means always attached |
| `f1u_peer_address` | string | `127.0.10.2` |  | Address for DL F1-U packets for test mode |
| `nof_ues` | integer | `1` |  | Number of UEs used for test mode |


## pcap

Logging configuration

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `n3_filename` | string | `/tmp/cu-up_n3.pcap` |  | N3 GTP-U PCAP file output path |
| `n3_enable` | boolean | `false` |  | Enable N3 GTP-U packet capture |
| `f1u_filename` | string | `/tmp/cu-up_f1u.pcap` |  | F1-U GTP-U PCAP file output path |
| `f1u_enable` | boolean | `false` |  | F1-U GTP-U PCAP |
| `e1ap_filename` | string | `/tmp/cu-up_e1ap.pcap` |  | E1AP PCAP file output path |
| `e1ap_enable` | boolean | `false` |  | E1AP PCAP |
| `e2ap_cu_up_filename` | string | `/tmp/cu_up_e2ap.pcap` |  | E2AP PCAP file output path |
| `e2ap_enable` | boolean | `false` |  | Enable E2AP packet capture |


## qos

Configures RLC and PDCP radio bearers on a per 5QI basis.

_List of objects with the following items:_


### qos[]

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `five_qi` | integer | `9` | 0..255 | 5QI |


#### f1u_cu_up

F1-U parameters at CU_UP side

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `backoff_timer` | integer | `5` |  | F1-U backoff timer (ms) |
| `queue_size` | integer | `8192` |  | F1-U backoff timer (ms) |
| `batch_size` | integer | `256` |  | F1-U backoff timer (ms) |


## e2

E2 parameters

| Option | Type | Default | Constraints | Description |
|--------|------|---------|-------------|-------------|
| `enable_cu_up_e2` | boolean | `false` |  | Enable CU-UP E2 agent |
| `addrs` | array of string | `[127.0.0.1]` |  | RIC addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing |
| `port` | integer | `36421` | 20000..40000 | RIC port |
| `bind_addrs` | array of string | `[127.0.0.1]` |  | Local bind addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing. If left empty, implicit bind is performed |
| `sctp_rto_initial` | integer | `120` |  | SCTP initial RTO value in milliseconds (-1 to use system default) |
| `sctp_rto_min` | integer | `120` |  | SCTP RTO min in milliseconds (-1 to use system default) |
| `sctp_rto_max` | integer | `500` |  | SCTP RTO max in milliseconds (-1 to use system default) |
| `sctp_init_max_attempts` | integer | `3` |  | SCTP init max attempts (-1 to use system default) |
| `sctp_max_init_timeo` | integer | `500` |  | SCTP max init timeout in milliseconds (-1 to use system default) |
| `sctp_hb_interval` | integer | `30000` |  | SCTP heartbeat interval in milliseconds (-1 to use system default) |
| `sctp_assoc_max_retx` | integer | `10` |  | SCTP association max retransmissions (-1 to use system default) |
| `sctp_nodelay` | boolean | `false` |  | Send SCTP messages as soon as possible without any Nagle-like algorithm |
| `e2sm_kpm_enabled` | boolean | `false` |  | Enable KPM service module |
| `e2sm_rc_enabled` | boolean | `false` |  | Enable RC service module |
| `e2sm_ccc_enabled` | boolean | `false` |  | Enable CCC service module |


