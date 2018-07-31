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

#include <zepass/decoder.hh>
#include <zepass/priv.hh>

#include <usrp/usrp.hh>

#include <boost/program_options.hpp>

#include <complex>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

#include <csignal>
#include <cstdint>
#include <cstdlib>

namespace po = boost::program_options;
namespace z = zepass;

static volatile bool running = true;

void handle_sigint(int)
{
    running = false;
}

int main(int const argc, char const* const argv[])
{
    po::options_description desc("Options"),
                            hidden("Hidden");

    size_t sample_rate = 3000000ull;
    size_t interval_len = 580;

    std::cerr << "ZEPASSD: The E-Z Pass Reader Daemon" << std::endl;
    std::cerr << "Copyright (C)2018 Phil Vachon <phil@security-embedded.com>" << std::endl;
    std::cerr << "Licensed under the GPLv3 or later. See COPYING for more details.\n\n" << std::endl;

    desc.add_options()
        ("help,h", "Get some help (this screen)")
        ("device,d", po::value<std::string>()->default_value(""), "USRP device ID to use")
        ("center,c", po::value<std::uint64_t>()->default_value(915750000), "Center frequency")
        ("tx-gain,T", po::value<double>()->default_value(75.0), "Transmit gain")
        ("tx-port,t", po::value<std::string>()->default_value("A:A"), "Transmit port on USRP")
        ("tx-ant,A", po::value<std::string>()->default_value("TX/RX"), "Transmit antenna on specified USRP TX port")
        ("rx-gain,R", po::value<double>()->default_value(75.0), "Receive gain")
        ("rx-port,r", po::value<std::string>()->default_value("A:A"), "Receive port on USRP")
        ("rx-ant,a", po::value<std::string>()->default_value("RX2"), "Receive antenna on the specified USRP RX port")
        ("pulse-len,P", po::value<std::uint64_t>()->default_value(20), "Length of activation pulse, in microseconds")
        ("gps-pps", "Use the GPS PPS source and synchronize local time")
        ("pulse-spacing,p", po::value<std::uint64_t>()->default_value(25), "Pulse interval, in milliseconds")
        ("max-age,m", po::value<std::uint64_t>()->default_value(30), "Maximum stale pass age, in seconds")
        ;

    hidden.add_options()
        ("output-file", po::value<std::string>(), "Output file");

    po::positional_options_description popt;
    popt.add("output-file", -1);

    po::options_description all_desc;
    all_desc.add(desc).add(hidden);

    po::variables_map args;
    po::store(po::command_line_parser(argc, argv)
            .options(all_desc)
            .positional(popt)
            .run(), args);
    po::notify(args);

    if (args.count("help")) {
        std::cout << "Usage: " << argv[0] << " {options} [output filename]" << std::endl;
        std::cout << desc << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (args.count("output-file") != 1) {
        std::cerr << "Missing output filename, aborting." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::string output_file = args["output-file"].as<std::string>();

    z::freq_t center_freq = args["center"].as<std::uint64_t>();

    // Get USRP parameters
    std::string device = args["device"].as<std::string>();
    std::string tx_port = args["tx-port"].as<std::string>();
    std::string rx_port = args["rx-port"].as<std::string>();
    std::string tx_ant = args["tx-ant"].as<std::string>();
    std::string rx_ant = args["rx-ant"].as<std::string>();
    double tx_gain = args["tx-gain"].as<double>();
    double rx_gain = args["rx-gain"].as<double>();
    size_t activation_len = args["pulse-len"].as<size_t>();
    size_t spacing = args["pulse-spacing"].as<size_t>() * 1000;
    bool gps_pps = !!args.count("gps-pps");
    size_t max_age = args["max-age"].as<size_t>() * 1000 * 1000;

    auto out_file = std::make_shared<std::ofstream>(output_file, std::ofstream::app);

    if (!out_file->is_open()) {
        std::cerr << "Failed to open output file " << output_file << ", aborting." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::cout << "Writing to output file [" << output_file << "]" << std::endl;
    std::cout << "Activation pulse length: " << activation_len << " microseconds. Spacing: " << spacing << " microseconds"
        << std::endl;
    std::cout << "Maximum pass age: " << max_age << " microseconds." << std::endl;
    std::cout << "Center frequency: " << std::fixed << double(center_freq)/1e6 << "MHz" << std::endl;
    std::cout << "RX Port: " << rx_port << " antenna: " << rx_ant << " gain: " << std::fixed << rx_gain << "dB" << std::endl;
    std::cout << "TX Port: " << tx_port << " antenna: " << tx_ant << " gain: " << std::fixed << tx_gain << "dB" << std::endl;

    std::unique_ptr<z::decoder> decoder = std::make_unique<z::decoder>(center_freq,
            sample_rate, interval_len, max_age, out_file);
    std::unique_ptr<usrp::usrp_controller> radio = std::make_unique<usrp::usrp_controller>(device,
            center_freq, tx_port, rx_port, tx_ant, rx_ant, sample_rate, sample_rate,
            tx_gain, rx_gain, interval_len, activation_len, gps_pps);
    z::sample_t* in_buf = decoder->get_sample_buffer();

    std::cout << "Letting the radio settle..." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::signal(SIGINT, &handle_sigint);

    std::cout << "Starting the trigger loop." << std::endl;

    z::wallclock_t wallclock = 0;
    do {
        wallclock = radio->arm_and_fire(in_buf, spacing);
        decoder->process_data(wallclock);
    } while (running);

    std::cout << "Shutting down at wallclock " << double(wallclock)/1e6 << std::endl;

    return EXIT_SUCCESS;
}

