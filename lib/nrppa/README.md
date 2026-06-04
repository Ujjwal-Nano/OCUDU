# NRPPa

The NRPPa layer implements the NR Positioning Protocol A interface between the CU-CP and the Location Management Function (LMF) (TS 38.455). NRPPa messages are transported by NGAP (via UE-associated and non-UE-associated transport), but are handled independently of NGAP by this layer.

Responsibilities:

- Dispatch incoming NRPPa PDUs from the LMF to the appropriate procedure.
- Coordinate with the F1AP layer to request DU-side positioning measurements and configuration.
- Aggregate measurement results from multiple DUs and return them to the LMF.
- Maintain per-UE and per-DU measurement contexts for periodic measurements.

The NRPPa layer interfaces with:
- `nrppa_cu_cp_notifier` — to request TRP information or aggregate results from all DUs via CU-CP.
- `du_ctxt_list[du_index].f1ap` — to send F1AP positioning requests to individual DUs.

The public contract is expressed through `include/ocudu/nrppa/nrppa.h`.

---

## Procedures

| Procedure | File | 3GPP reference |
|-----------|------|----------------|
| E-CID Measurement Initiation | `e_cid_measurement_initiation_procedure.cpp` | TS 38.455 §8.8.1 |
| E-CID Measurement Termination | `e_cid_measurement_termination_procedure.cpp` | TS 38.455 §8.8.2 |
| Measurement | `measurement_procedure.cpp` | TS 38.455 §8.1.1 |
| Positioning Information Exchange | `positioning_information_exchange_procedure.cpp` | TS 38.455 §8.10.1 |
| Positioning Activation | `positioning_activation_procedure.cpp` | TS 38.455 §8.9.1 |
| TRP Information Exchange | `trp_information_exchange_procedure.cpp` | TS 38.455 §8.7.1 |

### End-to-End Positioning Flow

A complete positioning session requires four NRPPa exchanges in sequence. TRP information and positioning information are typically fetched once per session; activation and measurements repeat as needed.

```mermaid
sequenceDiagram
    participant UE
    participant DU
    box CU-CP
        participant F1AP as F1AP (CU)
        participant NRPPa
    end
    participant LMF

    Note over LMF: Step 1 — Retrieve TRP geometry (once per session)
    LMF->>NRPPa: TRP Information Request
    NRPPa->>F1AP: TRP Information Request
    F1AP->>DU: F1AP TRPInformationRequest
    Note over DU: Return TRP coordinates and capabilities
    DU->>F1AP: F1AP TRPInformationResponse
    F1AP->>NRPPa: TRP Information Response
    NRPPa->>LMF: TRP Information Response

    Note over LMF: Step 2 — Retrieve UE SRS configuration
    LMF->>NRPPa: Positioning Information Request
    NRPPa->>F1AP: Positioning Information Request
    F1AP->>DU: F1AP PositioningInformationRequest
    Note over DU: Collect UE SRS config
    DU->>F1AP: F1AP PositioningInformationResponse
    F1AP->>NRPPa: Positioning Information Response
    NRPPa->>LMF: Positioning Information Response

    Note over LMF: Step 3 — Activate SRS transmission at DU
    LMF->>NRPPa: Positioning Activation Request
    NRPPa->>F1AP: Positioning Activation Request
    F1AP->>DU: F1AP PositioningActivationRequest
    Note over DU: Activate SRS transmission
    DU->>F1AP: F1AP PositioningActivationResponse
    F1AP->>NRPPa: Positioning Activation Response
    NRPPa->>LMF: Positioning Activation Response
    Note over UE: Transmitting SRS

    Note over LMF: Step 4 — Collect measurements from all TRPs
    LMF->>NRPPa: Measurement Request
    loop For each DU / TRP
        NRPPa->>F1AP: Positioning Measurement Request
        F1AP->>DU: F1AP PositioningMeasurementRequest
        Note over DU: Measure UE SRS (RSTD / RSRP)
        DU->>F1AP: F1AP PositioningMeasurementResponse
        F1AP->>NRPPa: Positioning Measurement Response
    end
    Note over NRPPa: Aggregate results from all DUs
    NRPPa->>LMF: Measurement Response
    Note over LMF: Compute UE position
```

### E-CID Measurement Initiation

Requests Enhanced Cell ID (E-CID) measurements for a specific UE (TS 38.455 §8.8.1). Supports on-demand (single-shot) and periodic measurement modes. For on-demand measurements the result is returned immediately; for periodic measurements a timer is configured and results are reported at each interval.

