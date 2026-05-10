# GNB Application

- **`-c,--config`** (`string`): Configuration file path (INI or YAML format).
- **`-v,--version`**: Print version information and exit.
- **`--dryrun`** (`bool`): Enable application dry run mode. Default: `false`.
- **`--gnb_id`** (`unsigned`): gNodeB identifier.
- **`--gnb_id_bit_length`** (`unsigned`): gNodeB identifier length in bits. Range: [22, 32].
- **`--ran_node_name`** (`string`): RAN node name.

## `log`

Logging configuration.

- **`--mac_level`**: MAC log level.
- **`--rlc_level`**: RLC log level.
- **`--f1ap_level`**: F1AP log level.
- **`--f1u_level`**: F1-U log level.
- **`--gtpu_level`**: GTPU log level.
- **`--ntn_level`**: NTN log level.
- **`--du_level`**: Log level for the DU.
- **`--hex_max_size`** (`int`): Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes). Range: [-1, 1024].
- **`--broadcast_enabled`** (`bool`): Enable logging in the physical and MAC layer of broadcast messages and all PRACH opportunities.
- **`--f1ap_json_enabled`** (`bool`): Enable JSON logging of F1AP PDUs.
- **`--high_latency_diagnostics_enabled`** (`bool`): Log performance diagnostics when high computational latencies are detected.
- **`--fapi_level`**: FAPI log level.

## `trace`

General tracer configuration.

- **`--filename`** (`string`): Set to a valid file path to enable tracing and write the trace to the file.
- **`--max_tracing_events_per_file`** (`unsigned`): Maximum number of events per file. Set to zero for no limit. Default: `1000000`.
- **`--nof_tracing_events_after_severe`** (`unsigned`): Number of events to write prior to a severe event. Set to zero for writing all events. Default: `0`.

### `layers`

Layer basis tracing configuration.

- **`--du_high_enable`** (`bool`): Enable tracing for DU-high executors.

## `metrics`

Metrics configuration.

- **`--autostart_stdout_metrics`** (`bool`): Autostart stdout metrics reporting. Default: `false`.
- **`--enable_json`** (`bool`): Enables the metrics in JSON format. Default: `false`.
- **`--enable_log`** (`bool`): Enables the metrics in the log. Default: `false`.
- **`--enable_verbose`** (`bool`): Enable extended detail metrics reporting. Default: `false`.

### `periodicity`

Metrics periodicity configuration.

- **`--du_report_period`** (`unsigned`): DU statistics report period in milliseconds.
- **`--executors_report_period`**: Executors metrics report period in milliseconds.
- **`--app_usage_report_period`** (`unsigned`): Application usage report period in milliseconds. Default: `1000`.

### `layers`

Layer basis metrics configuration.

- **`--enable_sched`** (`bool`): Enable DU scheduler metrics.
- **`--enable_rlc`** (`bool`): Enable RLC metrics.
- **`--enable_mac`** (`bool`): Enable MAC metrics.
- **`--enable_du_proc`** (`bool`): Enable DU management and control procedure metrics.
- **`--enable_app_usage`** (`bool`): Enable application usage metrics. Default: `false`.
- **`--enable_executor`** (`bool`): Whether to log application executors metrics. Default: `false`.

## `pcap`

PCAP configuration.

- **`--f1ap_filename`** (`string`): F1AP PCAP file output path.
- **`--f1ap_enable`** (`bool`): Enable F1AP packet capture.
- **`--f1u_filename`** (`string`): F1-U PCAP file output path.
- **`--f1u_enable`** (`bool`): Enable F1-U packet capture.
- **`--rlc_filename`** (`string`): RLC PCAP file output path.
- **`--rlc_rb_type`** (`string`): RLC PCAP RB type (all, srb, drb).
- **`--rlc_enable`** (`bool`): Enable RLC packet capture.
- **`--mac_filename`** (`string`): MAC PCAP file output path.
- **`--mac_type`** (`string`): MAC PCAP pcap type (dlt or udp).
- **`--mac_enable`** (`bool`): Enable MAC packet capture.
- **`--e2ap_cu_cp_filename`** (`string`): E2AP PCAP file output path.
- **`--e2ap_enable`** (`bool`): Enable E2AP packet capture.
- **`--e2ap_cu_up_filename`** (`string`): E2AP PCAP file output path.
- **`--e2ap_du_filename`** (`string`): E2AP PCAP file output path.

