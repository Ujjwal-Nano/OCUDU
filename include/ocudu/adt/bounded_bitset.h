// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "ocudu/adt/detail/bitset_base.h"
#include "ocudu/adt/span.h"
#include "ocudu/adt/static_vector.h"
#include "ocudu/support/math/bit_ops.h"
#include "ocudu/support/math/math_utils.h"
#include "ocudu/support/ocudu_assert.h"
#include "fmt/format.h"

namespace ocudu {

namespace detail {

struct default_bounded_bitset_tag {};

} // namespace detail

/// \brief Represents a dynamically-sized bitset with an upper bound capacity of N bits.
///
/// The bounded_bitset is represented internally via an array of uint64_t, with each integer storing a bitmap.
/// This class also offers many standard logic manipulation methods, like ::any(), operators &=, &, |=, |, etc. and
/// utility methods to convert the bitset into strings or integers.
///
/// Depending on the passed \c LowestInfoBitIsMSB template argument, this class can represent the bits of the bitset in
/// different orders. E.g.
///
/// bounded_bitset<6, false> a(5); // Bitset of 5 bits. LSB is Lowest Information Bit (bit 0).
/// a.set(1);
/// assert(a.to_uint64() == 0b00010);
/// bounded_bitset<6, true> b(5); // Bitset of 5 bits. MSB is Lowest Information Bit (bit 0).
/// b.set(1);
/// assert(a.to_uint64() == 0b0100000000000000);
///
/// The \c LowestInfoBitIsMSB template argument also affects the default string representation of the bitset. E.g.
/// fmt::print("{:b}", a); // prints "00010".
/// fmt::print("{:x}", a); // prints "2".
/// fmt::print("{:b}", b); // prints "01000".
/// fmt::print("{:x}", b); // prints "8".
///
/// However, it does not affect the information bit position string representation, e.g.
/// fmt::print("{:n}", a); // prints "0".
/// fmt::print("{:n}", b); // prints "1".
///
/// \tparam N Upper bound for bitset size in number of bits.
/// \tparam LowestInfoBitIsMSB Bit index order in memory. If set to (false / true), the bit index 0 (Lowest Information
/// Bit) corresponds to either the LSB or MSB of the bitset. Note that this argument has an effect on the underlying
/// bitset memory layout.
template <size_t N, bool LowestInfoBitIsMSB = false, typename Tag = detail::default_bounded_bitset_tag>
class bounded_bitset : public detail::bitset_base<bounded_bitset<N, LowestInfoBitIsMSB, Tag>, N, LowestInfoBitIsMSB>
{
  using base_t = detail::bitset_base<bounded_bitset<N, LowestInfoBitIsMSB, Tag>, N, LowestInfoBitIsMSB>;
  using word_t = typename base_t::word_t;
  using LIB    = std::bool_constant<LowestInfoBitIsMSB>;
  friend base_t;

  static constexpr size_t bits_per_word = base_t::bits_per_word;

public:
  constexpr bounded_bitset() = default;

  constexpr explicit bounded_bitset(size_t cur_size_) : cur_size(cur_size_)
  {
    report_fatal_error_if_not(cur_size_ <= this->max_size(),
                              "The bounded_bitset current size cannot exceed its maximum size");
  }

  constexpr bounded_bitset(const bounded_bitset& other) noexcept : cur_size(other.cur_size)
  {
    std::copy(other.buffer.begin(), other.buffer.begin() + nof_words_(), this->buffer.begin());
  }

  /// \brief Constructs a bitset using iterators.
  ///
  /// The constructed bitset size is equal to <tt> end - begin </tt> size. The values in the list are mapped one to one
  /// starting from begin.
  ///
  /// \tparam Iterator Boolean iterator type.
  /// \param[in] begin Begin iterator.
  /// \param[in] end End iterator.
  template <typename Iterator,
            std::enable_if_t<std::is_convertible_v<typename std::iterator_traits<Iterator>::value_type, bool>, int> = 0>
  constexpr bounded_bitset(Iterator begin, Iterator end)
  {
    resize(end - begin);
    for (size_t count = 0; count != cur_size; ++count, ++begin) {
      this->set_(count, *begin);
    }
  }

