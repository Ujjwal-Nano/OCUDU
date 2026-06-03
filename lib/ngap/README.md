# NGAP

The NGAP layer implements the N2 control plane interface between the CU-CP and the AMF (TS 38.413). It maintains one `ngap_impl` instance per AMF connection and one `ngap_ue_context` per connected UE, identified by the RAN-UE-NGAP-ID / AMF-UE-NGAP-ID pair.

Responsibilities:

- Manage the NG association lifecycle (NG Setup, NG Reset).
- Dispatch incoming NGAP PDUs to per-UE or per-association procedure coroutines.
- Translate between NGAP message types and the CU-CP's internal message types via `ngap_cu_cp_notifier`.
- Forward NAS PDUs to the RRC UE layer via `ngap_rrc_ue_notifier`.
- Report PDU session setup/failure metrics via `ngap_metrics_aggregator`.

The public contract is expressed through `include/ocudu/ngap/ngap.h`.

---

## Procedures

### Association Management

Procedures that establish and reset the N2 connection between the CU-CP and the AMF.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| NG Setup | `ng_setup_procedure.cpp` | TS 38.413 §8.7.1 |
| NG Reset | `ng_reset_procedure.cpp` | TS 38.413 §8.7.4 |

#### NG Setup

Establishes the N2 association with the AMF (TS 38.413 §8.7.1). Retries with the AMF-provided back-off timer up to a configurable maximum.

```mermaid
sequenceDiagram
    participant NGAP
    participant AMF

    NGAP->>AMF: NGSetupRequest
    Note over AMF: Validate gNB configuration
    AMF->>NGAP: NGSetupResponse
    Note over NGAP: Store GUAMI list and PLMN support
```

#### NG Reset

Resets the NG association and all associated UE state (TS 38.413 §8.7.4).

```mermaid
sequenceDiagram
    participant NGAP
    participant AMF

    NGAP->>AMF: NGReset
    Note over AMF: Reset NG association state
    AMF->>NGAP: NGResetAcknowledge
```

### NAS Transport

Procedure that delivers downlink NAS PDUs from the AMF to the UE via the RRC layer.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| DL NAS Transfer | `ngap_dl_nas_message_transfer_procedure.cpp` | TS 38.413 §8.6.2 |

#### DL NAS Transfer

Forwards a downlink NAS PDU from the AMF to the UE via the RRC layer (TS 38.413 §8.6.2). Optionally triggers a UE Radio Capability Info Indication if the AMF requests it.

```mermaid
sequenceDiagram
    participant RRC as RRC UE
    participant NGAP
    participant AMF

    AMF->>NGAP: DLNASTransport
    NGAP->>RRC: on_new_pdu() [NAS PDU]
    Note over RRC: Forward NAS PDU to UE via SRB
```

### UE Context Management

Procedures that create, modify, and release the UE context at the AMF.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| Initial Context Setup | `ngap_initial_context_setup_procedure.cpp` | TS 38.413 §8.3.1 |
| UE Context Modification | `ngap_ue_context_modification_procedure.cpp` | TS 38.413 §8.3.2 |
| UE Context Release | `ngap_ue_context_release_procedure.cpp` | TS 38.413 §8.3.3 |

#### Initial Context Setup

Activates the UE's security context and optionally establishes PDU sessions, triggered by the AMF after NAS authentication (TS 38.413 §8.3.1).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: InitialContextSetupRequest
    NGAP->>CU-CP: on_new_initial_context_setup_request()
    Note over CU-CP: Activate security, setup bearers
    CU-CP->>NGAP: InitialContextSetupResponse
    NGAP->>AMF: InitialContextSetupResponse
```

#### UE Context Modification

Modifies an existing UE context at the request of the AMF, for example to update security or bearer configuration (TS 38.413 §8.3.2).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: UEContextModificationRequest
    NGAP->>CU-CP: on_new_ue_context_modification_request()
    Note over CU-CP: Apply security and bearer modifications
    CU-CP->>NGAP: UEContextModificationResponse
    NGAP->>AMF: UEContextModificationResponse
```

#### UE Context Release

Releases the UE context following an AMF-initiated release command (TS 38.413 §8.3.3).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: UEContextReleaseCommand
    NGAP->>CU-CP: on_new_ue_context_release_command()
    Note over CU-CP: Release F1AP context, RRC, and bearers
    CU-CP->>NGAP: UEContextReleaseComplete
    NGAP->>AMF: UEContextReleaseComplete
```

### PDU Session Management

Procedures that establish, modify, and release user-plane PDU sessions.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| PDU Session Resource Setup | `ngap_pdu_session_resource_setup_procedure.cpp` | TS 38.413 §8.2.1 |
| PDU Session Resource Modify | `ngap_pdu_session_resource_modify_procedure.cpp` | TS 38.413 §8.2.3 |
| PDU Session Resource Release | `ngap_pdu_session_resource_release_procedure.cpp` | TS 38.413 §8.2.2 |

#### PDU Session Resource Setup

Establishes PDU session resources for a connected UE (TS 38.413 §8.2.1).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: PDUSessionResourceSetupRequest
    Note over NGAP: Validate security context and PDU sessions
    NGAP->>CU-CP: on_new_pdu_session_resource_setup_request()
    Note over CU-CP: Configure E1AP bearers and F1AP UE context
    CU-CP->>NGAP: PDUSessionResourceSetupResponse
    Note over NGAP: Record PDU session metrics
    NGAP->>AMF: PDUSessionResourceSetupResponse
```

