// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

namespace ocudu {

struct o_du_high_e2_config;

/// Auto derive O-RAN DU high E2 parameters after the parsing.
void autoderive_o_du_high_e2_parameters_after_parsing(o_du_high_e2_config& unit_cfg);

} // namespace ocudu