  /// \brief Constructs a bitset from an initializer list.
  ///
  /// The constructed bitset size is equal to \c values size. The values in the list are mapped one to one.
  ///
  /// \param[in] values Boolean initializer list.
  constexpr bounded_bitset(const std::initializer_list<const bool>& values)
  {
    resize(values.size());
    const auto* it = values.begin();
    for (size_t count = 0; count != cur_size; ++count) {
      this->set_(count, *it);
      ++it;
    }
  }

  constexpr bounded_bitset& operator=(const bounded_bitset& other) noexcept
  {
    if (this != &other) {
      // In case of shrink, reset erased bits.
      const size_t other_nof_words = other.nof_words_();
      const size_t this_nof_words  = nof_words_();
      ocudu_assume(other_nof_words <= this->buffer.size());
      ocudu_assume(this_nof_words <= this->buffer.size());
      // Note: We use < comparison because other can be larger than this.
      for (size_t i = other_nof_words; i < this_nof_words; ++i) {
        this->buffer[i] = static_cast<word_t>(0);
      }
      cur_size = other.cur_size;
      std::copy(other.buffer.begin(), other.buffer.begin() + other_nof_words, this->buffer.begin());
    }
    return *this;
  }

  template <typename BoundedBitSet>
  BoundedBitSet convert_to() const
  {
    static_assert(BoundedBitSet::max_size() == this->max_size() and BoundedBitSet::bit_order() == this->bit_order(),
                  "Conversion only supported for same N and LowestInfoBitIsMSB");
    // Just the tag changes.
    BoundedBitSet ret(size());
    ret.buffer = this->buffer;
    return ret;
  }

  /// Current size of the bounded_bitset in bits.
  OCUDU_FORCE_INLINE constexpr size_t size() const noexcept { return cur_size; }

  /// \brief Resize of the bounded_bitset. If <tt> new_size > max_size() </tt>, an assertion is triggered. The newly
  /// created are set to zero.
  constexpr void resize(size_t new_size)
  {
    if (new_size == cur_size) {
      return;
    }
    report_fatal_error_if_not(
        new_size <= this->max_size(), "ERROR: new size='{}' exceeds bitset capacity='{}'", new_size, this->max_size());
    const size_t prev_size = size();
    cur_size               = new_size;
    if (new_size < prev_size) {
      // Shrinking case. Need to sanitize removed bits.
      sanitize_();
      const size_t prev_nof_words = divide_ceil(prev_size, bits_per_word);
      const size_t new_nof_words  = divide_ceil(new_size, bits_per_word);
      ocudu_assume(prev_nof_words <= this->buffer.size());
      for (size_t i = new_nof_words; i < prev_nof_words; ++i) {
        this->buffer[i] = static_cast<word_t>(0);
      }
    }
  }

  /// \brief Appends a bit with value \c val to the set.
  ///
  /// Assertion is triggered if the resultant size exceeds the maximum size of the bitset.
  void push_back(bool val)
  {
    size_t bitpos = size();
    resize(bitpos + 1);
    this->set(bitpos, val);
  }

  /// \brief Appends \c nof_bits bits to the set.
  ///
  /// The least \c nof_bits significant bits of \c val are appended to the set, starting from the most significant bit
  /// and finishing with the least significant bit.
  ///
  /// Assertion is triggered if the resultant size exceeds the maximum size of the bitset.
  template <typename Integer>
  void push_back(Integer val, unsigned nof_bits)
  {
    static_assert(std::is_unsigned_v<Integer>, "push_back only works for unsigned integers");
    unsigned bitpos = size();
    resize(bitpos + nof_bits);
    for (unsigned bit_index = 0; bit_index != nof_bits; ++bit_index) {
      this->set(bitpos + bit_index, (val >> (nof_bits - 1 - bit_index)) & 1U);
    }
  }

