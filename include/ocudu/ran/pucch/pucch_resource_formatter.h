// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/ran/pucch/pucch_configuration.h"
#include "ocudu/ran/pucch/pucch_mapping.h"
#include "ocudu/support/format/delimited_formatter.h"

namespace fmt {

template <>
struct formatter<ocudu::pucch_resource> {
  ocudu::delimited_formatter helper;

  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return helper.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const ocudu::pucch_resource& res, FormatContext& ctx) const
  {
    const unsigned symb_start = res.starting_sym_idx;
    const unsigned symb_stop  = res.starting_sym_idx + res.nof_symbols;

    helper.format_always(ctx, "cell_res_id={}", res.res_id.cell_res_id);

    switch (res.format) {
      case ocudu::pucch_format::FORMAT_0: {
        const auto& f0 = std::get<ocudu::pucch_format_0_cfg>(res.format_params);
        helper.format_always(ctx, "format=0");
        helper.format_always(ctx, "prb1={}", res.starting_prb);
        if (res.second_hop_prb.has_value()) {
          helper.format_always(ctx, "prb2={}", *res.second_hop_prb);
        }
        helper.format_always(ctx, "symb=[{}, {})", symb_start, symb_stop);
        helper.format_always(ctx, "cs={}", f0.initial_cyclic_shift);
        break;
      }
      case ocudu::pucch_format::FORMAT_1: {
        const auto& f1 = std::get<ocudu::pucch_format_1_cfg>(res.format_params);
        helper.format_always(ctx, "format=1");
        helper.format_always(ctx, "prb1={}", res.starting_prb);
        if (res.second_hop_prb.has_value()) {
          helper.format_always(ctx, "prb2={}", *res.second_hop_prb);
        }
        helper.format_always(ctx, "symb=[{}, {})", symb_start, symb_stop);
        helper.format_always(ctx, "cs={}", f1.initial_cyclic_shift);
        helper.format_always(ctx, "occ={}", f1.time_domain_occ);
        break;
      }
      case ocudu::pucch_format::FORMAT_2: {
        const auto& f2 = std::get<ocudu::pucch_format_2_3_cfg>(res.format_params);
        helper.format_always(ctx, "format=2");
        helper.format_always(ctx, "prb=[{}, {})", res.starting_prb, res.starting_prb + f2.nof_prbs);
        if (res.second_hop_prb.has_value()) {
          helper.format_always(ctx, "prb2={}", *res.second_hop_prb);
        }
        helper.format_always(ctx, "symb=[{}, {})", symb_start, symb_stop);
        break;
      }
      case ocudu::pucch_format::FORMAT_3: {
        const auto& f3 = std::get<ocudu::pucch_format_2_3_cfg>(res.format_params);
        helper.format_always(ctx, "format=3");
        helper.format_always(ctx, "prb=[{}, {})", res.starting_prb, res.starting_prb + f3.nof_prbs);
        if (res.second_hop_prb.has_value()) {
          helper.format_always(ctx, "prb2={}", *res.second_hop_prb);
        }
        helper.format_always(ctx, "symb=[{}, {})", symb_start, symb_stop);
        break;
      }
      case ocudu::pucch_format::FORMAT_4: {
        const auto& f4 = std::get<ocudu::pucch_format_4_cfg>(res.format_params);
        helper.format_always(ctx, "format=4");
        helper.format_always(ctx, "prb=[{}, {})", res.starting_prb, res.starting_prb + 1);
        if (res.second_hop_prb.has_value()) {
          helper.format_always(ctx, "prb2={}", *res.second_hop_prb);
        }
        helper.format_always(ctx, "symb=[{}, {})", symb_start, symb_stop);
        helper.format_always(ctx, "occ={}/{}", fmt::underlying(f4.occ_index), fmt::underlying(f4.occ_length));
        break;
      }
      default:
        helper.format_always(ctx, "format=invalid");
        break;
    }

    if (res.rep_factor != ocudu::pucch_repetition_factor::n1) {
      helper.format_always(ctx, "rep={}", static_cast<unsigned>(res.rep_factor));
    }

    return ctx.out();
  }
};

} // namespace fmt
