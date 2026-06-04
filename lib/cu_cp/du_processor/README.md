# DU Processor

The DU Processor is the CU-CP component responsible for managing a single connected DU. One `du_processor_impl` instance is created per DU when the F1 Setup completes. It owns the F1AP entity and the RRC handler for that DU and acts as the coordination point between them.

Responsibilities:

- Own and drive the F1AP CU instance for the DU.
- Create and remove RRC UE instances when UEs attach or detach from the DU.
- Wire up the per-UE adapters that connect F1AP SRB bearers to the RRC UE layer.
- Handle UE RRC context transfer requests during handover and reestablishment.
- Forward DU configuration update requests to the F1AP layer (`gnb_cu_configuration_update`).

The public contract is expressed through `include/ocudu/cu_cp/cu_cp_types.h` and the `du_processor` interface. Inbound events from F1AP arrive via `f1ap_du_processor_notifier`; outbound events toward the CU-CP go via `du_processor_notifier`.

---

## Procedures

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| UE RRC Context Creation | *(inline in `du_processor_impl.cpp`)* | TS 38.473 §8.3.1 |
| Reestablishment Context Modification | `reestablishment_context_modification_routine.cpp` | TS 38.331 §5.3.7 |

### UE RRC Context Creation

Creates a new RRC UE instance and connects its SRB adapters to the F1AP bearer manager. Called by the F1AP layer when it receives the first UL-CCCH message for a UE.

```mermaid
sequenceDiagram
    participant F1AP as F1AP (CU)
    participant DUP as DU Processor
    participant RRC as RRC UE

    F1AP->>DUP: on_ue_rrc_context_creation_request()
    Note over DUP: Allocate UE in UE Manager
    DUP->>RRC: add_ue()
    Note over RRC: Initialise RRC context and SRB0
    DUP->>F1AP: SRB0/SRB1/SRB2 notifiers
    Note over DUP: Connect F1AP SRB adapters to RRC UE
```

### Reestablishment Context Modification

Triggered by the RRC Reestablishment procedure after `RRCReestablishmentComplete` is received. The DU Processor issues an F1AP UE Context Modification to re-anchor the UE on the correct DU cell with its recovered bearer configuration.

```mermaid
sequenceDiagram
    participant DU
    participant F1AP as F1AP (CU)
    participant DUP as DU Processor
    participant RRC as RRC UE

    RRC->>DUP: on_rrc_reestablishment_context_modification_required()
    Note over DUP: Build UE context modification with recovered bearers
    DUP->>F1AP: UEContextModificationRequest
    F1AP->>DU: F1AP UEContextModificationRequest
    Note over DU: Update UE radio context in target cell
    DU->>F1AP: F1AP UEContextModificationResponse
    F1AP->>DUP: UEContextModificationResponse
    DUP->>RRC: context_modification_success
```