  /// \brief Extracts \c nof_bits starting from \c startpos.
  ///
  /// \return An unsigned integer containing \c nof_bits of the set where starting with the most significant bit and
  /// finishing with the most significant bit.
  /// \remark An assertion is triggered if the bit range exceed the set size.
  template <typename Integer = unsigned>
  Integer extract(unsigned startpos, unsigned nof_bits) const
  {
    static_assert(std::is_unsigned_v<Integer>, "Extract only works for unsigned integers");
    ocudu_assert(nof_bits <= sizeof(Integer) * 8,
                 "The number of bits (i.e., {}) exceeds the destination bit-width (i.e., {}).",
                 nof_bits,
                 sizeof(Integer) * 8);
    ocudu_assert(startpos + nof_bits <= size(),
                 "The start position (i.e., {}) plus the number of bits (i.e., {}) exceed the current size (i.e., {})",
                 startpos,
                 nof_bits,
                 size());

    Integer val = 0;

    for (unsigned bit_index = 0; bit_index != nof_bits; ++bit_index) {
      if (this->test(startpos + bit_index)) {
        val |= Integer{1} << (nof_bits - 1 - bit_index);
      }
    }

    return val;
  }

  /// \brief Returns bounded_bitset<> that represents a slice or subview of the original bounded_bitset. Unless
  /// it is specified, the returned slice has the same template parameters "N" and "LowestInfoBitIsMSB" of "this".
  ///
  /// \param[in] startpos The bit index where the subview starts.
  /// \param[in] endpos The bit index where the subview stops.
  template <size_t N2 = N, typename NewTag = Tag>
  bounded_bitset<N2, LowestInfoBitIsMSB, NewTag> slice(size_t startpos, size_t endpos) const
  {
    bounded_bitset<N2, LowestInfoBitIsMSB, NewTag> sliced(endpos - startpos);
    const unsigned                                 start_word = startpos / bits_per_word;
    unsigned                                       start_mod  = startpos % bits_per_word;
    const auto                                     nwords     = nof_words_();

    if (start_mod != 0) {
      if constexpr (LowestInfoBitIsMSB) {
        const auto left_mask  = mask_msb_ones<word_t>(bits_per_word - start_mod);
        const auto right_mask = mask_msb_ones<word_t>(start_mod);
        for (unsigned i = 0, sl_nw = sliced.nof_words_(); i != sl_nw; ++i) {
          sliced.buffer[i] = (this->buffer[i + start_word] << start_mod) & left_mask;
          if (i + start_word + 1 < nwords) {
            ocudu_assume(i < sliced.buffer.size());
            sliced.buffer[i] |= (this->buffer[i + start_word + 1] & right_mask) >> (bits_per_word - start_mod);
          }
        }
      } else {
        const auto left_mask  = mask_lsb_ones<word_t>(bits_per_word - start_mod);
        const auto right_mask = mask_lsb_ones<word_t>(start_mod);
        for (unsigned i = 0, sl_nw = sliced.nof_words_(); i != sl_nw; ++i) {
          sliced.buffer[i] = (this->buffer[i + start_word] >> start_mod) & left_mask;
          if (i + start_word + 1 < nwords) {
            sliced.buffer[i] |= (this->buffer[i + start_word + 1] & right_mask) << (bits_per_word - start_mod);
          }
        }
      }
    } else {
      for (unsigned i = 0, sl_nw = sliced.nof_words_(); i != sl_nw; ++i) {
        sliced.buffer[i] = this->buffer[i + start_word];
      }
    }
    sliced.sanitize_();
    return sliced;
  }

