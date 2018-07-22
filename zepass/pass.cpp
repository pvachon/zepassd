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

#include <zepass/pass.hh>
#include <zepass/types.hh>
#include <zepass/priv.hh>

#include <boost/crc.hpp>

#include <algorithm>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numeric>

#include <cmath>

using namespace zepass;

std::ostream& operator<<(std::ostream& os, zepass::pass const& p)
{
    if (p.is_decoded()) {
        std::time_t now = std::time(nullptr);
        std::tm *utc = std::gmtime(&now);

        os << std::dec << "{\"passHeader\":" << p.get_header() <<
            ", \"tagType\":" << p.get_tag_type() <<
            ", \"appId\":" << p.get_app_id() <<
            ", \"groupId\":" << p.get_group_id() <<
            ", \"agencyId\":" << p.get_agency_id() <<
            ", \"serialNum\":" << p.get_serial_number() <<
            ", \"lastSeenAt\":" << p.last_updated_at() <<
            ", \"nrSamples\":" << p.get_measure_count() <<
            ", \"centerFreqDelta\":"<< p.get_center_freq_delta() <<
            ", \"seenAt\": \"" << std::setfill('0') <<
                std::setw(4) << utc->tm_year + 1900 << "-" <<
                std::setw(2) << utc->tm_mon + 1 << "-" <<
                std::setw(2) << utc->tm_mday << " " <<
                std::setw(2) << utc->tm_hour << ":" <<
                std::setw(2) << utc->tm_min << ":" <<
                std::setw(2) << utc->tm_sec << "\"" << "}";
    } else {
        os << "{\"decoded\":false, \"lastSeenAt\":" << p.last_updated_at() <<
            ", \"nrSamples\":" << p.get_measure_count() <<
            ", \"centerFreqDelta\":" << p.get_center_freq_delta() << "}";
    }

    return os;
}

pass::ptr_t pass::make_pass(double const center_freq_hz_delta,
                            freq_t const samples_per_interval,
                            freq_t const sampling_rate,
                            size_t const interval_len)
{
    return std::make_unique<pass>(center_freq_hz_delta,
                                  samples_per_interval,
                                  sampling_rate,
                                  interval_len);
}


/// Create a new state tracking object for a pass.
/// \param center_freq_hz_delta The center frequency, in hertz, to baseband
/// \param samples_per_interval The number of samples in a 550 uS interval
/// \param
pass::pass(double const center_freq_hz_delta,
           freq_t const samples_per_interval,
           freq_t const sampling_rate,
           size_t const interval_len) : m_center_freq_hz(center_freq_hz_delta),
                                        m_raw_data(),
                                        m_baseband_shift(sample_vector_t(samples_per_interval, 0.0)),
                                        m_accumulated(sample_vector_t(samples_per_interval, 0.0)),
                                        m_samples_per_interval(samples_per_interval),
                                        m_sampling_rate(sampling_rate),
                                        m_nr_acc(0),
                                        m_last_at(0),
                                        m_interval_len(interval_len),
                                        m_slice_win(m_window_size)
{
    m_samples_per_bit = m_sampling_rate/500000;
    // Pre-calculate the vector to shift to baseband
    static double const time_delta = priv::us_to_sec(interval_len)/(m_samples_per_interval - 1);
    for (size_t i = 0; i < m_samples_per_interval; i++) {
       m_baseband_shift[i] = std::exp(sample_t(0.0, -2.0 * M_PI * m_center_freq_hz * double(i) * time_delta));
    }
}

pass::~pass()
{
}

size_t pass::find_transition(int& bit) const
{
    auto last = m_slice_win[0];

    for (size_t i = 1; i < m_slice_win.size(); i++) {
        auto v = m_slice_win[i];
        if (last != v) {
            bit = last > v ? 1 : 0;
            return i;
        }
        last = v;
    }

    bit = -1;
    return 0;
}

void pass::set_bit(size_t const bit_num, int const bit_value)
{
    m_raw_data[bit_num] = !!bit_value;
}

uint64_t pass::get_field(size_t const start, size_t const length) const
{
    uint64_t v = 0;

    for (size_t i = 0; i < length; i++) {
        v <<= 1;
        v |= m_raw_data[i + start];
    }

    return v;
}

std::uint16_t pass::calc_crc() const
{
    boost::crc_optimal<16, 0x1021, 0, 0, false, false> crc;

    for (size_t i = 0; i < 256/8; i++) {
        uint8_t v = 0;
        for (size_t j = 0; j < 8; j++) {
            v = (v << 1) | m_raw_data[i * 8 + j];
        }
        crc(v);
    }

    return crc();
}

