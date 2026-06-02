/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "ocudu/adt/complex.h"
#include "ocudu/adt/span.h"

namespace ocudu {

/// \brief Converts packed 12-bit signed integer samples to 16-bit complex integer samples.
/// \remark An assertion is triggered if the input and output sizes are not consistent.
void convert_i12_to_ci16(span<ci16_t> out, span<const uint32_t> in);

/// \brief Converts packed 16-bit signed integer samples to 12-bit complex integer samples.
/// \remark An assertion is triggered if the input and output sizes are not consistent.
void convert_ci16_to_i12(span<uint32_t> out, span<const ci16_t> in);

/// \brief Scales the downlink baseband samples according to the IQ resolution provided by the radio device.
///
/// The output samples are scaled such that the full scale vale of the radio is mapped to the maximum baseband sample
/// value. For example, if the radio device has an IQ resolution of 12 bits, the input samples are scaled by a factor of
/// \f$2^{12-16} = 1/16\f$.
///
/// \param[in] out          Output buffer for the scaled IQ samples.
/// \param[in] in           Input buffer of baseband samples.
/// \param[in] iq_bit_depth IQ resolution of the radio device in number of bits.
/// \remark An assertion is triggered if the input and output sizes are not consistent.
void scale_to_iq_bit_depth(span<int16_t> out, span<const int16_t> in, unsigned iq_bit_depth);

/// \brief Scales the uplink radio samples according to the IQ resolution provided by the radio device.
///
/// The output samples are scaled such that the full scale vale of the radio is mapped to the maximum baseband sample
/// value. For example, if the radio device has an IQ resolution of 12 bits, the input samples are scaled by a factor of
/// \f$2^{16-12} = 16\f$.
///
/// \param[in] out          Output buffer for the scaled baseband samples.
/// \param[in] in           Input buffer of radio samples.
/// \param[in] iq_bit_depth IQ resolution of the radio device in number of bits.
/// \warning The implementation does not check that the input values don't go beyond the specified bit depth.
/// \remark An assertion is triggered if the input and output sizes are not consistent.
void scale_to_baseband_bit_depth(span<int16_t> out, span<const int16_t> in, unsigned iq_bit_depth);

} // namespace ocudu