  /// \brief Kronecker product of the bitset with another bitset.
  ///
  /// Expands the bitset by a factor of \c other.size() replacing every \c true bit with the contents of \c other and
  /// every \c false bit with \c other.size() \c false bits.
  ///
  /// \tparam Factor    Maximum expansion factor.
  /// \param[in] other  Bitset used for expansion.
  /// \return The result of the bitset product.
  /// \remark The current implementation supports only a bitset containing one word. An assertion is triggered if \c
  /// other contains more than one word.
  template <unsigned long Factor>
  bounded_bitset<Factor * N> kronecker_product(const bounded_bitset<Factor>& other) const
  {
    static_assert(Factor <= bits_per_word,
                  "The current algorithm does not support a filter containing more than one word.");

    // Prepare an empty result.
    bounded_bitset<Factor * N> result(size() * other.size());

    // Places the contents of other centered at the positions indicated by the true bits.
    auto kronecker_expansion = [&other, &result](unsigned bit_index) {
      unsigned bitpos = bit_index * Factor;
      word_t   word   = other.buffer[0];

      unsigned bit_offset = bitpos % bits_per_word;
      unsigned word_index = bitpos / bits_per_word;

      result.buffer[word_index] |= (word << bit_offset);
      if (bit_offset && (bit_offset + other.size() > bits_per_word)) {
        result.buffer[word_index + 1] |= (word >> (bits_per_word - bit_offset));
      }
    };

    if (this->is_contiguous(true)) {
      int i_begin = this->find_lowest(true);
      int i_end   = this->find_highest(true) + 1;

      if ((i_begin < 0) || (i_end <= 0)) {
        // Empty bitset.
        return result;
      }

      // If the bitset in contiguous and the other bitset is all set, then use fill.
      if (other.all()) {
        result.fill(i_begin * other.size(), i_end * other.size());
      } else {
        // Otherwise, place the contents of other into contiguous bit positions.
        for (int i_bit = i_begin; i_bit != i_end; ++i_bit) {
          kronecker_expansion(i_bit);
        }
      }

      ocudu_assert(this->count() * other.count() == result.count(),
                   "The resultant number of ones is not consistent with inputs. It expected {} but got {}.",
                   this->count() * other.count(),
                   result.count());
      return result;
    }

    // Place the contents of other into arbitrary bit positions.
    this->for_each(0, size(), kronecker_expansion);

    ocudu_assert(this->count() * other.count() == result.count(),
                 "The resultant number of ones is not consistent with inputs. It expected {} but got {}.",
                 this->count() * other.count(),
                 result.count());

    return result;
  }

private:
  template <size_t N2, bool reversed2, typename Tag2>
  friend class bounded_bitset;
  friend struct fmt::formatter<bounded_bitset<N, LowestInfoBitIsMSB, Tag>>;

  size_t cur_size = 0;

  // Expose nof_words_() for use by slice() and operator=().
  OCUDU_FORCE_INLINE constexpr size_t nof_words_() const noexcept { return divide_ceil(cur_size, bits_per_word); }

