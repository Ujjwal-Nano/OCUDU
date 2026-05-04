// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "ocudu/adt/span.h"
#include "ocudu/adt/static_vector.h"
#include "ocudu/support/math/bit_ops.h"
#include "ocudu/support/math/math_utils.h"
#include "ocudu/support/ocudu_assert.h"
#include "fmt/format.h"
#include <array>
#include <cinttypes>

namespace ocudu {
namespace detail {

/// \brief CRTP base class for bitset types.
///
/// Provides the common bit manipulation logic for bounded_bitset and fixed_size_bitset.
/// The derived class must implement a public \c size() const noexcept method returning the current number of bits.
///
/// \tparam Derived            The concrete bitset class (CRTP).
/// \tparam N                  Maximum number of bits.
/// \tparam LowestInfoBitIsMSB If true, bit index 0 is the MSB of the underlying word.
template <typename Derived, size_t N, bool LowestInfoBitIsMSB>
class bitset_base
{
protected:
  using word_t                          = uint64_t;
  static constexpr size_t bits_per_word = 8U * sizeof(word_t);

  // Capacity of the underlying array in number of words.
  static constexpr size_t max_nof_words_() noexcept { return (N + bits_per_word - 1) / bits_per_word; }

  std::array<word_t, max_nof_words_()> buffer{};

public:
  static constexpr bool bit_order() noexcept { return LowestInfoBitIsMSB; }

  static constexpr size_t max_size() noexcept { return N; }

  OCUDU_FORCE_INLINE constexpr bool empty() const noexcept { return size_() == 0; }

  void set(size_t pos, bool val)
  {
    assert_within_bounds_(pos, true);
    set_(pos, val);
  }

  void set(size_t pos)
  {
    assert_within_bounds_(pos, true);
    set_(pos);
  }

  void reset(size_t pos)
  {
    assert_within_bounds_(pos, true);
    reset_(pos);
  }

  void reset() noexcept
  {
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      buffer[i] = static_cast<word_t>(0);
    }
  }

  void fill(bool val = true) noexcept
  {
    if (not val) {
      reset();
      return;
    }
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      buffer[i] = static_cast<word_t>(-1);
    }
    sanitize_();
  }

  Derived& fill(size_t startpos, size_t endpos, bool value = true)
  {
    find_first_word_(*this, startpos, endpos, [value](word_t& w, const word_t& mask) {
      if (value) {
        w |= mask;
      } else {
        w &= ~mask;
      }
      return false;
    });
    return *static_cast<Derived*>(this);
  }

  [[nodiscard]] constexpr bool test(size_t pos) const
  {
    assert_within_bounds_(pos, true);
    return test_(pos);
  }

  void flip(size_t pos)
  {
    assert_within_bounds_(pos, true);
    if (test(pos)) {
      reset_(pos);
    } else {
      set_(pos);
    }
  }

