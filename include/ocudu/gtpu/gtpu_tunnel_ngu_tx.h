// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/adt/byte_buffer.h"
#include "ocudu/ran/cu_types.h"

struct sockaddr_storage;

/*
 * This file will hold the interfaces and notifiers for the GTP-U tunnel.
 * They follow the following nomenclature:
 *
 *   gtpu_tunnel_{domain}_{tx/rx}_{lower/upper}_{data/control}_{interface/notifier}
 *
 * 1. Domain indicates the GTP-U 3gpp interface the entity is being used for,
 *    e.g., NG-U or F1-U.
 * 2. TX/RX indicates whether the interface is intended for the
 *    TX or RX side of the tunnel. TX/RX terminology is used from the
 *    perspective of the GTP-U, i.e. are we receiving or sending packets to
 *    the socket gateway.
 * 3. Lower/Upper indicates whether the interface/notifier interacts
 *    with the upper or lower layers.
 * 4. Data/Control indicates whether this interface should be used for passing data
 *    or perform control actions.
 * 5. Interface/Notifier: whether this is an interface the GTP-U tunnel will
 *    inherit or a notifier that the GTP-U will keep as a member.
 *
 */

namespace ocudu {

/****************************************
 * Interfaces/notifiers for the gateway
 ****************************************/
/// This interface represents the data entry point of the transmitting side of a GTP-U entity.
/// The lower layer will use this call to pass GTP-U SDUs (i.e. NG-U PDUs) into the TX entity.
class gtpu_tunnel_ngu_tx_lower_data_interface
{
public:
  gtpu_tunnel_ngu_tx_lower_data_interface()                                                          = default;
  virtual ~gtpu_tunnel_ngu_tx_lower_data_interface()                                                 = default;
  gtpu_tunnel_ngu_tx_lower_data_interface(const gtpu_tunnel_ngu_tx_lower_data_interface&)            = delete;
  gtpu_tunnel_ngu_tx_lower_data_interface& operator=(const gtpu_tunnel_ngu_tx_lower_data_interface&) = delete;
  gtpu_tunnel_ngu_tx_lower_data_interface(gtpu_tunnel_ngu_tx_lower_data_interface&&)                 = delete;
  gtpu_tunnel_ngu_tx_lower_data_interface& operator=(gtpu_tunnel_ngu_tx_lower_data_interface&&)      = delete;

  /// \brief Interface for the lower layer to pass a SDU (i.e. NG-U PDU) into the GTP-U.
  /// \param sdu SDU to be handled.
  /// \param qfi QoS flow ID that is associated with the SDU.
  virtual void handle_sdu(byte_buffer sdu, qos_flow_id_t qfi) = 0;
};

/// This interface represents the control service access point of the transmitting side of a GTP-U entity.
/// The upper layer will use this to control GTP-U TX entity.
class gtpu_tunnel_ngu_tx_upper_control_interface
{
public:
  gtpu_tunnel_ngu_tx_upper_control_interface()                                                             = default;
  virtual ~gtpu_tunnel_ngu_tx_upper_control_interface()                                                    = default;
  gtpu_tunnel_ngu_tx_upper_control_interface(const gtpu_tunnel_ngu_tx_upper_control_interface&)            = delete;
  gtpu_tunnel_ngu_tx_upper_control_interface& operator=(const gtpu_tunnel_ngu_tx_upper_control_interface&) = delete;
  gtpu_tunnel_ngu_tx_upper_control_interface(gtpu_tunnel_ngu_tx_upper_control_interface&&)                 = delete;
  gtpu_tunnel_ngu_tx_upper_control_interface& operator=(gtpu_tunnel_ngu_tx_upper_control_interface&&)      = delete;

  /// \brief Update the remote UPF endpoint (N3 UL tunnel), e.g. after an Xn path switch.
  /// \param new_addr New peer IPv4/IPv6 address string.
  /// \param new_port New peer UDP port.
  /// \param new_teid New peer GTP-U TEID.
  virtual void update_tx_endpoint(const std::string& new_addr, uint16_t new_port, uint32_t new_teid) = 0;
};

} // namespace ocudu
