// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "ocudu/adt/detail/bitset_base.h"
#include "fmt/format.h"

namespace ocudu {

/// \brief Bitset with a fixed compile-time size of N bits.
///
/// Unlike bounded_bitset, this class does not carry a dynamic size member: the size is always N. This saves space
/// and avoids runtime checks for size-changing operations.
///
/// \tparam N                  Number of bits.
/// \tparam LowestInfoBitIsMSB If true, bit index 0 maps to the MSB of the underlying word.
template <size_t N, bool LowestInfoBitIsMSB = false>
class fixed_size_bitset : public detail::bitset_base<fixed_size_bitset<N, LowestInfoBitIsMSB>, N, LowestInfoBitIsMSB>
{
  using base_t = detail::bitset_base<fixed_size_bitset<N, LowestInfoBitIsMSB>, N, LowestInfoBitIsMSB>;
  friend base_t;
  friend struct fmt::formatter<fixed_size_bitset<N, LowestInfoBitIsMSB>>;

public:
  constexpr fixed_size_bitset() = default;

  /// \brief Constructs a bitset using iterators.
  ///
  /// The iterator range must contain exactly N elements. The values are mapped one-to-one starting from begin.
  ///
  /// \tparam Iterator Boolean iterator type.
  /// \param[in] begin Begin iterator.
  /// \param[in] end End iterator.
  template <typename Iterator,
            std::enable_if_t<std::is_convertible_v<typename std::iterator_traits<Iterator>::value_type, bool>, int> = 0>
  constexpr fixed_size_bitset(Iterator begin, Iterator end)
  {
    report_fatal_error_if_not(static_cast<size_t>(end - begin) == N,
                              "fixed_size_bitset iterator constructor requires exactly N={} elements",
                              N);
    for (size_t count = 0; count != N; ++count, ++begin) {
      this->set_(count, *begin);
    }
  }

  /// \brief Constructs a bitset from an initializer list.
  ///
  /// The initializer list must contain exactly N elements.
  ///
  /// \param[in] values Boolean initializer list.
  constexpr fixed_size_bitset(const std::initializer_list<const bool>& values)
  {
    report_fatal_error_if_not(
        values.size() == N, "fixed_size_bitset initializer list requires exactly N={} elements", N);
    auto it = values.begin();
    for (size_t count = 0; count != N; ++count, ++it) {
      this->set_(count, *it);
    }
  }

  static constexpr size_t size() noexcept { return N; }
};

/// \brief Bitwise AND operation result = lhs & rhs.
template <size_t N, bool LowestInfoBitIsMSB>
inline fixed_size_bitset<N, LowestInfoBitIsMSB> operator&(const fixed_size_bitset<N, LowestInfoBitIsMSB>& lhs,
                                                          const fixed_size_bitset<N, LowestInfoBitIsMSB>& rhs) noexcept
{
  fixed_size_bitset<N, LowestInfoBitIsMSB> res(lhs);
  res &= rhs;
  return res;
}

/// \brief Bitwise OR operation result = lhs | rhs.
template <size_t N, bool LowestInfoBitIsMSB>
inline fixed_size_bitset<N, LowestInfoBitIsMSB> operator|(const fixed_size_bitset<N, LowestInfoBitIsMSB>& lhs,
                                                          const fixed_size_bitset<N, LowestInfoBitIsMSB>& rhs) noexcept
{
  fixed_size_bitset<N, LowestInfoBitIsMSB> res(lhs);
  res |= rhs;
  return res;
}

} // namespace ocudu

namespace fmt {

/// \brief Custom formatter for fixed_size_bitset<N, LowestInfoBitIsMSB>.
template <size_t N, bool LowestInfoBitIsMSB>
struct formatter<ocudu::fixed_size_bitset<N, LowestInfoBitIsMSB>> {
  enum { hexadecimal, binary, bit_positions } mode = binary;
  enum { forward, reverse } order                  = forward;

  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    auto it = ctx.begin();
    while (it != ctx.end() and *it != '}') {
      if (*it == 'x') {
        mode = hexadecimal;
      }
      if (*it == 'r') {
        order = reverse;
      }
      if (*it == 'n') {
        mode = bit_positions;
      }
      ++it;
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const ocudu::fixed_size_bitset<N, LowestInfoBitIsMSB>& s, FormatContext& ctx) const
  {
    if (mode == hexadecimal) {
      return s.template to_string_of_hex<decltype(std::declval<FormatContext>().out())>(ctx.out(), order == reverse);
    }

    if (mode == bit_positions) {
      if (s.count() == 0) {
        fmt::format_to(ctx.out(), "none");
      } else if (s.is_contiguous()) {
        unsigned lowest  = s.find_lowest();
        unsigned highest = s.find_highest();
        if (lowest == highest) {
          fmt::format_to(ctx.out(), "{}", lowest);
        } else {
          fmt::format_to(ctx.out(), "[{}, {})", lowest, highest + 1);
        }
      } else {
        ocudu::static_vector<size_t, N> bit_pos = s.get_bit_positions();
        fmt::format_to(ctx.out(), "{}", ocudu::span<size_t>(bit_pos));
      }
      return ctx.out();
    }

    return s.template to_string_of_bits<decltype(std::declval<FormatContext>().out())>(ctx.out(), order == reverse);
  }
};

} // namespace fmt
