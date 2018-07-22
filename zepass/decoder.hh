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
// ZEPASSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ZEPASSD.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <zepass/types.hh>
#include <zepass/pass.hh>

#include <complex>
#include <fstream>
#include <memory>
#include <map>
#include <vector>

#include <fftw3.h>

namespace zepass {

class pass;

class decoder {
public:
    decoder(freq_t const centre_freq, freq_t const sampling_rate, size_t const interval_len,
            wallclock_t const max_age, std::shared_ptr<std::ofstream> out_file);
    ~decoder();

    void process_data(wallclock_t const at);
    size_t get_required_input_samples() const;
    sample_t *get_sample_buffer() { return m_in_vec; }
    size_t get_fft_len() const { return m_fft_len; }

private:
    void find_passes(wallclock_t const at);
    void reap_passes(wallclock_t const at);
    void process_peak(double peak_freq, freq_t peak_bin, sample_t const peak, wallclock_t const at);

    std::map<freq_t, zepass::pass::ptr_t> m_passes; //< std::map of passes, by bin index
    sample_t *m_freq_vec; //< Memory to contain FFT of input signal
    sample_t *m_in_vec; //< Input sample vector, populated by the application
    freq_t m_centre_freq; //< The centre frequency of all sampling
    freq_t m_sampling_rate; //< The sampling rate, in Hz, of the signal
    size_t m_fft_len; //< The length of the FFT output, in bins
    size_t m_samp_t_len; //< The length, in samples, of the chirp.
    fftw_plan m_plan;
    wallclock_t m_interval_len; //< Length of the capture interval, in microseconds
    wallclock_t m_max_age; //< Maximum age of a pass, if decoded or failed to decode
    std::shared_ptr<std::ofstream> m_out_file; //< File to write records to, one per line
};

} // end namespace zepass

