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

## `cell_cfg`

Default cell configuration.

- **`--pci`** (`int`): PCI. Range: [0, 1007].
- **`--sector_id`** (`unsigned`): Sector ID (4-14 bits). Concatenated with gNB Id to form the NR Cell Identity (NCI). Auto-derived if not specified. Range: [0, 16383].
- **`--dl_arfcn`** (`unsigned`): Downlink ARFCN.
- **`--band`** (`string`): NR band.
- **`--common_scs`** (`string`): Cell common subcarrier spacing.
- **`--channel_bandwidth_MHz`** (`unsigned`): Channel bandwidth in MHz.
- **`--nof_antennas_ul`** (`unsigned`): Number of antennas in uplink.
- **`--nof_antennas_dl`** (`unsigned`): Number of antennas in downlink.
- **`--plmn`** (`string`): PLMN.
- **`--additional_plmns`** (`string`): List of PLMNs.
- **`--tac`** (`unsigned`): TAC.
- **`--enabled`** (`bool`): Automatically activate the cell on startup.
- **`--cell_barred`** (`bool`): MIB cellBarred: if true, UEs cannot camp on this cell.
- **`--intra_freq_reselection`** (`bool`): MIB intraFreqReselection: if true, intra-frequency cell reselection is allowed when cell is barred.
- **`--q_rx_lev_min`** (`int`): q-RxLevMin: required minimum received RSRP level for cell selection/re-selection, in dBm. Range: [-70, -22].
- **`--q_qual_min`** (`int`): q-QualMin: required minimum received RSRQ level for cell selection/re-selection, in dB. Range: [-43, -12].
- **`--pcg_p_nr_fr1`** (`int`): p-nr-fr1: maximum total TX power to be used by the UE in this NR cell group across FR1. Range: [-30, 23].
- **`--slicing`** (`string`): Network slicing configuration (list of slice configs with sst, sd, and sched_cfg).

### `mac_cell_group`

MAC Cell Group parameters.

#### `bsr_cfg`

Buffer status report configuration parameters.

- **`--periodic_bsr_timer`** (`unsigned`): Periodic BSR Timer in nof. subframes. Value 0 equates to infinity.
- **`--retx_bsr_timer`** (`unsigned`): Retransmission BSR Timer in nof. subframes.
- **`--lc_sr_delay_timer`** (`unsigned`): Logical Channel SR delay timer in nof. subframes.

#### `phr_cfg`

Power Headroom report configuration parameters.

- **`--phr_prohibit_timer`** (`unsigned`): PHR prohibit timer in nof. subframes.

#### `sr_cfg`

Scheduling Request configuration parameters.

- **`--sr_trans_max`** (`unsigned`): Maximum number of SR transmissions.
- **`--sr_prohibit_timer`** (`unsigned`): Timer for SR transmission on PUCCH in ms.

### `ssb`

SSB parameters.

- **`--ssb_period`** (`unsigned`): Period of SSB scheduling in milliseconds.
- **`--ssb_block_power_dbm`** (`int`): SS_PBCH_power_block in dBm. Range: [-60, 50].
- **`--pss_to_sss_epre_db`** (`unsigned`): SSB PSS to SSS EPRE ratio in dB {0, 3}.

### `sib`

SIB configuration parameters.

- **`--si_window_length`** (`unsigned`): Length of the SI scheduling window in slots. Must be shorter or equal to SI period.
- **`--si_sched_info`** (`string`): Scheduling for each SI-message (list of {si_period, sib_mapping, si_window_position}).
- **`--t300`** (`unsigned`): RRC Connection Establishment timer in ms.
- **`--t301`** (`unsigned`): RRC Connection Re-establishment timer in ms.
- **`--t310`** (`unsigned`): Out-of-sync timer in ms.
- **`--n310`** (`unsigned`): Out-of-sync counter.
- **`--t311`** (`unsigned`): RRC Connection Re-establishment procedure timer in ms.
- **`--n311`** (`unsigned`): In-sync counter.
- **`--t319`** (`unsigned`): RRC Connection Resume timer in ms.

