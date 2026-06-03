# RRC UE Layer

The RRC UE layer implements the per-UE Radio Resource Control state machine as defined in **3GPP TS 38.331**. Each connected UE is represented by an `rrc_ue_impl` instance that owns the UE's RRC context, SRB lifecycle, and the coroutine-based procedures that drive state transitions.

Responsibilities:

- Encode and decode RRC messages (DL/UL-CCCH and DL/UL-DCCH) via the ASN.1 codec helpers.
- Manage the UE's RRC state (`idle → connected → inactive`) and the associated SRB bearers.
- Run procedures as async coroutine tasks, suspending on UE responses and resuming on receipt of the matching UL-DCCH message.
- Forward NAS PDUs and location information to the NGAP layer via `rrc_ue_ngap_notifier`.
- Notify the CU-CP orchestration layer of context updates via `rrc_ue_context_update_notifier`.

The public contract of the layer is expressed through the interfaces in `include/ocudu/rrc/rrc_ue.h`. Inbound messages arrive through `rrc_ul_pdu_handler` and `rrc_ngap_message_handler`; outbound messages leave through the corresponding notifier interfaces.

---

## Procedures

Procedures live in [`procedures/`](procedures/) and follow a common coroutine pattern: send a downlink message, suspend on a transaction, validate the response, and notify upstream components on success or trigger a UE release on failure. Transaction timeouts are governed by the 3GPP timer specified for each procedure (e.g. T300 for RRC Setup).

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| RRC Setup | `rrc_setup_procedure.cpp` | TS 38.331 §5.3.3 |
| RRC Reconfiguration | `rrc_reconfiguration_procedure.cpp` | TS 38.331 §5.3.5 |
| RRC Reestablishment | `rrc_reestablishment_procedure.cpp` | TS 38.331 §5.3.7 |
| RRC Resume | `rrc_resume_procedure.cpp` | TS 38.331 §5.3.13 |
| UE Capability Transfer | `rrc_ue_capability_transfer_procedure.cpp` | TS 38.331 §5.6.1 |

### RRC Setup

Establishes the initial RRC connection. The procedure creates SRB1, sends an `RRCSetup` message to the UE, and waits for `RRCSetupComplete`. On success it validates the selected PLMN, transitions the UE to `connected`, and forwards the NAS PDU to the AMF via an NGAP Initial UE Message. For the full cross-layer flow see the [CU-CP message flows](../../cu_cp/README.md#rrc-setup).

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant SRB as SRB Handler
    participant CUCPP as CU-CP
    participant Metrics
    participant NGAP

    UE->>RRC: RRCSetupRequest (UL-CCCH)
    RRC->>SRB: create_srb(SRB1)
    Note over SRB: Create PDCP entity for SRB1
    RRC->>UE: RRCSetup (DL-CCCH)

    Note over RRC: Await RRCSetupComplete (T300)

    UE->>RRC: RRCSetupComplete (UL-DCCH)
    Note over RRC: Validate selected PLMN
    RRC->>CUCPP: on_ue_setup_complete_received()
    Note over CUCPP: Verify PLMN is supported
    Note over RRC: UE state → Connected
    RRC->>Metrics: on_successful_rrc_connection_establishment()
    RRC->>Metrics: on_new_rrc_connection()
    Note over Metrics: Record connection metrics
    RRC->>NGAP: on_initial_ue_message()
    Note over NGAP: Forward NAS PDU + location info to AMF
```

### RRC Reconfiguration

Modifies the UE's radio configuration while connected — used to add or release SRBs/DRBs, update measurement config, or deliver a handover command (TS 38.331 §5.3.5). The procedure creates any new SRBs first, then sends the `RRCReconfiguration` and awaits the complete. Timer T311 governs the wait.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant SRB as SRB Handler

    Note over RRC: Triggered by CU-CP (bearer setup, HO, etc.)
    RRC->>SRB: create_srb() [if new SRBs requested]
    Note over SRB: Create PDCP entity for new SRB
    RRC->>UE: RRCReconfiguration (DL-DCCH SRB1)

    Note over RRC: Await RRCReconfigurationComplete (T311)

    UE->>RRC: RRCReconfigurationComplete
```

### RRC Reestablishment

Re-establishes the RRC connection after a radio link failure (TS 38.331 §5.3.7). The procedure verifies the ShortMAC-I against the old UE's security context, transfers the UP and security contexts to the new UE instance, creates SRB1 with active PDCP security, and triggers a UE context modification at the DU after success.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant SRB as SRB Handler
    participant CUCPP as CU-CP
    participant CUCPPue as CU-CP UE

    UE->>RRC: RRCReestablishmentRequest (UL-CCCH)
    RRC->>CUCPP: on_rrc_reestablishment_request()
    Note over CUCPP: Look up old UE context by C-RNTI/PCI
    RRC->>CUCPP: on_ue_transfer_required()
    Note over CUCPP: Transfer context from old UE
    RRC->>CUCPP: on_up_context_setup_required()
    Note over CUCPP: Transfer UP bearer context
    RRC->>CUCPPue: update_security_context()
    RRC->>CUCPPue: perform_horizontal_key_derivation()
    Note over CUCPPue: Derive new AS keys (horizontal)
    RRC->>SRB: create_srb(SRB1)
    Note over SRB: Create PDCP entity with security active
    RRC->>UE: RRCReestablishment (DL-DCCH SRB1)

    Note over RRC: Await RRCReestablishmentComplete (T311)

    UE->>RRC: RRCReestablishmentComplete
    Note over RRC: UE state → Connected
    RRC->>CUCPP: on_rrc_reestablishment_context_modification_required()
    Note over CUCPP: Trigger UE context modification at DU
    RRC->>CUCPP: on_rrc_reestablishment_complete()
    Note over CUCPP: Remove old UE context
```

### RRC Resume

Resumes a UE from RRC Inactive state (TS 38.331 §5.3.13). The procedure verifies the ResumeMAC-I, derives new AS keys, reestablishes all SRBs, and sends any pending downlink NAS PDUs once the UE is connected again.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant CUCPPue as CU-CP UE
    participant CUCPP as CU-CP
    participant Metrics

    UE->>RRC: RRCResumeRequest (UL-CCCH)
    RRC->>CUCPPue: get_security_context()
    Note over RRC: Verify ResumeMAC-I
    RRC->>CUCPPue: perform_horizontal_key_derivation()
    Note over CUCPPue: Derive new AS keys (horizontal)
    Note over RRC: Reestablish all SRBs with new keys
    RRC->>CUCPP: on_rrc_resume_request()
    Note over CUCPP: Restore UE context from inactive state
    RRC->>UE: RRCResume (DL-DCCH SRB1)

    Note over RRC: Await RRCResumeComplete (T311)

    UE->>RRC: RRCResumeComplete
    Note over RRC: UE state → Connected
    RRC->>Metrics: on_successful_rrc_connection_resume()
    Note over Metrics: Record resume metrics
    RRC->>UE: DLInformationTransfer [pending NAS PDUs, if any]
```

### UE Capability Transfer

Queries and stores the UE's radio access capabilities (TS 38.331 §5.6.1). Skipped if capabilities are already present in the UE context.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE

    Note over RRC: Triggered by CU-CP after connection established
    RRC->>UE: UECapabilityEnquiry (DL-DCCH SRB1)

    Note over RRC: Await UECapabilityInformation

    UE->>RRC: UECapabilityInformation
    Note over RRC: Parse and store NR capabilities
```
