# CU-UP Processor

The CU-UP Processor owns and operates the E1AP protocol stack for a single connected CU-UP. One `cu_up_processor_impl` instance is created per CU-UP when the GNB-CU-UP E1 Setup handshake completes. It manages the E1 association lifecycle and relays bearer context operations between the CU-CP and the CU-UP.

Responsibilities:

- Own and drive the E1AP CU-CP instance for the CU-UP.
- Handle the GNB-CU-UP E1 Setup request and maintain the CU-UP's capabilities and configuration.
- Forward bearer context setup, modification, and release requests from the CU-CP to the E1AP layer.
- Handle E1 Reset and E1 Release procedures, cleaning up affected UE contexts.

The public contract is expressed through `include/ocudu/e1ap/cu_cp/e1ap_cu_cp.h` and the `cu_up_processor` interface. For procedure diagrams see [E1AP (CU-CP)](../../e1ap/cu_cp/README.md).