#### `sib2`

SIB2 parameters.

- **`--q_hyst`** (`int`): Hysteresis value for ranking criteria.
- **`--thresh_serving_low_p`** (`unsigned`): Rx level threshold for serving cell when reselecting towards a lower priority RAT/frequency. Range: [0, 31].
- **`--cell_reselection_priority`** (`unsigned`): Integer part of the cell reselection priority for the frequency of this cell. Range: [0, 7].
- **`--q_rx_lev_min`** (`int`): Minimum required Rx level in the cell in dBm.
- **`--s_intra_search_p`** (`unsigned`): Rx level threshold for intra frequency measurements in dB.
- **`--t_reselection_nr`** (`unsigned`): Cell reselection timer value in seconds. Range: [0, 7].

#### `sib3`

SIB3 parameters.

- **`--intra_freq_neigh_cell_list`** (`string`): Intra frequency neighbor cell list (list of {pci, q_offset_cell}).
- **`--intra_freq_excluded_cell_list`** (`string`): Intra frequency excluded cell list (list of {start, size}).

#### `sib4`

SIB4 parameters.

- **`--inter_freq_carrier_freq_list`** (`string`): Inter frequency carrier frequency list (list of {arfcn, ssb_scs, q_rx_lev_min, thresh_x_high_p, thresh_x_low_p, q_offset_freq}).

#### `sib5`

SIB5 parameters.

- **`--t_reselection_eutra`** (`unsigned`): Cell reselection timer value in seconds for EUTRA. Range: [0, 7].
- **`--carrier_freq_list_eutra`** (`string`): EUTRA carrier frequency list (list of {earfcn, allowed_meas_bandwidth, cell_reselection_priority, thresh_x_high, thresh_x_low, q_rx_lev_min, q_qual_min, p_max_eutra}).

#### `sib16`

SIB16 parameters.

- **`--freq_prio_list_slicing`** (`string`): Frequency priority slicing list (list of {dl_implicit_carrier_freq, slice_info_list}).

#### `etws`

ETWS configuration parameters.

- **`--message_id`** (`unsigned`): ETWS message ID.
- **`--serial_num`** (`unsigned`): ETWS message serial number.
- **`--warning_type`** (`unsigned`): ETWS warning type.
- **`--data_coding_scheme`** (`unsigned`): ETWS message CBS coding scheme.
- **`--warning_message`** (`string`): ETWS warning message.

#### `cmas`

CMAS configuration parameters.

- **`--message_id`** (`unsigned`): CMAS message ID.
- **`--serial_num`** (`unsigned`): CMAS message serial number.
- **`--data_coding_scheme`** (`unsigned`): CMAS message CBS coding scheme.
- **`--warning_message`** (`string`): CMAS warning message.

### `ul_common`

UL common parameters.

- **`--p_max`** (`int`): Maximum transmit power allowed in this serving cell. Range: [-30, 23].
- **`--max_pucchs_per_slot`** (`unsigned`): Maximum number of PUCCH grants that can be allocated per slot.
- **`--max_ul_grants_per_slot`** (`unsigned`): Maximum number of UL grants that can be allocated per slot.
- **`--min_pucch_pusch_prb_distance`** (`unsigned`): Minimum PRB distance between PUCCH and UE-dedicated PUSCH grants.

### `pdcch`

PDCCH parameters.

#### `common`

PDCCH Common configuration parameters.

- **`--coreset0_index`** (`unsigned`): CORESET#0 index. Range: [0, 15].
- **`--ss1_n_candidates`** (`string`): Number of PDCCH candidates per aggregation level for SearchSpace#1.
- **`--ss0_index`** (`unsigned`): SearchSpace#0 index. Range: [0, 15].
- **`--max_coreset0_duration`** (`unsigned`): Maximum CORESET#0 duration in OFDM symbols to consider when deriving CORESET#0 index. Range: [1, 2].

#### `dedicated`

PDCCH Dedicated configuration parameters.

