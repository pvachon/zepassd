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

#include <zepass/decoder.hh>
#include <zepass/pass.hh>
#include <zepass/priv.hh>

#include <array>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <stdexcept>
#include <sstream>

#include <fftw3.h>

using namespace zepass;

decoder::decoder(freq_t const centre_freq,
                 freq_t const sampling_rate,
                 size_t const interval_len,
                 wallclock_t const max_age,
                 std::shared_ptr<std::ofstream> out_file) :
                                              m_freq_vec(NULL),
                                              m_in_vec(NULL),
                                              m_centre_freq(centre_freq),
                                              m_sampling_rate(sampling_rate),
                                              m_interval_len(interval_len),
                                              m_max_age(max_age),
                                              m_out_file(out_file)
{
    if (0 >= sampling_rate) {
        throw std::invalid_argument("sampling_rate");
    }

    if (centre_freq <= 0) {
        throw std::invalid_argument("center_freq");
    }

    m_samp_t_len = size_t(double(m_sampling_rate) * priv::us_to_sec(m_interval_len));
    m_fft_len = priv::round_nearest_power_2(m_samp_t_len);

    std::cout << "Interval samples: " << m_samp_t_len << " FFT Length: " << m_fft_len << std::endl;

    if (NULL == (m_freq_vec = reinterpret_cast<sample_t *>(fftw_malloc(sizeof(fftw_complex) * m_fft_len)))) {
        throw std::bad_alloc();
    }

    if (NULL == (m_in_vec = reinterpret_cast<sample_t *>(fftw_malloc(sizeof(fftw_complex) * m_fft_len)))) {
        throw std::bad_alloc();
    }

    std::cout << "Planning FFT..." << std::endl;

    m_plan = fftw_plan_dft_1d(int(m_fft_len), reinterpret_cast<fftw_complex *>(m_in_vec),
            reinterpret_cast<fftw_complex *>(m_freq_vec), FFTW_FORWARD, FFTW_MEASURE | FFTW_PRESERVE_INPUT);

    std::cout << "FFT planning is done, we are ready to roll." << std::endl;
}

decoder::~decoder()
{
    fftw_destroy_plan(m_plan);

    if (NULL != m_freq_vec) {
        fftw_free(m_freq_vec);
        m_freq_vec = NULL;
    }

    if (NULL != m_in_vec) {
        fftw_free(m_in_vec);
        m_in_vec = NULL;
    }
}

void decoder::process_peak(double peak_freq, freq_t peak_bin, sample_t const peak, wallclock_t const at)
{
    auto freq = m_passes.find(peak_bin);
    zepass::pass::ptr_t pass;

    if (freq == m_passes.end()) {
        // Create a new pass instance and insert it
        std::cout << "Found peak: " << peak_bin << " at dF " <<
            std::fixed << std::setw(8) << peak_freq <<  " (f=" << peak_freq + m_centre_freq << ")" << std::endl;

        pass = zepass::pass::make_pass(peak_freq, m_samp_t_len,
                m_sampling_rate, m_interval_len);
        m_passes.insert(std::make_pair(peak_bin, pass));
    } else {
        pass = freq->second;
    }

    pass->accumulate(m_in_vec, peak, at);
    if (pass->get_measure_count() > 32 and !pass->is_decoded()) {
        // If we have integrated 32 times and we haven't been able to decode, throw it all away.
        std::cout << "Unable to decode, erasing pass in case we're getting owned by noise." << std::endl;
        m_passes.erase(peak_bin);
    } else if (pass->get_measure_count() > 16 and !pass->is_decoded()) {
        if (pass->decode()) {
            (*m_out_file) << *pass << std::endl;
        }
    }
}

void decoder::reap_passes(wallclock_t const at)
{
    for (auto it = m_passes.cbegin(); it != m_passes.cend();) {
        auto pass = it->second;
        if (at - pass->last_updated_at() > m_max_age) {
            std::cout << "Reaping pass " << *pass << ", it's out of date" << std::endl;
            it = m_passes.erase(it);
        } else {
            ++it;
        }
    }
}

void decoder::find_passes(wallclock_t const at)
{
    std::array<double, 3> window = { 0.0, 0.0, 0.0 };

    // TODO: optimize me a bit, at least.
    for (size_t i = 1; i < m_fft_len - 1; ++i) {
        window[0] = std::abs(m_freq_vec[i - 1]);
        window[1] = std::abs(m_freq_vec[i]);
        window[2] = std::abs(m_freq_vec[i + 1]);

        if (window[1] > window[0] && window[1] > window[2] && window[1] > 500.0) {
            // the actual bin ID is rotated by half the length of the FFT
            freq_t bin_id = (i + (m_fft_len/2)) % m_fft_len;
            // Using the bin ID and the length of the FFT, calculate our offset, in Hz, from baseband
            double peak_freq = (double(bin_id) * double(m_sampling_rate)/double(m_fft_len)) - float(m_sampling_rate)/2.0;

            process_peak(peak_freq, bin_id, m_freq_vec[i], at);
        }
    }
}

/// Given a vector of samples of T=m_interval_len uS, extract various components and
/// process the signal. ``
/// \param sig A vector of samples, as double-precision integers.
void decoder::process_data(wallclock_t const at)
{
    // Calculate FFT for the data set
    fftw_execute(m_plan);

    // Find all candidate passes
    find_passes(at);

    // Reap any stale passes
    reap_passes(at);
}

