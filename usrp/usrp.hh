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

#include <memory>
#include <string>

namespace usrp {

class usrp_controller {
public:
    usrp_controller(std::string const& device_id,
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
    ~usrp_controller();

    zepass::wallclock_t arm_and_fire(zepass::sample_t* target_buffer, zepass::wallclock_t const delay);
private:
    struct usrp_controller_impl;
    std::unique_ptr<usrp_controller_impl> m_pimpl;
};

} // end namespace usrp