```mermaid
sequenceDiagram
    participant UE
    participant DU
    box CU-CP
        participant F1AP as F1AP (CU)
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: E-CID Measurement Initiation Request
    Note over NRPPa: Identify target UE and DU
    NRPPa->>F1AP: Positioning Information Request
    F1AP->>DU: F1AP PositioningInformationRequest
    Note over DU: Collect UE cell-ID measurements
    DU->>F1AP: F1AP PositioningInformationResponse
    F1AP->>NRPPa: Positioning Information Response
    NRPPa->>LMF: E-CID Measurement Initiation Response
    Note over LMF: Compute UE location estimate
```

### E-CID Measurement Termination

Stops an ongoing periodic E-CID measurement session for a UE (TS 38.455 §8.8.2).

```mermaid
sequenceDiagram
    box CU-CP
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: E-CID Measurement Termination Command
    Note over NRPPa: Cancel periodic measurement timer
    NRPPa->>LMF: E-CID Measurement Termination Response
```

### Measurement

Coordinates positioning measurements across multiple DUs for a given measurement request (TS 38.455 §8.1.1). The NRPPa layer fans out F1AP Positioning Measurement Requests to each relevant DU, aggregates the results, and returns them to the LMF.

```mermaid
sequenceDiagram
    participant DU as DU (one per TRP)
    box CU-CP
        participant F1AP as F1AP (CU)
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: Measurement Request
    Note over NRPPa: Create measurement context, allocate RAN Meas ID
    loop For each DU / TRP
        NRPPa->>F1AP: Positioning Measurement Request
        F1AP->>DU: F1AP PositioningMeasurementRequest
        Note over DU: Perform RSTD / RSRP measurements
        DU->>F1AP: F1AP PositioningMeasurementResponse
        F1AP->>NRPPa: Positioning Measurement Response
    end
    Note over NRPPa: Aggregate results from all DUs
    NRPPa->>LMF: Measurement Response
    Note over LMF: Compute UE position
```

### Positioning Information Exchange

Retrieves SRS configuration and other positioning-related information for a UE from its serving DU (TS 38.455 §8.10.1).

```mermaid
sequenceDiagram
    participant UE
    participant DU
    box CU-CP
        participant F1AP as F1AP (CU)
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: Positioning Information Request
    NRPPa->>F1AP: Positioning Information Request
    F1AP->>DU: F1AP PositioningInformationRequest
    Note over DU: Collect UE SRS and positioning config
    DU->>F1AP: F1AP PositioningInformationResponse
    F1AP->>NRPPa: Positioning Information Response
    NRPPa->>LMF: Positioning Information Response
```

### Positioning Activation

Activates semi-persistent or aperiodic SRS transmission at the DU for a UE, enabling uplink reference signal measurements (TS 38.455 §8.9.1).

```mermaid
sequenceDiagram
    participant UE
    participant DU
    box CU-CP
        participant F1AP as F1AP (CU)
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: Positioning Activation Request
    NRPPa->>F1AP: Positioning Activation Request
    F1AP->>DU: F1AP PositioningActivationRequest
    Note over DU: Configure and activate SRS transmission
    DU->>F1AP: F1AP PositioningActivationResponse
    F1AP->>NRPPa: Positioning Activation Response
    NRPPa->>LMF: Positioning Activation Response
    Note over UE: Begins transmitting SRS
```

### TRP Information Exchange

Retrieves Transmission/Reception Point (TRP) information from connected DUs (TS 38.455 §8.7.1). If the requested TRP IDs are unknown or the list is empty, the NRPPa layer requests the information from the CU-CP, which aggregates it across all DUs.

```mermaid
sequenceDiagram
    participant DU
    box CU-CP
        participant F1AP as F1AP (CU)
        participant CUCPP as CU-CP
        participant NRPPa
    end
    participant LMF

    LMF->>NRPPa: TRP Information Request
    alt TRP IDs known
        NRPPa->>F1AP: TRP Information Request
        F1AP->>DU: F1AP TRPInformationRequest
        Note over DU: Return TRP geometry and capabilities
        DU->>F1AP: F1AP TRPInformationResponse
        F1AP->>NRPPa: TRP Information Response
    else TRP IDs unknown or empty
        NRPPa->>CUCPP: on_trp_information_request()
        Note over CUCPP: Aggregate TRP info from all DUs via F1AP
        CUCPP->>NRPPa: TRP Information
    end
    NRPPa->>LMF: TRP Information Response
```
