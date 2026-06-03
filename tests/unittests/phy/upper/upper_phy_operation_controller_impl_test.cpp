// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "../../../../lib/phy/upper/upper_phy_operation_controller_impl.h"
#include <gtest/gtest.h>
#include <vector>

using namespace ocudu;

namespace {

TEST(UpperPhyOperationControllerImplTest, StartWithoutTargetIsNoOp)
{
  upper_phy_operation_controller_impl controller;
  controller.start();
}

TEST(UpperPhyOperationControllerImplTest, StopWithoutTargetIsNoOp)
{
  upper_phy_operation_controller_impl controller;
  controller.stop();
}

TEST(UpperPhyOperationControllerImplTest, StartInvokesTargetWithTrue)
{
  upper_phy_operation_controller_impl controller;
  bool                                last_value = false;
  bool                                called     = false;
  controller.set_active_target([&](bool a) {
    last_value = a;
    called     = true;
  });

  controller.start();

  EXPECT_TRUE(called);
  EXPECT_TRUE(last_value);
}

TEST(UpperPhyOperationControllerImplTest, StopInvokesTargetWithFalse)
{
  upper_phy_operation_controller_impl controller;
  bool                                last_value = true;
  bool                                called     = false;
  controller.set_active_target([&](bool a) {
    last_value = a;
    called     = true;
  });

  controller.stop();

  EXPECT_TRUE(called);
  EXPECT_FALSE(last_value);
}

TEST(UpperPhyOperationControllerImplTest, StartStopSequencePreservesOrdering)
{
  upper_phy_operation_controller_impl controller;
  std::vector<bool>                   trace;
  controller.set_active_target([&](bool a) { trace.push_back(a); });

  controller.start();
  controller.stop();
  controller.start();
  controller.start();
  controller.stop();

  ASSERT_EQ(trace.size(), 5u);
  EXPECT_TRUE(trace[0]);
  EXPECT_FALSE(trace[1]);
  EXPECT_TRUE(trace[2]);
  EXPECT_TRUE(trace[3]);
  EXPECT_FALSE(trace[4]);
}

TEST(UpperPhyOperationControllerImplTest, RewiringTargetReplacesPrevious)
{
  upper_phy_operation_controller_impl controller;
  int                                 first_count  = 0;
  int                                 second_count = 0;
  controller.set_active_target([&](bool) { ++first_count; });
  controller.start();
  EXPECT_EQ(first_count, 1);

  controller.set_active_target([&](bool) { ++second_count; });
  controller.start();
  EXPECT_EQ(first_count, 1);
  EXPECT_EQ(second_count, 1);
}

} // namespace