#### PDU Session Resource Modify

Modifies existing PDU session resources at the request of the AMF (TS 38.413 §8.2.3).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: PDUSessionResourceModifyRequest
    Note over NGAP: Validate PDU sessions
    NGAP->>CU-CP: on_new_pdu_session_resource_modify_request()
    Note over CU-CP: Modify E1AP bearers and F1AP UE context
    CU-CP->>NGAP: PDUSessionResourceModifyResponse
    NGAP->>AMF: PDUSessionResourceModifyResponse
```

#### PDU Session Resource Release

Releases PDU session resources following an AMF command (TS 38.413 §8.2.2).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: PDUSessionResourceReleaseCommand
    NGAP->>CU-CP: on_new_pdu_session_resource_release_command()
    Note over CU-CP: Release E1AP bearers and F1AP DRBs
    CU-CP->>NGAP: PDUSessionResourceReleaseResponse
    NGAP->>AMF: PDUSessionResourceReleaseResponse
```

### Handover

Procedures that coordinate inter-gNB handover via the AMF. See also [XnAP](../../xnap/README.md) for Xn-based direct handover without AMF involvement.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| Handover Preparation | `ngap_handover_preparation_procedure.cpp` | TS 38.413 §8.4.1 |
| Handover Resource Allocation | `ngap_handover_resource_allocation_procedure.cpp` | TS 38.413 §8.4.2 |
| Path Switch | `ngap_path_switch_procedure.cpp` | TS 38.413 §8.4.4 |
| DL RAN Status Transfer | `ngap_dl_ran_status_transfer_procedure.cpp` | TS 38.413 §8.4.5 |

#### Handover — End-to-End

Complete NGAP-based inter-gNB handover flow (TS 38.413 §8.4). The source sends a Handover Required to the AMF, which selects the target and forwards the request. After the UE attaches to the target, the target notifies the AMF, which instructs the source to release the UE context.

```mermaid
sequenceDiagram
    participant UE
    participant SourceCUCP as Source CU-CP
    participant AMF
    participant TargetCUCP as Target CU-CP

    Note over SourceCUCP: Measurement report triggers HO decision
    SourceCUCP->>AMF: HandoverRequired
    Note over AMF: Select target gNB
    AMF->>TargetCUCP: HandoverRequest
    Note over TargetCUCP: Allocate UE context and radio resources
    TargetCUCP->>AMF: HandoverRequestAck
    AMF->>SourceCUCP: HandoverCommand
    SourceCUCP->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Detach from source, attach to target
    UE->>TargetCUCP: RRCReconfigurationComplete
    TargetCUCP->>AMF: Handover Notify
    Note over AMF: Downlink path switched to target
    AMF->>SourceCUCP: UEContextReleaseCommand
    Note over SourceCUCP: Release UE context
```

#### Handover Preparation (Source)

Initiates inter-gNB handover from the source side (TS 38.413 §8.4.1). Collects the RRC container from the RRC UE, sends Handover Required to the AMF, and on receipt of the Handover Command forwards the RRC reconfiguration to CU-CP.

```mermaid
sequenceDiagram
    participant RRC as RRC UE
    participant CU-CP
    participant NGAP
    participant AMF

    CU-CP->>NGAP: Trigger Handover Preparation
    NGAP->>RRC: on_handover_preparation_message_required()
    Note over RRC: Prepare RRC container for target gNB
    NGAP->>AMF: HandoverRequired
    Note over AMF: Coordinate with target gNB
    AMF->>NGAP: HandoverCommand
    NGAP->>CU-CP: on_new_rrc_handover_command()
    Note over CU-CP: Deliver HO command to RRC UE
```

#### Handover Resource Allocation (Target)

Allocates resources on the target gNB when the AMF sends a Handover Request (TS 38.413 §8.4.2).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: HandoverRequest
    NGAP->>CU-CP: on_ngap_handover_request()
    Note over CU-CP: Allocate UE context and radio resources
    CU-CP->>NGAP: HandoverRequestAck
    NGAP->>AMF: HandoverRequestAck
    Note over NGAP,CU-CP: Await RRCReconfigurationComplete and DL Status Transfer
    NGAP->>CU-CP: on_n2_handover_execution()
    Note over CU-CP: Finalise handover execution
```

#### Path Switch

Notifies the AMF to switch the downlink path to the new gNB after an intra-5GS handover (TS 38.413 §8.4.4).

```mermaid
sequenceDiagram
    participant CU-CP
    participant NGAP
    participant AMF

    CU-CP->>NGAP: Trigger Path Switch
    NGAP->>AMF: PathSwitchRequest
    Note over AMF: Update downlink path to new gNB
    AMF->>NGAP: PathSwitchRequestAck
    Note over NGAP: Update security context (NCC/NH)
    NGAP->>CU-CP: PathSwitchResponse
```

#### DL RAN Status Transfer

Receives PDCP sequence number state forwarded by the AMF from the source gNB to the target during handover (TS 38.413 §8.4.5).

```mermaid
sequenceDiagram
    participant NGAP
    participant CU-CP
    participant AMF

    AMF->>NGAP: DLRANStatusTransfer
    Note over NGAP: Await DL RAN status from AMF
    NGAP->>CU-CP: on_dl_ran_status_transfer()
    Note over CU-CP: Apply PDCP COUNT values to target bearers
```
