# UE Security Manager

The UE Security Manager holds the per-UE 3GPP NR security context and is the single source of truth for all cryptographic material in the CU-CP. It receives the initial security context from the AMF via NGAP, selects AS ciphering and integrity-protection algorithms according to the configured preference list, and derives the RRC and UP access-stratum (AS) keys used by the RRC and E1AP layers. Key derivation follows TS 33.501 (5G Security Architecture and Procedures).

Responsibilities:

- Store and manage the UE's `security_context` (K_gNB, NCC, supported algorithms).
- Select the AS ciphering and integrity algorithms from the CU-CP preference list and AMF capabilities.
- Derive RRC and UP 128-bit AS keys (`sec_128_as_config`) for use by the RRC layer (ciphering, integrity) and CU-UP (PDCP).
- Support horizontal key derivation (K_gNB* from PCI and ARFCN) for handover and reestablishment.
- Provide the current security context and NCC to NGAP for Path Switch and Handover responses.

The public contract is expressed through three role-based interfaces in `include/ocudu/cu_cp/cu_cp_types.h`:
- `rrc_ue_security_manager` — used by the RRC UE layer
- `ngap_ue_security_manager` — used by the NGAP layer
- `up_ue_security_manager` — used by the E1AP / CU-UP layer

---

## Key Derivation Flow

Triggered by the Initial Context Setup procedure (TS 38.413 §8.3.1). The AS keys are derived from K_gNB as specified in TS 33.501 Annex A.4; algorithm selection follows TS 38.331 §5.3.4.

```mermaid
sequenceDiagram
    participant NGAP
    participant SecMgr as UE Security Manager
    participant RRC as RRC UE
    participant E1AP as E1AP (CU-CP)
    participant AMF

    AMF->>NGAP: InitialContextSetupRequest [security_context]
    NGAP->>SecMgr: init_security_context()
    Note over SecMgr: Store K_gNB, NCC, UE algorithm capabilities
    Note over SecMgr: Select AS algorithms (cipher + integrity)
    Note over SecMgr: Derive RRC 128-bit AS keys
    Note over SecMgr: Derive UP 128-bit AS keys

    RRC->>SecMgr: get_rrc_128_as_config()
    Note over SecMgr: Return RRC AS config (alg + keys)
    RRC->>RRC: Activate SRB ciphering and integrity

    E1AP->>SecMgr: get_up_128_as_config()
    Note over SecMgr: Return UP AS config (alg + keys)
    E1AP->>E1AP: Configure PDCP security on CU-UP
```

## Horizontal Key Derivation (Handover / Reestablishment)

During handover or reestablishment, new AS keys are derived from the current K_gNB using the target cell's PCI and SSB ARFCN (TS 33.501 Annex A.11).

```mermaid
sequenceDiagram
    participant RRC as RRC UE
    participant SecMgr as UE Security Manager

    RRC->>SecMgr: perform_horizontal_key_derivation(pci, arfcn)
    Note over SecMgr: Derive K_gNB* from PCI and SSB ARFCN
    Note over SecMgr: Update stored K_gNB and increment NCC
    RRC->>SecMgr: get_rrc_128_as_config()
    Note over SecMgr: Return refreshed AS keys
```
