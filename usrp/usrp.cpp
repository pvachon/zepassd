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

#include <usrp/usrp.hh>
#include <zepass/types.hh>
#include <zepass/priv.hh>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/thread_priority.hpp>

#include <memory>
#include <string>
#include <iostream>
#include <iomanip>
#include <complex>

#include <cmath>

using namespace usrp;

namespace z = zepass;

struct usrp_controller::usrp_controller_impl {
    usrp_controller_impl(std::string const& device_id,
                         size_t const center_freq,
                         std::string const& tx_port_id,
                         std::string const& rx_port_id,
                         std::string const& tx_ant_id,
                         std::string const& rx_ant_id,
                         size_t const tx_rate,
                         size_t const rx_rate,
                         double const tx_gain,
                         double const rx_gain,
                         size_t const rx_len_us,
                         size_t const activation_len_us,
                         bool const use_pps);
    ~usrp_controller_impl();

    z::wallclock_t arm_and_fire(z::sample_t* target_buffer, z::wallclock_t const delay);
private:
    uhd::usrp::multi_usrp::sptr m_usrp;
    std::string m_device_id;
    size_t m_center_freq;
    std::string m_tx_port_id;
    std::string m_rx_port_id;
    std::string m_tx_ant_id;
    std::string m_rx_ant_id;
    size_t m_tx_rate;
    size_t m_rx_rate;
    double m_tx_gain;
    double m_rx_gain;
    size_t m_rx_len_us;
    size_t m_activation_len_us;
    size_t m_samples_per_interval;
    size_t m_tx_center_freq;
    size_t m_rx_center_freq;

    uhd::tx_streamer::sptr m_tx_stream;
    uhd::rx_streamer::sptr m_rx_stream;

    /// Output pulse to transmit, a sine wave shifted from baseband based on the center
    /// frequency we calculate
    std::vector<std::complex<float>> m_tx_buf;
    std::vector<std::complex<float>*> m_tx_buff;
    std::vector<std::complex<double>*> m_rx_buff;

    size_t m_pulse_samps;

    bool m_use_pps; //< Whether or not to use the GPS PPS signal port
};

usrp_controller::usrp_controller_impl::usrp_controller_impl(std::string const& device_id,
                                                            size_t const center_freq,
                                                            std::string const& tx_port_id,
                                                            std::string const& rx_port_id,
                                                            std::string const& tx_ant_id,
                                                            std::string const& rx_ant_id,
                                                            size_t const tx_rate,
                                                            size_t const rx_rate,
                                                            double const tx_gain,
                                                            double const rx_gain,
                                                            size_t const rx_len_us,
                                                            size_t const activation_len_us,
                                                            bool const use_pps)
    : m_device_id(device_id),
      m_center_freq(center_freq),
      m_tx_port_id(tx_port_id),
      m_rx_port_id(rx_port_id),
      m_tx_ant_id(tx_ant_id),
      m_rx_ant_id(rx_ant_id),
      m_tx_rate(tx_rate),
      m_rx_rate(rx_rate),
      m_tx_gain(tx_gain),
      m_rx_gain(rx_gain),
      m_rx_len_us(rx_len_us),
      m_activation_len_us(activation_len_us),
      m_use_pps(use_pps)
{
    uhd::set_thread_priority_safe();

    m_usrp = uhd::usrp::multi_usrp::make(m_device_id);
    m_usrp->set_tx_rate(m_tx_rate);
    std::cout << "Requested TX rate: " << std::fixed << double(tx_rate)/1e6 << "Msps got " <<
        double(m_usrp->get_tx_rate())/1e6 << "Msps" << std::endl;
    m_usrp->set_rx_rate(m_rx_rate);
    std::cout << "Requested RX rate: " << std::fixed << double(rx_rate)/1e6 << "Msps got " <<
        double(m_usrp->get_rx_rate())/1e6 << "Msps" << std::endl;

    m_samples_per_interval = size_t(double(rx_rate) * z::priv::us_to_sec(m_rx_len_us));
    std::cout << "Samples in " << rx_len_us << " microsecond interval: " << m_samples_per_interval << std::endl;

    // Tuning fudge. Because the USRP has spurs if we directly request the center frequency
    // for transmit, we will adjust by 200kHz of the transmit center frequency. This keeps the
    // spur well above our received signals of interest
    m_tx_center_freq = m_center_freq + 200000;
    m_rx_center_freq = m_center_freq;

    std::cout << "Tuning transmit front-end to " << std::fixed << double(m_tx_center_freq) << "MHz" << std::endl;

    // Set up the front end routing and state.
    uhd::tune_request_t tx_tune(m_tx_center_freq);
    m_usrp->set_tx_subdev_spec(m_tx_port_id, 0);
    m_usrp->set_tx_antenna(m_tx_ant_id, 0);
    m_usrp->set_tx_gain(tx_gain, 0);
    m_usrp->set_tx_freq(tx_tune, 0);

    uhd::tune_request_t rx_tune(m_rx_center_freq);
    m_usrp->set_rx_subdev_spec(m_rx_port_id, 0);
    m_usrp->set_rx_antenna(m_rx_ant_id, 0);
    m_usrp->set_rx_gain(rx_gain, 0);
    m_usrp->set_rx_freq(rx_tune, 0);

    std::cout << "TX Channel specs: " << std::endl;
    for (size_t i = 0; i < m_usrp->get_tx_num_channels(); i++) {
        std::cout << "    " << m_usrp->get_tx_subdev_name(i) << std::endl;
    }

    std::cout << "RX Channel specs: " << std::endl;
    for (size_t i = 0; i < m_usrp->get_rx_num_channels(); i++) {
        std::cout << "    " << m_usrp->get_rx_subdev_name(i) << std::endl;
    }

    // Set up the RX streamer
    uhd::stream_args_t rx_stream_args("fc64");
    rx_stream_args.channels = { 0 };

    m_rx_stream = m_usrp->get_rx_stream(rx_stream_args);

    // Set up the TX streamer
    uhd::stream_args_t tx_stream_args("fc32");
    tx_stream_args.channels = { 0 };

    m_tx_stream = m_usrp->get_tx_stream(tx_stream_args);

    // Create frequency shifted sinusoid for the trigger pulse
    m_pulse_samps = z::priv::us_to_sec(m_activation_len_us) * double(m_tx_rate);
    std::cout << "Pulse is " << m_pulse_samps << " samples long" << std::endl;

    if (m_tx_stream->get_max_num_samps() < m_pulse_samps) {
        throw std::range_error("pulse length is too long!");
    }

    double time_delta = z::priv::us_to_sec(m_activation_len_us)/(m_pulse_samps - 1);
    m_tx_buf.resize(m_pulse_samps, 0.0);
    for (size_t i = 0; i < m_pulse_samps; i++) {
        m_tx_buf[i] = std::complex<float>(0.9, 0.9) *
            std::exp(std::complex<float>(0.0, -2.0 * M_PI * double(200000) * double(i) * time_delta));
    }
    m_tx_buff.resize(1, &m_tx_buf.front());
    m_rx_buff.resize(1, NULL);

    if (m_use_pps) {
        std::cout << "Time sources: " << std::endl;
        for (auto &ts : m_usrp->get_time_sources(0)) {
            std::cout << "    " << ts << std::endl;
        }

        uhd::time_spec_t cur_time = m_usrp->get_time_now();
        std::cout << "Time is: " << cur_time.get_real_secs() << std::endl;
    }
}