/// Attempt to decode this pass. If successful, returns true.
bool pass::decode()
{
    std::vector<int> norm(m_accumulated.size());

    double average = std::accumulate(m_accumulated.begin(), m_accumulated.end(), 0.0,
            [](double a, sample_t b) { return a + std::abs(b); })/double(m_accumulated.size());
    std::transform(m_accumulated.begin(), m_accumulated.end(), norm.begin(),
            [average](sample_t v) { double val = std::abs(v) - average; return val > 0.0 ? 1 : -1; });

#ifdef _DUMP_RUNS
    int cur_run = 0,
        cur_sym = 0,
        nr_runs = 0;

    std::cout << "Writing out " << norm.size() << " symbols worth of runs: ";

    for (auto i: norm) {
        if (0 == cur_sym) {
            cur_sym = i;
            cur_run = 1;
            continue;
        }
        if (cur_sym != i) {
            std::cout << " " << cur_sym * cur_run;
            cur_sym = i;
            cur_run = 1;
            nr_runs++;
        } else {
            cur_run++;
        }
    }
    std::cout << " " << cur_sym * cur_run << std::endl;
    std::cout << "There were " << nr_runs << " runs." << std::endl;
#endif
    m_slice_win.clear();

    size_t sample_id = 0,
           bit_id = 0,
           skip = 0;

    bool found_start = false;

#if defined(_DEBUG_MFM_DECODE)
    std::cout << "Processing " << norm.size() << " samples." << std::endl;
#endif // defined(_DEBUG_MFM_DECODE)

    while ((sample_id++) < norm.size() && bit_id < 256) {
        int bit = -1;
        size_t offset = 0;

        m_slice_win.push_back(norm[sample_id]);

        if (m_slice_win.size() < m_window_size) {
            // Make sure the window is full
            continue;
        }

        if ((skip--) == 0) {
            offset = find_transition(bit);
            if (not found_start) {
                if (offset == m_window_size/2 and bit == 1) {
                    found_start = true;
                    set_bit(bit_id++, bit);
                    skip = m_samples_per_bit - 1;

#if defined(_DEBUG_MFM_DECODE)
                    std::cerr << "F -> " << sample_id << " S=" << skip << " W=[";
                    for (auto i: m_slice_win) {
                        std::cerr << i << ", ";
                    }
                    std::cerr << "] b=" << bit_id - 1 << std::endl;
                    if (bit == -1) {
                        std::cerr << "Invalid field, at offset " << sample_id << " terminating decode attempt" << std::endl;
                        break;
                    }
#endif // defined(_DEBUG_MFM_DECODE)
                } else {
                    skip = 0;
                }
            } else {
                skip = m_samples_per_bit - ((m_window_size/2) - offset) - 1;
#if defined(_DEBUG_MFM_DECODE)
                std::cerr << "T -> " << sample_id << " S=" << skip << " W=[";
                for (auto i: m_slice_win) {
                    std::cerr << i << ", ";
                }
                std::cerr << "] b=" << bit_id << std::endl;
                if (bit == -1) {
                    std::cerr << "Invalid field, at offset " << sample_id << " terminating decode attempt" << std::endl;
                    break;
                }
#endif // defined(_DEBUG_MFM_DECODE)
                set_bit(bit_id++, bit);
            }
        }
    }

    if (256 == bit_id) {
        m_header = get_field(0, 3);
        m_tag_type = get_field(3, 3);
        m_app_id = get_field(6, 3);
        m_group_id = get_field(9, 7);
        m_agency_id = get_field(16, 7);
        m_serial_num = get_field(23, 24);

#if defined(_DUMP_RAW_TAG)
        unsigned tx_crc = get_field(256-16-1, 16);
        uint16_t crc_calc = calc_crc();
        std::cout << "Tag: " << m_header << " type=" << m_tag_type << " app=" << m_app_id
            << " group=" << m_group_id << " agency=" << m_agency_id << " serial=" << std::hex
            << m_serial_num << " crc_tx=" << tx_crc << " crc_calc=" << crc_calc
            << std::dec << std::endl;
#endif // defined(_DUMP_RAW_TAG)
        m_decoded = calc_crc() == 0;
        if (m_decoded) {
            std::cout << *this << std::endl;
        }
    }

    return m_decoded;
}

/// Accumulate the given sample vector of length m_samples_per_interval. This
/// shifts the sample vector down to baseband, then simply adds it to the sample
/// vector, after normalizing for estimated transmit phase.
///
/// \param sig The signal - this is checked to be the right length
/// \param est_phase The estimated phase (the peak from the FFT)
/// \param at The time, in nanoseconds since the epoch, that this occurred.
/// 
void pass::accumulate(sample_t const* const sig, sample_t const est_phase, wallclock_t const at)
{
    // No need to accumulate if we've already successfully decoded
    if (m_decoded) {
        return;
    }

    // Normalize by phase, then shift the signal to baseband, and accumulate
    // the measured signals, such that the signal at baseband accumulates
    // coherently.
    for (size_t i = 0; i < m_accumulated.size(); i++) {
        m_accumulated[i] += (sig[i]/est_phase) * m_baseband_shift[i];
    }

    m_nr_acc++;
    m_last_at = at;
}

void pass::dump_to_file(std::shared_ptr<std::ofstream> ofs) const
{
    std::vector<std::complex<float>> cfv(m_accumulated.size());
    std::copy(m_accumulated.begin(), m_accumulated.end(), cfv.begin());
    ofs->write(reinterpret_cast<char const*>(&cfv.at(0)), sizeof(std::complex<float>) * m_accumulated.size());
}

