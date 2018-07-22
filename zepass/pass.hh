// This file is part of ZEPASSD.
//
// ZEPASSD is Copyright (C) 2018 Phil Vachon
// <phil@security-embedded.com>
//
// ZEPASSD is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Foobar is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <zepass/types.hh>

#include <boost/circular_buffer.hpp>

#include <bitset>
#include <complex>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

namespace zepass {

///
/// \brief Object to describe an EZ-Pass.
/// Provides decoding, signal interpretation and similar.
///
class pass {
public:
    ///
    /// Create a new E-Z Pass state object.
    ///
    static std::shared_ptr<pass> make_pass(double const center_freq_hz_delta,
                                           freq_t const samples_per_interval,
                                           freq_t const sampling_rate,
                                           size_t const interval_len);
    ~pass();

    typedef std::shared_ptr<pass> ptr_t; //< Pointer type for a pass

    /// Return the difference between the radio center frequency and the center
    /// frequency of this pass.
    double get_center_freq_delta() const { return m_center_freq_hz; }

    void accumulate(sample_t const* const sig, sample_t const est_phase, wallclock_t const at);
    void dump_to_file(std::shared_ptr<std::ofstream> ofs) const;
    bool decode();

    bool is_ready() const;

    /// Return the number of samples that have been accumulated into this pass's state
    size_t get_measure_count() const { return m_nr_acc; }

    /// Return whether or not this pass has been successfully decoded
    bool is_decoded() const { return m_decoded; }

    /// Return the last time (relatively) that we updated the pass
    wallclock_t last_updated_at() const { return m_last_at; }

    /// Constructor for an E-Z Pass object.
    pass(double const center_freq_hz_delta,
         freq_t const samples_per_interval,
         freq_t const sampling_rate,
         size_t const interval_len);

    unsigned get_header() const { return m_header; }
    unsigned get_tag_type() const { return m_tag_type; }
    unsigned get_app_id() const { return m_app_id; }
    unsigned get_group_id() const { return m_group_id; }
    unsigned get_agency_id() const { return m_agency_id; }
    unsigned get_serial_number() const { return m_serial_num; }

private:

    size_t find_transition(int &bit) const;
    void set_bit(std::size_t const bit_num, int const bit_value);
    uint64_t get_field(size_t const start, size_t const length) const;
    std::uint16_t calc_crc() const;

    double m_center_freq_hz;
    std::bitset<256> m_raw_data; //< Bit vector of sliced/converted values
    sample_vector_t m_baseband_shift; //< Vector of values to shift this pass to baseband
    sample_vector_t m_accumulated; //< the accumulated sample vector
    size_t m_samples_per_interval; //< The number of samples in the 512us interval
    size_t m_sampling_rate; //< The sampling rate of the input signal
    size_t m_nr_acc; //< The number of accumulated transponder responses
    wallclock_t m_last_at; //< Last time interval this was seen at
    size_t m_interval_len; //< The length of the capture interval, in microseconds
    size_t m_samples_per_bit; //< The number of samples, per bit
    size_t m_window_size = 4; //< Size of the window. TODO: not hardcoded
    boost::circular_buffer<int> m_slice_win; //< The slice window, used in attempting to decode
    bool m_decoded = false; //< Whether or not this pass has been decoded successfully

    unsigned m_header = 0;
    unsigned m_tag_type = 0;
    unsigned m_app_id = 0;
    unsigned m_group_id = 0;
    unsigned m_agency_id = 0;
    unsigned m_serial_num = 0;
};

} // end namespace zepass

/// ostream operator to render the zepass::pass as a JSON object
std::ostream& operator<<(std::ostream& os, zepass::pass const& p);