usrp_controller::usrp_controller_impl::~usrp_controller_impl()
{
}

z::wallclock_t usrp_controller::usrp_controller_impl::arm_and_fire(z::sample_t* target_buffer, z::wallclock_t const pulse_delay)
{
    uhd::stream_cmd_t rx_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    uhd::rx_metadata_t rx_md;

    m_rx_buff[0] = target_buffer;

    // Get the commands queued up (with fuuuuudge)
    auto start_of_epoch = m_usrp->get_time_now();
    m_usrp->set_command_time(start_of_epoch + z::priv::us_to_sec(pulse_delay - 15000));

    // Arm an m_activation_len_us microsecond pulse to transmit
    uhd::tx_metadata_t tx_md;
    tx_md.start_of_burst = true;
    tx_md.end_of_burst = true;
    tx_md.has_time_spec = true;
    tx_md.time_spec = start_of_epoch + z::priv::us_to_sec(pulse_delay);

    rx_cmd.stream_now = false;
    rx_cmd.num_samps = m_samples_per_interval;
    rx_cmd.time_spec = start_of_epoch +
        z::priv::us_to_sec(pulse_delay) +
        z::priv::us_to_sec(m_activation_len_us) +
        z::priv::us_to_sec(105.0);

    rx_md.time_spec = rx_cmd.time_spec;
    rx_md.has_time_spec = true;

    m_rx_stream->issue_stream_cmd(rx_cmd);

    // Send the burst
    size_t sent = m_tx_stream->send(m_tx_buff, m_pulse_samps, tx_md, 1.0);
    if (sent < m_pulse_samps) {
        throw std::runtime_error("didn't transmit enough samples, aborting");
    }

    // Kick off the receive operation
    size_t received = m_rx_stream->recv(m_rx_buff, m_samples_per_interval, rx_md, 1.0);
    if (received < m_samples_per_interval) {
        std::cout << "Got " << received << " samples" << std::endl;
        std::cout << "Receive metadata was: " << rx_md.to_pp_string(false) << std::endl;
        throw std::runtime_error("didn't receive enough samples, aborting.");
    }

    if (rx_md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
        std::cout << "Transmit time was: " << rx_cmd.time_spec.get_real_secs() << std::endl;
        std::cout << "Receive metadata was: " << rx_md.to_pp_string(false) << std::endl;
        throw std::runtime_error(rx_md.strerror());
    }

    return z::wallclock_t(rx_md.time_spec.get_real_secs() * 1000000.0);
}

/// Construct a new USRP controller. This object precisely controls the dispatch
/// of the activation signal and the reception of the OOK message, to be fed into
/// the zepass decoder pieces.
usrp_controller::usrp_controller(std::string const& device_id,
                                 size_t const center_freq,
                                 std::string const& tx_port_id,
                                 std::string const& rx_port_id,
                                 std::string const& tx_ant_id,
                                 std::string const& rx_ant_id,
                                 size_t const tx_rate,
                                 size_t const rx_rate,
                                 double const tx_gain,
                                 double const rx_gain,
                                 size_t const rx_len_us,
                                 size_t const activation_len_us,
                                 bool const use_pps)
{
    m_pimpl = std::make_unique<usrp_controller::usrp_controller_impl>(device_id,
                                                                      center_freq,
                                                                      tx_port_id,
                                                                      rx_port_id,
                                                                      tx_ant_id,
                                                                      rx_ant_id,
                                                                      tx_rate,
                                                                      rx_rate,
                                                                      tx_gain,
                                                                      rx_gain,
                                                                      rx_len_us,
                                                                      activation_len_us,
                                                                      use_pps);
}

usrp_controller::~usrp_controller()
{
}

/// Arm the USRP to send the activation pulse, then receive signals data into the
/// target buffer.
z::wallclock_t usrp_controller::arm_and_fire(z::sample_t* target_buffer, z::wallclock_t const delay)
{
    return m_pimpl->arm_and_fire(target_buffer, delay);
}