  Derived& flip() noexcept
  {
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      buffer[i] = ~buffer[i];
    }
    sanitize_();
    return *static_cast<Derived*>(this);
  }

  int find_lowest(bool value = true) const noexcept { return find_lowest(0, size_(), value); }

  int find_lowest(size_t startpos, size_t endpos, bool value = true) const noexcept
  {
    int pos = -1;
    find_first_word_(*this, startpos, endpos, [this, value, &pos](const word_t& word_ref, const word_t& mask) {
      word_t w = value ? word_ref : ~word_ref;
      w &= mask;
      if (w != 0) {
        pos = (&word_ref - buffer.data()) * bits_per_word;
        if constexpr (LowestInfoBitIsMSB) {
          pos += convert_bitpos_(find_first_msb_one(w));
        } else {
          pos += find_first_lsb_one(w);
        }
        return true;
      }
      return false;
    });
    return pos;
  }

  int find_highest(bool value = true) const noexcept { return find_highest(0, size_(), value); }

  int find_highest(size_t startpos, size_t endpos, bool value = true) const noexcept
  {
    assert_range_bounds_(startpos, endpos);
    if (startpos == endpos) {
      return -1;
    }
    size_t startword = startpos / bits_per_word;
    size_t lastword  = (endpos - 1) / bits_per_word;

    for (size_t i = lastword; i != startword - 1; --i) {
      word_t w = buffer[i];
      if (not value) {
        w = ~w;
      }

      if (i == startword) {
        size_t removed_bits = startpos % bits_per_word;
        if constexpr (LowestInfoBitIsMSB) {
          w &= mask_msb_zeros<word_t>(removed_bits);
        } else {
          w &= mask_lsb_zeros<word_t>(removed_bits);
        }
      }
      if (i == lastword) {
        size_t kept_bits = ((endpos - 1) % bits_per_word) + 1;
        if constexpr (LowestInfoBitIsMSB) {
          w &= mask_msb_ones<word_t>(kept_bits);
        } else {
          w &= mask_lsb_ones<word_t>(kept_bits);
        }
      }
      if (w != 0) {
        if constexpr (LowestInfoBitIsMSB) {
          return static_cast<int>(i * bits_per_word + convert_bitpos_(find_first_lsb_one(w)));
        } else {
          return static_cast<int>(i * bits_per_word + find_first_msb_one(w));
        }
      }
    }
    return -1;
  }

  template <class T>
  void for_each(size_t startpos, size_t endpos, T&& function, bool value = true) const noexcept
  {
    static_assert(std::is_convertible_v<T, std::function<void(size_t)>>,
                  "The function must have void(size_t) signature.");
    static_assert(!LowestInfoBitIsMSB, "The for_each method is not yet available for reversed bitsets.");

    assert_range_bounds_(startpos, endpos);

    if (startpos == endpos) {
      return;
    }

    if ((value && all(startpos, endpos)) || (!value && none(startpos, endpos))) {
      for (size_t bitpos = startpos; bitpos != endpos; ++bitpos) {
        function(bitpos);
      }
      return;
    }

    size_t startword = startpos / bits_per_word;
    size_t lastword  = (endpos + bits_per_word - 1) / bits_per_word;
    for (size_t i = startword; i != lastword; ++i) {
      word_t w = buffer[i];
      if (not value) {
        w = ~w;
      }

      if (w == 0) {
        continue;
      }

      if (i == startword) {
        w &= mask_lsb_zeros<word_t>(startpos % bits_per_word);
      }

      if ((i == lastword - 1) && (endpos % bits_per_word != 0)) {
        w &= mask_lsb_ones<word_t>(endpos % bits_per_word);
      }

      unsigned bitpos = i * bits_per_word;
      for (; w != 0; w = w >> 4, bitpos += 4) {
        switch (w & 0xf) {
          case 0B0000:
            break;
          case 0B0001:
            function(bitpos + 0);
            break;
          case 0B0010:
            function(bitpos + 1);
            break;
          case 0B0011:
            function(bitpos + 0);
            function(bitpos + 1);
            break;
          case 0B0100:
            function(bitpos + 2);
            break;
          case 0B0101:
            function(bitpos + 0);
            function(bitpos + 2);
            break;
          case 0B0110:
            function(bitpos + 1);
            function(bitpos + 2);
            break;
          case 0B0111:
            function(bitpos + 0);
            function(bitpos + 1);
            function(bitpos + 2);
            break;
          case 0B1000:
            function(bitpos + 3);
            break;
          case 0B1001:
            function(bitpos + 0);
            function(bitpos + 3);
            break;
          case 0B1010:
            function(bitpos + 1);
            function(bitpos + 3);
            break;
          case 0B1011:
            function(bitpos + 0);
            function(bitpos + 1);
            function(bitpos + 3);
            break;
          case 0B1100:
            function(bitpos + 2);
            function(bitpos + 3);
            break;
          case 0B1101:
            function(bitpos + 0);
            function(bitpos + 2);
            function(bitpos + 3);
            break;
          case 0B1110:
            function(bitpos + 1);
            function(bitpos + 2);
            function(bitpos + 3);
            break;
          case 0B1111:
          default:
            function(bitpos + 0);
            function(bitpos + 1);
            function(bitpos + 2);
            function(bitpos + 3);
            break;
        }
      }
    }
  }

  bool all() const noexcept
  {
    const size_t nw = nof_words_();
    if (nw == 0) {
      return true;
    }
    word_t allset = ~static_cast<word_t>(0);
    for (size_t i = 0; i < nw - 1; i++) {
      if (buffer[i] != allset) {
        return false;
      }
    }
    if constexpr (LowestInfoBitIsMSB) {
      return buffer[nw - 1] == (allset << (nw * bits_per_word - size_()));
    } else {
      return buffer[nw - 1] == (allset >> (nw * bits_per_word - size_()));
    }
  }

  bool all(size_t start, size_t stop) const
  {
    bool not_all_found = find_first_word_(*this, start, stop, [](const word_t& word, const word_t& mask) {
      return (word | ~mask) != ~static_cast<word_t>(0);
    });
    return !not_all_found;
  }

  bool any() const noexcept
  {
    for (size_t i = 0, sz = nof_words_(); i != sz; ++i) {
      if (buffer[i] != static_cast<word_t>(0)) {
        return true;
      }
    }
    return false;
  }

  bool any(size_t start, size_t stop) const
  {
    bool any_found = find_first_word_(
        *this, start, stop, [](const word_t& w, const word_t& mask) { return (w & mask) != static_cast<word_t>(0); });
    return any_found;
  }

  bool none() const noexcept { return !any(); }

  bool none(size_t start, size_t stop) const noexcept { return !any(start, stop); }

  bool is_contiguous(bool value = true) const noexcept
  {
    int startpos = find_lowest(0, size_(), value);

    if (startpos == -1) {
      return true;
    }

    int endpos = find_highest(startpos + 1, size_(), value);

    if (endpos == -1) {
      return true;
    }

    size_t value_count = count();
    if (not value) {
      value_count = size_() - value_count;
    }

    return (value_count == static_cast<size_t>((endpos + 1) - startpos));
  }

  size_t count() const noexcept
  {
    int result = 0;
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      result += count_ones(buffer[i]);
    }
    return result;
  }

  bool operator==(const Derived& other) const noexcept
  {
    if (size_() != other.size()) {
      return false;
    }
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      if (buffer[i] != other.buffer[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const Derived& other) const noexcept { return not(*this == other); }

  Derived& operator|=(const Derived& other)
  {
    ocudu_assert(other.size() == size_(),
                 "ERROR: operator|= called for bitsets of different sizes ('{}'!='{}')",
                 size_(),
                 other.size());
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      buffer[i] |= other.buffer[i];
    }
    return *static_cast<Derived*>(this);
  }

  Derived& operator&=(const Derived& other)
  {
    ocudu_assert(other.size() == size_(),
                 "ERROR: operator&= called for bitsets of different sizes ('{}'!='{}')",
                 size_(),
                 other.size());
    for (size_t i = 0, nw = nof_words_(); i != nw; ++i) {
      buffer[i] &= other.buffer[i];
    }
    return *static_cast<Derived*>(this);
  }

  Derived operator~() const noexcept
  {
    Derived ret(*static_cast<const Derived*>(this));
    ret.flip();
    return ret;
  }

  uint64_t to_uint64() const
  {
    ocudu_assert(nof_words_() == 1, "ERROR: cannot convert bitset of size='{}' to uint64_t", size_());
    if constexpr (LowestInfoBitIsMSB) {
      const size_t rem = size_() % bits_per_word;
      return (rem == 0) ? buffer[0] : (buffer[0] >> (bits_per_word - rem));
    }
    return buffer[0];
  }

  void from_uint64(uint64_t v)
  {
    ocudu_assert(nof_words_() == 1, "ERROR: cannot convert bitset of size='{}' to uint64_t", size_());
    ocudu_assert((size_() == 64U) || (v < (static_cast<uint64_t>(1U) << size_())),
                 "ERROR: Provided mask='{}' does not fit in bitset of size='{}'",
                 v,
                 size_());
    if constexpr (LowestInfoBitIsMSB) {
      const size_t rem = size_() % bits_per_word;
      buffer[0]        = (rem == 0) ? v : (v << (bits_per_word - rem));
    } else {
      buffer[0] = v;
    }
  }

  template <typename UnsignedInteger>
  size_t to_packed_bits(span<UnsignedInteger> packed_bits) const
  {
    static_assert(sizeof(UnsignedInteger) <= sizeof(word_t), "ERROR: provided array type is too large");
    static_assert(std::is_unsigned_v<UnsignedInteger>, "Only unsigned integers are supported");
    static constexpr size_t steps_per_word   = sizeof(word_t) / sizeof(UnsignedInteger);
    static constexpr size_t bits_per_integer = sizeof(UnsignedInteger) * 8U;
    static constexpr auto   integer_mask     = mask_lsb_ones<word_t>(bits_per_integer);
    const word_t            sz               = size_();
    const unsigned          last_word_steps =
        (sz % bits_per_word) ? divide_ceil(sz % bits_per_word, bits_per_integer) : steps_per_word;
    const unsigned nof_words           = nof_words_();
    const unsigned nof_integers_packed = (nof_words - 1) * steps_per_word + last_word_steps;
    ocudu_assert(
        packed_bits.size() >= nof_integers_packed, "ERROR: provided array size='{}' is too small", packed_bits.size());

    size_t count = 0;
    if (not LowestInfoBitIsMSB) {
      for (unsigned i = 0; i != nof_words; ++i) {
        unsigned nof_steps = i == nof_words - 1 ? last_word_steps : steps_per_word;
        for (unsigned j = 0; j != nof_steps; ++j) {
          packed_bits[count++] = (buffer[i] >> (j * steps_per_word)) & integer_mask;
        }
      }
    } else {
      for (unsigned i = 0; i != nof_words; ++i) {
        word_t   w         = buffer[i];
        unsigned nof_steps = steps_per_word;
        if (i == nof_words - 1) {
          nof_steps = last_word_steps;
        }
        for (unsigned j = 0; j != nof_steps; ++j) {
          packed_bits[count++] = (w >> (bits_per_word - (j + 1) * bits_per_integer)) & integer_mask;
        }
      }
    }

    return nof_integers_packed;
  }

  template <typename UnsignedInteger>
  void to_unpacked_bits(span<UnsignedInteger> unpacked_bits) const
  {
    static_assert(std::is_unsigned_v<UnsignedInteger>, "Only unsigned integers are supported");
    ocudu_assert(size_() == unpacked_bits.size(),
                 "ERROR: provided array size='{}' does not match bitset size='{}'",
                 unpacked_bits.size(),
                 size_());

    for (unsigned i = 0, ie = size_(); i != ie; ++i) {
      unpacked_bits[i] = test(i);
    }
  }

  static_vector<size_t, N> get_bit_positions(bool value = true) const
  {
    static_vector<size_t, N> positions;

    for (size_t i_bit = 0, sz = size_(); i_bit != sz;) {
      int next_position = find_lowest(i_bit, sz, value);
      if (next_position < 0) {
        break;
      }

      positions.emplace_back(static_cast<size_t>(next_position));

      i_bit = next_position + 1;
    }

    return positions;
  }

  template <typename OutputIt>
  OutputIt to_string_of_bits(OutputIt&& mem_buffer, bool reverse) const
  {
    if (size_() == 0) {
      return mem_buffer;
    }

    reverse = reverse ^ LowestInfoBitIsMSB;

    if (!reverse) {
      for (size_t i = size_(); i != 0; --i) {
        fmt::format_to(mem_buffer, "{}", test(i - 1) ? '1' : '0');
      }
    } else {
      for (size_t i = 0; i != size_(); ++i) {
        fmt::format_to(mem_buffer, "{}", test(i) ? '1' : '0');
      }
    }
    return mem_buffer;
  }

  template <typename OutputIt>
  OutputIt to_string_of_hex(OutputIt&& mem_buffer, bool reverse) const
  {
    const size_t sz = size_();
    if (sz == 0) {
      return mem_buffer;
    }
    const size_t rem_bits   = sz % bits_per_word;
    const size_t rem_digits = divide_ceil(rem_bits, 4U);
    const size_t nwords     = nof_words_();

    if (not reverse) {
      if constexpr (LowestInfoBitIsMSB) {
        unsigned i = 0;
        for (; i != nwords - 1; ++i) {
          uint64_t w = buffer[i];
          fmt::format_to(mem_buffer, "{:0>16x}", w);
        }
        word_t w = buffer[i] >> (bits_per_word - rem_bits);
        fmt::format_to(mem_buffer, "{:0>{}x}", w, rem_digits);
      } else {
        int    i = nwords - 1;
        word_t w = buffer[i];
        fmt::format_to(mem_buffer, "{:0>{}x}", w, rem_digits);
        for (--i; i >= 0; --i) {
          fmt::format_to(mem_buffer, "{:0>16x}", buffer[i]);
        }
      }
    } else {
      if constexpr (LowestInfoBitIsMSB) {
        int    i = nwords - 1;
        word_t w = bit_reverse(buffer[i]);
        fmt::format_to(mem_buffer, "{:0>{}x}", w, rem_digits);
        for (--i; i >= 0; --i) {
          fmt::format_to(mem_buffer, "{:0>16x}", bit_reverse(buffer[i]));
        }
      } else {
        unsigned i = 0;
        for (; i != nwords - 1; ++i) {
          uint64_t w = bit_reverse(buffer[i]);
          fmt::format_to(mem_buffer, "{:0>16x}", w);
        }
        word_t w = bit_reverse(buffer[i]) >> (bits_per_word - rem_bits);
        fmt::format_to(mem_buffer, "{:0>{}x}", w, rem_digits);
      }
    }
    return mem_buffer;
  }

protected:
  constexpr void set_(size_t bitpos, bool val) noexcept
  {
    if (val) {
      set_(bitpos);
    } else {
      reset_(bitpos);
    }
  }

  constexpr void set_(size_t bitpos) noexcept
  {
    const size_t word_idx = bitpos / bits_per_word;
    ocudu_assume(word_idx < buffer.size());
    buffer[word_idx] |= maskbit(bitpos);
  }

  constexpr void reset_(size_t bitpos) noexcept
  {
    const size_t word_idx = bitpos / bits_per_word;
    ocudu_assume(word_idx < buffer.size());
    buffer[word_idx] &= ~maskbit(bitpos);
  }

private:
  OCUDU_FORCE_INLINE constexpr size_t size_() const noexcept { return static_cast<const Derived*>(this)->size(); }

  constexpr void sanitize_()
  {
    const size_t n = size_() % bits_per_word;
    if (n != 0) {
      const size_t nwords = nof_words_();
      if constexpr (LowestInfoBitIsMSB) {
        buffer[nwords - 1] &= ~((~static_cast<word_t>(0)) >> n);
      } else {
        buffer[nwords - 1] &= ~((~static_cast<word_t>(0)) << n);
      }
    }
  }

  OCUDU_FORCE_INLINE size_t convert_bitpos_(size_t bitpos) const noexcept
  {
    if constexpr (LowestInfoBitIsMSB) {
      return bits_per_word - 1 - (bitpos % bits_per_word);
    } else {
      return bitpos;
    }
  }

  OCUDU_FORCE_INLINE constexpr bool test_(size_t bitpos) const noexcept
  {
    const size_t word_idx = bitpos / bits_per_word;
    ocudu_assume(word_idx < buffer.size());
    const word_t bitmask = maskbit(bitpos);
    return ((buffer[word_idx] & bitmask) != static_cast<word_t>(0));
  }

  OCUDU_FORCE_INLINE constexpr size_t nof_words_() const noexcept { return divide_ceil(size_(), bits_per_word); }

  constexpr size_t word_idx_(size_t bitidx) const { return bitidx / bits_per_word; }

  constexpr void assert_within_bounds_(size_t pos, bool strict) const noexcept
  {
    ocudu_assert(pos < size_() or (not strict and pos == size_()),
                 "ERROR: index='{}' is out-of-bounds for bitset of size='{}'",
                 pos,
                 size_());
  }

  constexpr void assert_range_bounds_(size_t startpos, size_t endpos) const noexcept
  {
    ocudu_assert(startpos <= endpos and endpos <= size_(),
                 "ERROR: range ['{}', '{}') out-of-bounds for bitsize of size='{}'",
                 startpos,
                 endpos,
                 size_());
  }

  OCUDU_FORCE_INLINE static constexpr word_t maskbit(size_t pos) noexcept
  {
    if constexpr (LowestInfoBitIsMSB) {
      return static_cast<word_t>(1U) << (bits_per_word - 1 - (pos % bits_per_word));
    } else {
      return static_cast<word_t>(1U) << (pos % bits_per_word);
    }
  }

  template <typename Self, typename C>
  static bool find_first_word_(Self& self, size_t start, size_t stop, const C& pred)
  {
    self.assert_range_bounds_(start, stop);
    if (start == stop) {
      return false;
    }
    const size_t startmod    = start % bits_per_word;
    const size_t stopmod     = stop % bits_per_word;
    const size_t start_word  = self.word_idx_(start);
    const size_t end_word    = self.word_idx_(stop - 1) + 1;
    const size_t tail_unused = (stopmod == 0) ? 0 : bits_per_word - stopmod;

    for (size_t i = start_word; i < end_word; ++i) {
      word_t mask = ~static_cast<word_t>(0);
      if (i == start_word && startmod != 0) {
        if constexpr (LowestInfoBitIsMSB) {
          mask &= mask_msb_zeros<word_t>(startmod);
        } else {
          mask &= mask_lsb_zeros<word_t>(startmod);
        }
      }
      if (i == end_word - 1 && stopmod != 0) {
        if constexpr (LowestInfoBitIsMSB) {
          mask &= mask_lsb_zeros<word_t>(tail_unused);
        } else {
          mask &= mask_msb_zeros<word_t>(tail_unused);
        }
      }
      if (pred(self.buffer[i], mask)) {
        return true;
      }
    }
    return false;
  }
};

} // namespace detail
} // namespace ocudu