  constexpr void sanitize_()
  {
    const size_t n = cur_size % bits_per_word;
    if (n != 0) {
      const size_t nwords = nof_words_();
      if constexpr (LowestInfoBitIsMSB) {
        this->buffer[nwords - 1] &= ~((~static_cast<word_t>(0)) >> n);
      } else {
        this->buffer[nwords - 1] &= ~((~static_cast<word_t>(0)) << n);
      }
    }
  }
};

/// \brief Bitwise AND operation result = lhs & rhs.
/// \return new bounded_bitset that results from the Bitwise AND operation.
template <size_t N, bool LowestInfoBitIsMSB>
inline bounded_bitset<N, LowestInfoBitIsMSB> operator&(const bounded_bitset<N, LowestInfoBitIsMSB>& lhs,
                                                       const bounded_bitset<N, LowestInfoBitIsMSB>& rhs) noexcept
{
  bounded_bitset<N, LowestInfoBitIsMSB> res(lhs);
  res &= rhs;
  return res;
}

/// \brief Bitwise AND operation result = lhs | rhs.
/// \return new bounded_bitset that results from the Bitwise OR operation.
template <size_t N, bool LowestInfoBitIsMSB>
inline bounded_bitset<N, LowestInfoBitIsMSB> operator|(const bounded_bitset<N, LowestInfoBitIsMSB>& lhs,
                                                       const bounded_bitset<N, LowestInfoBitIsMSB>& rhs) noexcept
{
  bounded_bitset<N, LowestInfoBitIsMSB> res(lhs);
  res |= rhs;
  return res;
}

/// \brief Flip bits from left to right.
/// \return new bounded_bitset that results from the fliplr operation.
template <size_t N, bool LowestInfoBitIsMSB>
inline bounded_bitset<N, LowestInfoBitIsMSB> fliplr(const bounded_bitset<N, LowestInfoBitIsMSB>& other) noexcept
{
  bounded_bitset<N, LowestInfoBitIsMSB> ret(other.size());
  for (uint32_t i = 0; i < ret.size(); ++i) {
    if (other.test(i)) {
      ret.set(ret.size() - 1 - i);
    }
  }
  return ret;
}

/// Iterate over bits set to 1 in the bitset, from begin() until \c predicate returns true.
/// \return The position of the first bit for which \c predicate returns true, or -1 if no such bit exists.
template <size_t N, bool LowestInfoBitIsMSB, typename Tag, typename FindPred>
int find_first(const bounded_bitset<N, LowestInfoBitIsMSB, Tag>& bitset, FindPred&& predicate)
{
  auto last = bitset.find_highest(0, bitset.size(), true);
  if (last < 0) {
    return -1;
  }
  size_t end = static_cast<size_t>(last) + 1;

  for (size_t start = 0; start != end; ++start) {
    auto pos = bitset.find_lowest(start, end, true);
    if (pos < 0) {
      break;
    }
    start = static_cast<size_t>(pos);
    if (predicate(start)) {
      return pos;
    }
  }
  return -1;
}

/// \brief Divides a bitset of size "S" into "M" smaller bitsets, where each bitset has length "L=S/M". A bitwise-or
/// operation is performed across bitsets. At the end, a slice with an offset "O" and length "K" is taken from the
/// bitset of length "L" that resulted from the bitwise-or operation.
/// This operation is equivalent to reshaping an array of bits of size "S" into a matrix of dimensions "(M, L)" and
/// applying an "or" operation across all bits of each column. The resulting array of "L" bits, is then sliced with
/// an offset "O" and length "K".
/// The operation asserts if "S % L != 0".
/// E.g. Consider the bitset 1000 0100 0000 1001 (S=16), L=4, O=1, K=2. This function performs the following steps:
/// 1. Break the bitset into M=S/L=4 parts: {1000, 0100, 0000, 1001}.
/// 2. Bitwise-or all the M parts: 1101.
/// 3. Slice the bitset obtained in 2. with offset O=1 and slice length K=2: 10.
///
/// \tparam N2 maximum bitset size for returned bitset.
/// \param[in] other original bitset of length "S".
/// \param[in] fold_length length of each folded bitset "L".
/// \param[in] slice_offset offset from where to slice each fold "O".
/// \param[in] slice_length length of the slice taken from each fold "K".
/// \return bitset of size slice_length with the or-accumulated folds.
template <size_t N2, size_t N, bool LowestInfoBitIsMSB>
inline bounded_bitset<N2, LowestInfoBitIsMSB> fold_and_accumulate(const bounded_bitset<N, LowestInfoBitIsMSB>& other,
                                                                  size_t fold_length,
                                                                  size_t slice_offset,
                                                                  size_t slice_length) noexcept
{
  ocudu_assert(
      other.size() % fold_length == 0, "Invalid fold length={} for bitset of size={}", fold_length, other.size());
  bounded_bitset<N2, LowestInfoBitIsMSB> ret(slice_length);
  for (size_t i = 0; i != other.size(); i += fold_length) {
    ret |= other.template slice<N2>(i + slice_offset, i + slice_offset + slice_length);
  }
  return ret;
}

/// \brief Performs the fold and accumulate operation, but without slicing at the end.
///
/// \tparam N2 maximum bitset size for returned bitset.
/// \tparam LowestInfoBitIsMSB internal bit order representation of returned bitset.
/// \param[in] other original bitset from where folds are generated.
/// \param[in] fold_length length of each fold bitset.
/// \return bitset of size fold_length with the accumulated folds.
template <size_t N2, size_t N, bool LowestInfoBitIsMSB>
inline bounded_bitset<N2, LowestInfoBitIsMSB> fold_and_accumulate(const bounded_bitset<N, LowestInfoBitIsMSB>& other,
                                                                  size_t fold_length) noexcept
{
  return fold_and_accumulate<N2, N, LowestInfoBitIsMSB>(other, fold_length, 0, fold_length);
}

/// \brief Executes a function for all \c true (or all \c false) intervals in the given bitset interval.
///
/// The method calls \c function for each interval, passing the first and last bit positions of the interval.
///
/// \param[in] startpos Smallest bit index considered for the function execution (included).
/// \param[in] endpos   Largest bit index considered for the function execution (excluded).
/// \param[in] function Function to execute - the signature should be compatible with <tt>void ()(size_t,size_t)</tt>.
/// \param[in] value    Bit value that triggers the function execution.
template <size_t N, bool LowestInfoBitIsMSB, typename Tag, class Func>
void for_each_interval(const bounded_bitset<N, LowestInfoBitIsMSB, Tag>& bitset,
                       size_t                                            startpos,
                       size_t                                            endpos,
                       Func&&                                            function,
                       bool                                              value = true)
{
  static_assert(std::is_convertible_v<Func, std::function<void(size_t, size_t)>>,
                "The function must have void(size_t) signature.");

  // Iterate for all intervals.
  for (int start_interval = bitset.find_lowest(startpos, endpos, value); start_interval != (-1);
       start_interval     = bitset.find_lowest(start_interval, endpos, value)) {
    // Find the end of the current interval.
    int end_interval = bitset.find_lowest(start_interval, endpos, !value);

    // If no more ending, force the end position.
    if (end_interval == -1) {
      end_interval = endpos;
    }

    // Call function.
    function(start_interval, end_interval);

    // Advance interval.
    start_interval = end_interval;
  }
}

// Executes a function for all \c true (or all \c false) intervals in the given bitset.
template <size_t N, bool LowestInfoBitIsMSB, typename Tag, class Func>
void for_each_interval(const bounded_bitset<N, LowestInfoBitIsMSB, Tag>& bitset, Func&& function, bool value = true)
{
  for_each_interval(bitset, 0, bitset.size(), function, value);
}

/// Converts a list of bit positions to a bounded_bitset.
template <size_t N,
          bool   LowestInfoBitIsMSB = false,
          typename Tag              = detail::default_bounded_bitset_tag,
          typename RangeType,
          typename PosInteger = typename RangeType::value_type>
bounded_bitset<N, LowestInfoBitIsMSB, Tag> bit_positions_to_bitset(const RangeType& bit_positions)
{
  bounded_bitset<N, LowestInfoBitIsMSB, Tag> result(N);
  int                                        max_pos = -1;
  for (PosInteger pos : bit_positions) {
    result.set(static_cast<size_t>(pos));
    max_pos = std::max(max_pos, static_cast<int>(pos));
  }
  result.resize(max_pos + 1);
  return result;
}

} // namespace ocudu