- **`--coreset1_rb_start`** (`unsigned`): Starting CRB number for CORESET 1 relative to CRB 0. Range: [0, 275].
- **`--coreset1_l_crb`** (`unsigned`): Length of CORESET 1 in number of CRBs. Range: [0, 275].
- **`--coreset1_duration`** (`unsigned`): Duration of CORESET 1 in number of OFDM symbols. Range: [1, 2].
- **`--ss2_n_candidates`** (`string`): Number of PDCCH candidates per aggregation level for SearchSpace#2.
- **`--dci_format_0_1_and_1_1`** (`bool`): DCI format to use in UE dedicated SearchSpace#2.
- **`--ss2_type`** (`search_space_type`): SearchSpace type for UE dedicated SearchSpace#2. Allowed values: `common`, `ue_dedicated`.
- **`--al_cqi_offset`** (`int`): Offset to apply to the CQI value used in the PDCCH aggregation level calculation. Range: [-15, 15].

### `pdsch`

PDSCH parameters.

- **`--min_ue_mcs`** (`unsigned`): Minimum UE MCS. Range: [0, 28].
- **`--max_ue_mcs`** (`unsigned`): Maximum UE MCS. Range: [0, 28].
- **`--fixed_rar_mcs`** (`unsigned`): Fixed RAR MCS. Range: [0, 28].
- **`--fixed_sib1_mcs`** (`unsigned`): Fixed SIB1 MCS. Range: [0, 28].
- **`--harq_feedback_disabled`** (`string`): Disable DL HARQ Feedback (NTN cells). Set to true/false or a 32-bit bitmask (0x.../0b...).
- **`--nof_harqs`** (`unsigned`): Number of DL HARQ processes (32 applies only for NTN cells).
- **`--max_nof_harq_retxs`** (`unsigned`): Maximum number of times a DL HARQ can be retransmitted before being discarded. Range: [0, 64].
- **`--harq_retx_timeout`** (`unsigned`): Maximum time in ms between a HARQ NACK and the scheduler allocating the HARQ for retransmission. Range: [10, 500].
- **`--max_consecutive_kos`** (`unsigned`): Maximum number of HARQ-ACK consecutive KOs before a Radio Link Failure is reported.
- **`--rv_sequence`** (`string`): RV sequence for PDSCH (e.g. [0 2 3 1]).
- **`--mcs_table`** (`pdsch_mcs_table`): MCS table to use for PDSCH. Allowed values: `qam64`, `qam256`, `qam64lowse`.
- **`--min_rb_size`** (`unsigned`): Minimum RB size for UE PDSCH resource allocation.
- **`--max_rb_size`** (`unsigned`): Maximum RB size for UE PDSCH resource allocation.
- **`--start_rb`** (`unsigned`): Start RB for resource allocation of UE PDSCHs.
- **`--end_rb`** (`unsigned`): End RB for resource allocation of UE PDSCHs.
- **`--max_pdschs_per_slot`** (`unsigned`): Maximum number of PDSCH grants per slot, including SIB, RAR, Paging and UE data.
- **`--max_alloc_attempts`** (`unsigned`): Maximum number of DL or UL PDCCH grant allocation attempts per slot.
- **`--olla_cqi_inc_step`** (`float`): Outer-loop link adaptation (OLLA) increment value. Value 0 disables OLLA. Range: [0.0, 1.0].
- **`--olla_target_bler`** (`float`): Target DL BLER in Outer-loop link adaptation (OLLA) algorithm. Range: [0.0, 0.5].
- **`--olla_max_cqi_offset`** (`float`): Maximum offset that OLLA can apply to CQI.
- **`--dc_offset`** (`string`): DC offset in subcarriers. Set to 'outside', 'undetermined', 'center', or an integer.
- **`--harq_la_cqi_drop_threshold`** (`unsigned`): LA threshold for CQI drop above which HARQ retransmissions are cancelled (0 = disabled). Range: [0, 15].
- **`--harq_la_ri_drop_threshold`** (`unsigned`): LA threshold for nof. layers drop above which HARQ retransmission is cancelled (0 = disabled). Range: [0, 4].
- **`--dmrs_additional_position`** (`unsigned`): PDSCH DMRS additional position. Range: [0, 3].
- **`--interleaving_bundle_size`** (`unsigned`): PDSCH interleaving bundle size. Valid values: [0, 2, 4].
- **`--max_rank`** (`unsigned`): Maximum number of PDSCH transmission layers.
- **`--enable_csi_rs_pdsch_multiplexing`** (`bool`): Enable multiplexing of CSI-RS and PDSCH.

