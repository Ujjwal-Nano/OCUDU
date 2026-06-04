# Metrics Handler

The Metrics Handler aggregates performance counters from the CU-CP's sub-components and delivers periodic reports to registered consumers. It drives reporting sessions via a configurable timer and collects data from the UE Manager, DU repository, NGAP repository, and Mobility Manager on each tick.

Responsibilities:

- Open and close metrics reporting sessions via `metrics_report_session`.
- Collect UE counts, bearer statistics, DU connection states, NGAP peer states, and handover event counters on each timer tick.
- Deliver aggregated `cu_cp_metrics` reports to registered `cu_cp_metrics_report_notifier` consumers.

The public contract is expressed through `include/ocudu/cu_cp/cu_cp_metrics_handler.h`.

---

## Reporting Flow

```mermaid
sequenceDiagram
    participant UEM as UE Manager
    participant DUR as DU Repository
    participant NGAPR as NGAP Repository
    participant MobMgr as Mobility Manager
    participant Metrics as Metrics Handler
    participant Consumer

    Note over Metrics: Timer tick
    Metrics->>UEM: get_metrics()
    Note over UEM: Return UE counts and session stats
    Metrics->>DUR: get_metrics()
    Note over DUR: Return DU connection states
    Metrics->>NGAPR: get_metrics()
    Note over NGAPR: Return NGAP peer states
    Metrics->>MobMgr: get_metrics()
    Note over MobMgr: Return handover event counters
    Note over Metrics: Aggregate into cu_cp_metrics
    Metrics->>Consumer: on_metrics_report()
```