namespace fmt {

/// \brief Custom formatter for bounded_bitset<N, LowestInfoBitIsMSB, Tag>
template <size_t N, bool LowestInfoBitIsMSB, typename Tag>
struct formatter<ocudu::bounded_bitset<N, LowestInfoBitIsMSB, Tag>> {
  enum { hexadecimal, binary, bit_positions, intervals } mode = binary;
  enum { forward, reverse } order                             = forward;
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
      if (*it == 'i') {
        mode = intervals;
      }
      ++it;
    }

    return it;
  }

  template <typename FormatContext>
  auto format(const ocudu::bounded_bitset<N, LowestInfoBitIsMSB, Tag>& s, FormatContext& ctx) const
  {
    if (mode == hexadecimal) {
      return s.template to_string_of_hex<decltype(std::declval<FormatContext>().out())>(ctx.out(), order == reverse);
    }

    if (mode == intervals) {
      bool first = true;
      fmt::format_to(ctx.out(), "{{");
      for_each_interval(s, [&first, &ctx](size_t start_interval, size_t end_interval) {
        // Append a comma if the interval is not the first.
        if (first) {
          first = false;
        } else {
          fmt::format_to(ctx.out(), ", ");
        }

        // Print interval if it is more than one bit, otherwise a single value.
        if (end_interval - start_interval > 1) {
          fmt::format_to(ctx.out(), "[{}, {})", start_interval, end_interval);
        } else {
          fmt::format_to(ctx.out(), "{}", start_interval);
        }
      });
      fmt::format_to(ctx.out(), "}}");
      return ctx.out();
    }

    if (mode == bit_positions) {
      if (s.empty()) {
        fmt::format_to(ctx.out(), "empty");
      } else if (s.count() == 0) {
        fmt::format_to(ctx.out(), "none");
      } else if (s.is_contiguous()) {
        unsigned lowest  = s.find_lowest();
        unsigned highest = s.find_highest();
        if (lowest == highest) {
          // Single value.
          fmt::format_to(ctx.out(), "{}", lowest);
        } else {
          // Format as a range.
          fmt::format_to(ctx.out(), "[{}, {})", lowest, highest + 1);
        }

      } else {
        // Format as a list of bit positions.
        ocudu::static_vector<size_t, N> bit_pos = s.get_bit_positions();

        fmt::format_to(ctx.out(), "{}", ocudu::span<size_t>(bit_pos));
      }
      return ctx.out();
    }

    return s.template to_string_of_bits<decltype(std::declval<FormatContext>().out())>(ctx.out(), order == reverse);
  }
};

} // namespace fmt