## `du`

DU parameters.

- **`--warn_on_drop`** (`bool`): Log a warning for dropped packets in F1-U, RLC and MAC due to full queues.

## `expert_execution`

Expert execution configuration.

### `queues`

Task executor queue parameters.

- **`--du_ue_data_executor_queue_size`** (`unsigned`): DU's UE executor task queue size for PDU processing.

## `test_mode`

Test mode configuration.

### `test_ue`

Automatically created UE for testing purposes.

- **`--rnti`**: C-RNTI (0x0 if not configured).
- **`--nof_ues`**: Number of test UE(s) to create.
- **`--ue_creation_stagger_slots`** (`unsigned`): Number of slots between consecutive test mode UE creations. Range: [0, 10240].
- **`--auto_ack_indication_delay`** (`optional<unsigned>`): Delay before the UL and DL HARQs are automatically ACKed.
- **`--attach_detach_duration_ms`** (`optional<unsigned>`): Duration in milliseconds of active traffic before UEs are released and recreated. Range: [100, 10000].
- **`--attach_detach_guard_duration_ms`** (`unsigned`): Guard period duration in milliseconds between a release cycle and the next creation cycle. Range: [100, 60000].
- **`--pdsch_active`** (`bool`): PDSCH enabled.
- **`--pusch_active`** (`bool`): PUSCH enabled.
- **`--cqi`** (`unsigned`): Channel Quality Information (CQI) to be forwarded to test UE. Range: [1, 15].
- **`--ri`** (`unsigned`): Rank Indicator (RI) to be forwarded to test UE. Range: [1, 4].
- **`--pmi`** (`unsigned`): Precoder Matrix Indicator (PMI) to be forwarded to test UE. Range: [0, 3].
- **`--i_1_1`** (`unsigned`): Precoder Matrix codebook index "i_1_1" to be forwarded to test UE. Range: [0, 7].
- **`--i_1_3`** (`unsigned`): Precoder Matrix codebook index "i_1_3" to be forwarded to test UE. Range: [0, 1].
- **`--i_2`** (`unsigned`): Precoder Matrix codebook index "i_2" to be forwarded to test UE. Range: [0, 3].

## `buffer_pool`

Buffer pool configuration.

- **`--nof_segments`** (`size_t`): Number of segments allocated by the buffer pool.
- **`--segment_size`** (`size_t`): Size of each buffer pool segment in bytes.

## `affinities`

Application CPU affinities configuration.

- **`--main_pool_cpus`**: CPU cores assigned to main thread pool.
- **`--main_pool_pinning`**: Policy used for assigning CPU cores to the main thread pool.

## `threads`

Threads configuration.

### `main_pool`

Main thread pool configuration.

- **`--nof_threads`** (`optional<unsigned>`): Number of threads for processing upper PHY and upper layers..
- **`--task_queue_size`** (`unsigned`): Main thread pool task queue size..
- **`--backoff_period`** (`unsigned`): Main thread pool back-off period, in microseconds..

## `hal`

HAL configuration.

- **`--eal_args`** (`string`): EAL configuration parameters used to initialize DPDK.

## `remote_control`

Remote control configuration.

- **`--enabled`** (`bool`): Enables the Remote Control Server. Default: `false`.
- **`--bind_addr`** (`string`): Remote Control Server bind address. Default: `"127.0.0.1"`.
- **`--port`** (`uint16_t`): Port where the remote control server listens for incoming connections. Default: `8001` | Range: [0, 65535].

## `udp`

UDP parameters.

- **`--max_rx_msgs`** (`unsigned`): Maximum amount of messages RX in a single syscall. Default: `256`.
- **`--tx_qsize`** (`unsigned`): Batched queue size. Default: `4096`.
- **`--max_tx_msgs`** (`unsigned`): Maximum amount of messages TX in a single syscall. Default: `256`.
- **`--max_tx_segments`** (`unsigned`): Maximum amount of segments in a single TX SDU. Default: `256`.
- **`--pool_threshold`** (`float`): Pool accupancy threshold after which packets are dropped. Default: `0.9`.
- **`--reuse_addr`** (`bool`): Allow multiple sockets to re-use the bind port. Default: `false`.
- **`--dscp`** (`optional<unsigned>`): Differentiated Services Code Point value. Range: [0, 63].

