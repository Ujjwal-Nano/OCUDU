# XnAP

The XnAP layer implements the Xn-c control plane interface between two gNBs (TS 38.423). It enables direct inter-gNB handover without involving the AMF and transfers PDCP sequence number state between nodes during handover execution.

Responsibilities:

- Manage the Xn association lifecycle (Xn Setup).
- Run handover preparation procedures on the source and target sides.
- Transfer PDCP SN state from source to target after the UE executes the handover.
- Translate between XnAP ASN.1 message types and the CU-CP's internal types via `xnap_cu_cp_notifier`.

The public contract is expressed through `include/ocudu/xnap/xnap.h`.

---

## Procedures

### Association Management

Procedure that establishes the Xn-c connection between two gNBs.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| Xn Setup | `xn_setup_procedure.cpp` | TS 38.423 §8.10.1 |

#### Xn Setup

Establishes the Xn-c connection between two gNBs (TS 38.423 §8.10.1). Retries with the peer-provided back-off timer on failure.

```mermaid
sequenceDiagram
    box CU-CP
        participant CUCPP as CU-CP
        participant XnAP
    end
    participant PeerGNB as Peer gNB

    XnAP->>PeerGNB: XnSetupRequest
    Note over PeerGNB: Validate gNB configuration
    PeerGNB->>XnAP: XnSetupResponse
    Note over XnAP: Store peer gNB context
    XnAP->>CUCPP: on_xn_setup_complete()
    Note over CUCPP: Register peer gNB for handover
```

### Handover

Procedures that coordinate inter-gNB handover directly between two gNBs, including PDCP state transfer after execution. See also [NGAP](../../ngap/README.md) for AMF-coordinated handover.

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| Handover Preparation (Source) | `xnap_source_handover_preparation_procedure.cpp` | TS 38.423 §8.6.2 |
| Handover Preparation (Target) | `xnap_target_handover_preparation_procedure.cpp` | TS 38.423 §8.6.1 |
| SN Status Transfer | `xnap_sn_status_transfer_procedure.cpp` | TS 38.423 §8.8.1 |

#### Handover — End-to-End

Complete Xn-based inter-gNB handover flow (TS 38.423 §8.6). The source initiates with a Handover Request directly to the target. After the UE connects to the target, the source forwards PDCP state, and the target notifies the AMF via NGAP Path Switch to reroute the downlink.

```mermaid
sequenceDiagram
    participant UE
    participant SourceCUCP as Source CU-CP
    participant TargetCUCP as Target CU-CP
    participant AMF

    Note over SourceCUCP: Measurement report triggers HO decision
    SourceCUCP->>TargetCUCP: HandoverRequest (Xn)
    Note over TargetCUCP: Allocate UE context and radio resources
    TargetCUCP->>SourceCUCP: HandoverRequestAck (Xn)
    SourceCUCP->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Detach from source, attach to target
    UE->>TargetCUCP: RRCReconfigurationComplete
    SourceCUCP->>TargetCUCP: SNStatusTransfer (Xn)
    Note over TargetCUCP: Apply PDCP COUNT values
    TargetCUCP->>AMF: PathSwitchRequest (NGAP)
    Note over AMF: Switch downlink path to target
    AMF->>TargetCUCP: PathSwitchRequestAck
    TargetCUCP->>SourceCUCP: UEContextRelease (Xn)
    Note over SourceCUCP: Release UE context
```

#### Handover Preparation (Source)

Initiates an inter-gNB handover from the source side over Xn (TS 38.423 §8.6.2). Sends the Handover Request directly to the target gNB and awaits the Handover Request Ack. Sends a Handover Cancel on timeout.

```mermaid
sequenceDiagram
    participant UE
    box Source CU-CP
        participant RRC as RRC UE
        participant CUCPP as CU-CP
        participant XnAP
    end
    participant TargetGNB as Target gNB

    Note over CUCPP: Measurement report triggers HO decision
    CUCPP->>XnAP: Trigger Handover Preparation
    XnAP->>RRC: on_handover_preparation_message_required()
    Note over RRC: Prepare RRC container for target gNB
    XnAP->>TargetGNB: HandoverRequest
    Note over TargetGNB: Allocate resources in target cell
    TargetGNB->>XnAP: HandoverRequestAck
    XnAP->>CUCPP: on_new_rrc_handover_command()
    CUCPP->>RRC: Trigger RRCReconfiguration (HO command)
    RRC->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Execute handover to target cell
```

#### Handover Preparation (Target)

Allocates resources on the target side when a peer gNB sends a Handover Request over Xn (TS 38.423 §8.6.1). Notifies the CU-CP to allocate the UE context and bearers, then sends the Handover Request Ack back to the source. Notifies the CU-CP to await the RRC Reconfiguration Complete and SN Status Transfer.

```mermaid
sequenceDiagram
    participant DU
    box Target CU-CP
        participant F1AP as F1AP (CU)
        participant DUP as DU Processor
        participant RRC as RRC UE
        participant CUCPP as CU-CP
        participant XnAP
    end
    participant SourceGNB as Source gNB

    SourceGNB->>XnAP: HandoverRequest
    XnAP->>CUCPP: on_xnap_handover_request()
    Note over CUCPP: Allocate UE context, setup bearers
    CUCPP->>F1AP: UEContextSetupRequest
    F1AP->>DUP: on_ue_rrc_context_creation_request()
    Note over DUP: Create UE context in target cell
    DUP->>RRC: add_ue()
    F1AP->>DU: F1AP UEContextSetupRequest
    Note over DU: Allocate resources in target cell
    DU->>F1AP: F1AP UEContextSetupResponse
    XnAP->>SourceGNB: HandoverRequestAck
    Note over XnAP,CUCPP: Await RRCReconfigurationComplete and SN Status Transfer
    XnAP->>CUCPP: on_xn_handover_execution()
    Note over CUCPP: Finalise handover execution
```

#### SN Status Transfer

Transfers PDCP sequence number state (COUNT values) from the source gNB to the target gNB after the UE executes the handover, ensuring lossless bearer continuity (TS 38.423 §8.8.1).

```mermaid
sequenceDiagram
    box Target CU-CP
        participant CUCPP as CU-CP
        participant XnAP
    end
    participant SourceGNB as Source gNB

    SourceGNB->>XnAP: SNStatusTransfer
    Note over XnAP: Convert ASN.1 to internal SN status format
    XnAP->>CUCPP: on_sn_status_transfer()
    Note over CUCPP: Apply PDCP COUNT values to target bearers
```
