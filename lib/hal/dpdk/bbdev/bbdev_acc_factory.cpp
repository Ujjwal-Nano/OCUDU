// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#include "ocudu/hal/dpdk/bbdev/bbdev_acc_factory.h"

using namespace ocudu;
using namespace dpdk;

std::shared_ptr<bbdev_acc> ocudu::dpdk::create_bbdev_acc(const bbdev_acc_configuration& cfg,
                                                         ocudulog::basic_logger&        logger)
{
  // TODO implement bbdev accelerator creation for supported hardware acceleration.
  logger.error("[bbdev] bbdev accelerator creation failed. Cause: hardware-acceleration is not supported.");

  return nullptr;
}