### `pusch`

PUSCH parameters.

- **`--min_ue_mcs`** (`unsigned`): Minimum UE MCS. Range: [0, 28].
- **`--max_ue_mcs`** (`unsigned`): Maximum UE MCS. Range: [0, 28].
- **`--harq_mode_b`** (`string`): Set HARQ Mode B (NTN cells). Set to true/false or a 32-bit bitmask (0x.../0b...).
- **`--nof_harqs`** (`unsigned`): Number of UL HARQ processes (32 applies only for NTN cells).
- **`--max_nof_harq_retxs`** (`unsigned`): Maximum number of times a UL HARQ can be retransmitted before being discarded. Range: [0, 64].
- **`--harq_retx_timeout`** (`unsigned`): Maximum time in ms between a CRC=KO and the scheduler allocating the HARQ for retransmission. Range: [10, 500].
- **`--max_consecutive_kos`** (`unsigned`): Maximum number of CRC consecutive KOs before a Radio Link Failure is reported.
- **`--rv_sequence`** (`string`): RV sequence for PUSCH (e.g. [0 2 3 1]).
- **`--mcs_table`** (`pusch_mcs_table`): MCS table to use for PUSCH. Allowed values: `qam64`, `qam256`, `qam64lowse`.
- **`--max_rank`** (`unsigned`): Maximum number of PUSCH transmission layers. Range: [1, 4].
- **`--msg3_delta_preamble`** (`int`): msg3-DeltaPreamble, power offset between msg3 and RACH preamble. Range: [-1, 6].
- **`--p0_nominal_with_grant`** (`int`): P0 value for PUSCH with grant (except msg3) in dBm. Must be multiple of 2 in [-202, 24].
- **`--msg3_delta_power`** (`int`): Target power level at network receiver side in dBm. Must be multiple of 2 in [-6, 8].
- **`--max_puschs_per_slot`** (`unsigned`): Maximum number of PUSCH grants per slot.
- **`--beta_offset_ack_idx_1`** (`unsigned`): betaOffsetACK-Index1 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_ack_idx_2`** (`unsigned`): betaOffsetACK-Index2 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_ack_idx_3`** (`unsigned`): betaOffsetACK-Index3 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_csi_p1_idx_1`** (`unsigned`): betaOffsetCSI-Part1-Index1 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_csi_p1_idx_2`** (`unsigned`): betaOffsetCSI-Part1-Index2 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_csi_p2_idx_1`** (`unsigned`): betaOffsetCSI-Part2-Index1 part of UCI-OnPUSCH. Range: [0, 31].
- **`--beta_offset_csi_p2_idx_2`** (`unsigned`): betaOffsetCSI-Part2-Index2 part of UCI-OnPUSCH. Range: [0, 31].
- **`--min_k2`** (`unsigned`): Minimum value of K2 (difference in slots between PDCCH and PUSCH). Range: [1, 4].
- **`--dc_offset`** (`string`): DC offset in subcarriers. Set to 'outside', 'undetermined', 'center', or an integer.
- **`--olla_snr_inc_step`** (`float`): OLLA increment value. Value 0 disables OLLA. Range: [0.0, 1.0].
- **`--olla_target_bler`** (`float`): Target UL BLER in OLLA algorithm. Range: [0.0, 0.5].
- **`--olla_max_snr_offset`** (`float`): Maximum offset that OLLA can apply to estimated UL SINR.
- **`--dmrs_additional_position`** (`unsigned`): PUSCH DMRS additional position. Range: [0, 3].
- **`--min_rb_size`** (`unsigned`): Minimum RB size for UE PUSCH resource allocation.
- **`--max_rb_size`** (`unsigned`): Maximum RB size for UE PUSCH resource allocation.
- **`--start_rb`** (`unsigned`): Start RB for resource allocation of UE PUSCHs.
- **`--end_rb`** (`unsigned`): End RB for resource allocation of UE PUSCHs.
- **`--enable_cl_loop_pw_control`** (`bool`): Enable closed-loop power control for PUSCH.
- **`--enable_phr_bw_adaptation`** (`bool`): Enable bandwidth adaptation to prevent negative PHR.
- **`--target_sinr`** (`float`): Target PUSCH SINR in dB. Range: [-5.0, 30.0].
- **`--ref_path_loss`** (`float`): Reference path-loss for target PUSCH SINR in dB. Range: [50.0, 120.0].
- **`--pl_compensation_factor`** (`float`): Fractional path-loss compensation factor in PUSCH power control.
- **`--enable_transform_precoding`** (`bool`): Enable transform precoding for PUSCH.

### `pucch`

PUCCH parameters.

- **`--p0_nominal`** (`int`): Power control P0 for PUCCH in dBm. Must be multiple of 2 in [-202, 24].
- **`--pucch_resource_common`** (`unsigned`): Index of PUCCH resource set for common configuration. Range: [0, 15].
- **`--sr_period_ms`** (`float`): SR period in ms.
- **`--formats`** (`pucch_formats`): PUCCH formats combination to use. Allowed values: `f0_and_f2`, `f1_and_f2`, `f1_and_f3`, `f1_and_f4`.
- **`--resource_set_size`** (`unsigned`): Number of PUCCH resources in each PUCCH resource set. Range: [1, 8].
- **`--nof_cell_res_set_configs`** (`unsigned`): Number of PUCCH Resource Set configurations available per cell. Range: [1, 10].
- **`--nof_cell_sr_res`** (`unsigned`): Number of PUCCH F0/F1 resources available per cell for SR. Range: [1, 100].
- **`--nof_cell_csi_res`** (`unsigned`): Number of PUCCH F2/F3/F4 resources available per cell for CSI. Range: [0, 100].
- **`--f0_intraslot_freq_hop`** (`bool`): Enable intra-slot frequency hopping for PUCCH F0.
- **`--f1_enable_occ`** (`bool`): Enable OCC for PUCCH F1.
- **`--f1_nof_cyclic_shifts`** (`unsigned`): Number of possible cyclic shifts available for PUCCH F1 resources.
- **`--f1_intraslot_freq_hop`** (`bool`): Enable intra-slot frequency hopping for PUCCH F1.
- **`--f2_max_nof_rbs`** (`unsigned`): Max number of RBs for PUCCH F2 resources. Range: [1, 16].
- **`--f2_max_payload`** (`unsigned`): Min required payload capacity in bits for PUCCH F2 resources. Range: [4, 40].
- **`--f2_max_code_rate`** (`string`): PUCCH F2 max code rate.
- **`--f2_intraslot_freq_hop`** (`bool`): Enable intra-slot frequency hopping for PUCCH F2.
- **`--f3_max_nof_rbs`** (`unsigned`): Max number of RBs for PUCCH F3 resources.
- **`--f3_max_payload`** (`unsigned`): Min required payload capacity in bits for PUCCH F3 resources. Range: [4, 40].
- **`--f3_max_code_rate`** (`string`): PUCCH F3 max code rate.
- **`--f3_intraslot_freq_hop`** (`bool`): Enable intra-slot frequency hopping for PUCCH F3.
- **`--f3_additional_dmrs`** (`bool`): Enable additional DM-RS for PUCCH F3.
- **`--f3_pi2_bpsk`** (`bool`): Enable pi/2-BPSK modulation for PUCCH F3.
- **`--f4_max_code_rate`** (`string`): PUCCH F4 max code rate.
- **`--f4_intraslot_freq_hop`** (`bool`): Enable intra-slot frequency hopping for PUCCH F4.
- **`--f4_additional_dmrs`** (`bool`): Enable additional DM-RS for PUCCH F4.
- **`--f4_pi2_bpsk`** (`bool`): Enable pi/2-BPSK modulation for PUCCH F4.
- **`--f4_occ_length`** (`unsigned`): OCC length for PUCCH F4.
- **`--f4_enable_occ`** (`bool`): Enable OCC multiplexing for PUCCH F4.
- **`--min_k1`** (`unsigned`): Minimum value of K1 (difference in slots between PDSCH and HARQ-ACK). Range: [1, 4].
- **`--max_consecutive_kos`** (`unsigned`): Maximum number of consecutive undecoded PUCCH F2 for CSI before an RLF is reported.
- **`--enable_cl_loop_pw_control`** (`bool`): Enable closed-loop power control for PUCCH.
- **`--target_sinr_f0`** (`float`): Target PUCCH F0 SINR in dB. Range: [-10.0, 20.0].
- **`--target_sinr_f2`** (`float`): Target PUCCH F2 SINR in dB. Range: [-10.0, 20.0].
- **`--target_sinr_f3`** (`float`): Target PUCCH F3 SINR in dB. Range: [-15.0, 10.0].

### `srs`

SRS parameters.

- **`--type_enabled`** (`srs_resource_type`): Enable/disable SRS and set resource type. Allowed values: `disabled`, `periodic`, `aperiodic`.
- **`--period_ms`** (`float`): SRS period in ms (for aperiodic SRS, a tentative timing).
- **`--max_nof_sym_per_slot`** (`unsigned`): Number of symbols for UL slot reserved for SRS cell resources. Range: [1, 6].
- **`--nof_sym_per_resource`** (`unsigned`): Number of symbols per SRS resource.
- **`--c_srs`** (`unsigned`): C_SRS parameter for SRS. Auto-computed from cell parameters if not set. Range: [0, 63].
- **`--freq_domain_shift`** (`unsigned`): SRS frequency domain shift (only applies if c_srs is set). Range: [0, 268].
- **`--tx_comb`** (`unsigned`): SRS TX comb size.
- **`--cyclic_shift_reuse`** (`unsigned`): SRS cyclic shift reuse factor.
- **`--sequence_id_reuse`** (`unsigned`): Enable reuse of SRS sequence id with the set reuse factor.
- **`--p0`** (`int`): P0 value for SRS in dBm. Must be multiple of 2 in [-202, 24].

### `prach`

PRACH parameters.

- **`--prach_config_index`** (`unsigned`): PRACH configuration index. Auto-derived to fit in UL slot if not set. Range: [0, 255].
- **`--prach_root_sequence_index`** (`unsigned`): PRACH root sequence index. [0,837] for format 0-3; [0,137] for other formats. Range: [0, 837].
- **`--zero_correlation_zone`** (`unsigned`): Zero correlation zone index. Range: [0, 15].
- **`--fixed_msg3_mcs`** (`unsigned`): Fixed message 3 MCS. Range: [0, 28].
- **`--max_msg3_harq_retx`** (`unsigned`): Maximum number of message 3 HARQ retransmissions. Range: [0, 4].
- **`--total_nof_ra_preambles`** (`unsigned`): Number of different contention-based PRACH preambles per occasion. Range: [1, 64].
- **`--cfra_enabled`** (`bool`): Whether to enable Contention-free Random Access (CFRA).
- **`--prach_frequency_start`** (`unsigned`): PRACH message frequency offset in PRBs. Range: [0, 274].
- **`--preamble_rx_target_pw`** (`int`): Target power level at network receiver side in dBm. Must be multiple of 2 in [-202, -60].
- **`--preamble_trans_max`** (`unsigned`): Max number of RA preamble transmissions before declaring a failure.
- **`--power_ramping_step_db`** (`unsigned`): Power ramping steps for PRACH.
- **`--ports`** (`string`): List of antenna ports.
- **`--nof_ssb_per_ro`** (`unsigned`): Number of SSBs per RACH occasion.
- **`--nof_cb_preambles_per_ssb`** (`unsigned`): Number of Contention Based preambles per SSB.
- **`--ra_resp_window`** (`unsigned`): RA-Response window length in number of slots.
- **`--nof_prach_guardbands_rbs`** (`unsigned`): Number of RBs used as guardband on each side of the PRACH RBs for short PRACH formats. Range: [1, 10].
- **`--slice_based_ra_prioritization`** (`string`): List of configurations for slice-based RA prioritization ({power_ramp_step_high_priority, scaling_factor_bi, nsag_ids}).

#### `two_step`

Two-step RACH (MsgA/MsgB) configuration.

- **`--cb_preambles_per_ssb_per_shared_ro`** (`unsigned`): Number of CB preambles per SSB per shared RACH occasion for 2-step RA. Range: [1, 60].
- **`--msgA_rsrp_thres_dbm`** (`int`): RSRP threshold in dBm above which UE selects 2-step RA over 4-step RA. Range: [-156, -29].
- **`--msgB_response_window_slots`** (`unsigned`): MsgB response window length in slots.
- **`--td_offset`** (`unsigned`): Time-domain offset in slots from PRACH slot to MsgA PUSCH slot. Range: [1, 32].
- **`--pusch_td_res_index`** (`unsigned`): Index into PUSCH-TimeDomainAllocationResource table for MsgA PUSCH scheduling.
- **`--mcs`** (`unsigned`): MCS index for MsgA PUSCH transmission. Range: [0, 28].
- **`--nof_prbs_per_msgA_po`** (`unsigned`): Number of PRBs per MsgA PUSCH occasion. Range: [1, 32].
- **`--prb_start`** (`unsigned`): Frequency offset in PRBs of the lowest MsgA PUSCH occasion from PRB 0.
- **`--po_fdm`** (`unsigned`): Number of MsgA PUSCH occasions FDMed in one time instance.

### `tdd_ul_dl_cfg`

TDD UL DL configuration parameters.

- **`--dl_ul_tx_period`** (`unsigned`): TDD pattern periodicity in slots. Range: [2, 80].
- **`--nof_dl_slots`** (`unsigned`): TDD pattern nof. consecutive full DL slots. Range: [0, 80].
- **`--nof_dl_symbols`** (`unsigned`): TDD pattern nof. DL symbols at the beginning of the slot following full DL slots. Range: [0, 13].
- **`--nof_ul_slots`** (`unsigned`): TDD pattern nof. consecutive full UL slots. Range: [0, 80].
- **`--nof_ul_symbols`** (`unsigned`): TDD pattern nof. UL symbols at the end of the slot preceding the first full UL slot. Range: [0, 13].

#### `pattern2`

TDD UL DL pattern2 configuration parameters.

- **`--dl_ul_tx_period`** (`unsigned`): TDD pattern2 periodicity in slots. Range: [2, 80].
- **`--nof_dl_slots`** (`unsigned`): TDD pattern2 nof. consecutive full DL slots. Range: [0, 80].
- **`--nof_dl_symbols`** (`unsigned`): TDD pattern2 nof. DL symbols at the beginning of the slot following full DL slots. Range: [0, 13].
- **`--nof_ul_slots`** (`unsigned`): TDD pattern2 nof. consecutive full UL slots. Range: [0, 80].
- **`--nof_ul_symbols`** (`unsigned`): TDD pattern2 nof. UL symbols at the end of the slot preceding the first full UL slot. Range: [0, 13].

### `paging`

Paging parameters.

- **`--pg_search_space_id`** (`unsigned`): SearchSpace to use for Paging.
- **`--default_pg_cycle_in_rf`** (`unsigned`): Default Paging cycle in nof. Radio Frames.
- **`--nof_pf_per_paging_cycle`** (`paging_nof_pf_per_drx_cycle`): Number of paging frames per DRX cycle. Allowed values: `oneT`, `halfT`, `quarterT`, `oneEighthT`, `oneSixteethT`.
- **`--pf_offset`** (`unsigned`): Paging frame offset.
- **`--nof_po_per_pf`** (`unsigned`): Number of paging occasions per paging frame.
- **`--edrx_enabled`** (`bool`): Enable eDRX.

### `csi`

CSI-Meas parameters.

- **`--csi_rs_enabled`** (`bool`): Enable CSI-RS resources and CSI reporting.
- **`--csi_rs_period`** (`unsigned`): CSI-RS period in milliseconds.
- **`--report_type`** (`csi_report_type`): Type of CSI reporting configuration to use. Allowed values: `periodic`, `aperiodic`.
- **`--meas_csi_rs_slot_offset`** (`unsigned`): Slot offset of first CSI-RS resource used for measurement.
- **`--tracking_csi_rs_slot_offset`** (`unsigned`): Slot offset of first CSI-RS resource used for tracking.
- **`--zp_csi_rs_slot_offset`** (`unsigned`): Slot offset of the ZP CSI-RS resources.
- **`--pwr_ctrl_offset`** (`int`): powerControlOffset: power offset of PDSCH RE to NZP CSI-RS RE in dB. Range: [-8, 15].

### `scheduler`

Scheduler parameters.

- **`--nof_preselected_newtx_ues`** (`unsigned`): Number of UEs pre-selected for potential newTx allocations in a slot.

#### `policy`

Scheduler policy configuration. By default, time-domain QoS-aware policy is used.

##### `qos_sched`

Time-domain QoS-aware policy configuration.

- **`--combine_function`** (`qos_combine_function_type`): QoS-aware scheduler policy weight combining function. Allowed values: `gbr_prioritized`, `geometric_mean`.
- **`--pf_fairness_coeff`** (`float`): Fairness Coefficient to use in Proportional Fair (PF) weight.
- **`--prio_enabled`** (`bool`): Whether to take into account the QoS Flow priority in QoS-aware scheduling.
- **`--pdb_enabled`** (`bool`): Whether to take into account the QoS Flow Packet Delay Budget (PDB) in QoS-aware scheduling.
- **`--gbr_enabled`** (`bool`): Whether to take into account the QoS Flow Guaranteed Bit Rate (GBR) in QoS-aware scheduling.

##### `rr_sched`

Time-domain Round-robin policy configuration.

### `ta`

Time Advance (TA) parameters.

- **`--ta_measurement_slot_period`** (`unsigned`): Measurements periodicity in slots over which the new TA Command is computed.
- **`--ta_measurement_slot_prohibit_period`** (`unsigned`): Delay in slots between issuing TA_CMD and starting TA measurements. Range: [0, 10000].
- **`--ta_cmd_offset_threshold`** (`int`): TA offset threshold above which TA Command is triggered. Set to -1 to disable. Range: [-1, 31].
- **`--ta_target`** (`float`): Timing Advance target in units of TA. Range: [-30.0, 30.0].
- **`--ta_update_measurement_ul_sinr_threshold`** (`float`): UL SINR threshold (in dB) above which N_TA update measurement is considered valid.
- **`--ta_outlier_detection_zscore_threshold`** (`float`): Z-score threshold for outlier detection in N_TA measurements. 0.0 disables detection. Range: [0.0, 5.0].

### `drx`

DRX parameters.

- **`--on_duration_timer`** (`unsigned`): Minimum duration in ms that UE stays active when DRX is configured.
- **`--inactivity_timer`** (`unsigned`): Duration in ms that UE stays active after PDCCH reception when DRX is configured.
- **`--retx_timer_dl`** (`unsigned`): Maximum duration in slots until a DL ReTX is received by the UE when DRX is configured.
- **`--retx_timer_ul`** (`unsigned`): Maximum duration in slots until a grant for UL ReTX is received by the UE when DRX is configured.
- **`--long_cycle`** (`unsigned`): Duration in ms between UE DRX long cycles. Value 0 disables DRX.

### `rlm`

Radio Link Monitoring parameters.

- **`--rlm_resource_type`** (`rlm_resource_type`): Radio Link Monitoring resource detection type. Allowed values: `default_type`, `ssb`, `csi_rs`, `ssb_and_csi_rs`.

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

