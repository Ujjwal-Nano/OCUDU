# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI
# Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

set(SOURCES
        config_builder.cpp
        yaml_writer.cpp)

add_library(ocudu_config_app_helpers STATIC ${SOURCES})
target_include_directories(ocudu_config_app_helpers PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ocudu_config_app_helpers PUBLIC ocudu::yaml-cpp)
