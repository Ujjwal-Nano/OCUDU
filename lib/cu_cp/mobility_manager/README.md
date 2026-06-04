# Mobility Manager

The Mobility Manager orchestrates all UE mobility decisions and handover procedures within the CU-CP. It is notified by the Cell Measurement Manager when a neighbour cell becomes better and decides whether and how to trigger a handover.

Responsibilities:

- Evaluate measurement reports and select the handover target cell.
- Trigger intra-gNB (inter-DU) handovers directly via the DU Processor.
- Trigger inter-gNB handovers via the NGAP or XnAP layer.
- Trigger and manage Conditional Handover (CHO) preparation and execution.
- Report handover metrics via `mobility_manager_metrics_handler`.

The Mobility Manager interfaces with:
- `cell_meas_manager` — to receive neighbour-better events.
- `ngap_repository` — to reach the NGAP layer for inter-gNB (N2) handovers.
- `xnap_repository` — to reach the XnAP layer for inter-gNB (Xn) handovers.
- `du_processor_repository` — to reach DU Processors for intra-gNB handovers.
- `ue_manager` — to look up UE contexts.

---

## Procedures

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| Intra-gNB (Inter-DU) Handover | `intra_cu_handover_routine.cpp` | TS 38.300 §9.2.3.2 |
| Inter-gNB Handover (NGAP) | `inter_cu_handover_source_routine.cpp` | TS 38.413 §8.4 |
| Inter-gNB Handover (XnAP) | `xnap_source_handover_preparation_procedure.cpp` | TS 38.423 §8.6 |
| Conditional Handover — Preparation | `conditional_handover_source_routine.cpp` | TS 38.331 §5.3.5.3 |
| Conditional Handover — Execution | `conditional_handover_target_routine.cpp` | TS 38.331 §5.3.5.3 |

### Intra-gNB (Inter-DU) Handover

Moves a UE between two DUs connected to the same CU-CP. The Mobility Manager drives an F1AP UE Context Setup on the target DU, then an F1AP UE Context Release on the source DU. No NGAP or XnAP signalling is required.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant MobMgr as Mobility Manager
    participant DUP_TGT as DU Processor (Target)
    participant DUP_SRC as DU Processor (Source)

    Note over MobMgr: Measurement report triggers HO decision
    MobMgr->>DUP_TGT: UEContextSetupRequest (target cell)
    Note over DUP_TGT: Allocate UE context in target DU
    DUP_TGT->>MobMgr: UEContextSetupResponse
    MobMgr->>RRC: Trigger RRCReconfiguration (target cell config)
    RRC->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Execute handover to target cell
    UE->>RRC: RRCReconfigurationComplete
    MobMgr->>DUP_SRC: UEContextReleaseCommand (source cell)
    Note over DUP_SRC: Release source cell resources
```

### Inter-gNB Handover (NGAP)

Hands over a UE to a neighbouring gNB via the AMF. The Mobility Manager initiates the NGAP Handover Preparation procedure; the NGAP layer handles the AMF signalling and delivers the RRC Handover Command.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant MobMgr as Mobility Manager
    participant NGAP

    Note over MobMgr: Measurement report triggers inter-gNB HO decision
    MobMgr->>NGAP: Trigger Handover Preparation
    Note over NGAP: Run NGAP HO preparation procedure (via AMF)
    NGAP->>RRC: RRCReconfiguration (HO command)
    RRC->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Execute handover to target gNB
```

### Inter-gNB Handover (XnAP)

Hands over a UE to a neighbouring gNB directly over the Xn interface, bypassing the AMF. The Mobility Manager initiates the XnAP Handover Preparation procedure.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant MobMgr as Mobility Manager
    participant XnAP

    Note over MobMgr: Measurement report triggers inter-gNB HO decision
    MobMgr->>XnAP: Trigger Handover Preparation
    Note over XnAP: Run XnAP HO preparation procedure (direct to target gNB)
    XnAP->>RRC: RRCReconfiguration (HO command)
    RRC->>UE: RRCReconfiguration (DL-DCCH)
    Note over UE: Execute handover to target gNB
```

### Conditional Handover (CHO) Preparation

Prepares candidate target cells for a Conditional Handover. The Mobility Manager sends an F1AP UE Context Setup to each candidate DU in advance; the UE autonomously executes the handover when the trigger condition is met.

```mermaid
sequenceDiagram
    participant UE
    participant RRC as RRC UE
    participant MobMgr as Mobility Manager
    participant DUP as DU Processor (Candidate)

    Note over MobMgr: CHO policy triggers candidate preparation
    MobMgr->>DUP: UEContextSetupRequest (CHO candidate)
    Note over DUP: Allocate UE context with CHO trigger config
    DUP->>MobMgr: UEContextSetupResponse
    MobMgr->>RRC: Trigger RRCReconfiguration (CHO config)
    RRC->>UE: RRCReconfiguration (DL-DCCH, candidate cells)
    Note over UE: Monitor trigger conditions autonomously
```
