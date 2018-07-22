# ZEPASSD: E-Z Pass Capture Agent

ZEPASSD is a software defined radio (SDR) agent for capturing nearby E-Z Pass
metadata.

## Dependencies

There are a number of third-party dependencies for ZEPASSD.
 * boost.program-options
 * boost.crc
 * boost.circular-buffer
 * libfftw3
 * libuhd 3.9.5 or later

## Building

Clone the repo, install the above dependencies, and simply run `make`.

## Usage

```
Usage: ./zepassd {options} [output filename]
Options:
  -h [ --help ]                    Get some help (this screen)
  -d [ --device ] arg              USRP device ID to use
  -c [ --center ] arg (=915750000) Center frequency
  -T [ --tx-gain ] arg (=75)       Transmit gain
  -t [ --tx-port ] arg (=A:A)      Transmit port on USRP
  -A [ --tx-ant ] arg (=TX/RX)     Transmit antenna on specified USRP TX port
  -R [ --rx-gain ] arg (=75)       Receive gain
  -r [ --rx-port ] arg (=A:A)      Receive port on USRP
  -a [ --rx-ant ] arg (=RX2)       Receive antenna on the specified USRP RX 
                                   port
  -P [ --pulse-len ] arg (=20)     Length of activation pulse, in microseconds
  --gps-pps                        Use the GPS PPS source and synchronize local
                                   time
  -p [ --pulse-spacing ] arg (=25) Pulse interval, in milliseconds
  -m [ --max-age ] arg (=30)       Maximum stale pass age, in seconds

```

For example:

```
./zepassd --device "serial=F00FC7C8" --tx-port "A:A" --rx-port "A:A" --tx-gain 87 --rx-gain 85 -p 20 foobar 
```

will start ZEPASSD on the specified USRP, on the given TX/RX ports, with the
specified gains, a 20ms interval between activations, and will write the
outputs to a file named `foobar`.


## Hardware Compatibility

ZEPASSD will work with most radios that support UHD (i.e. USRPs). It relies on
the event triggering in hardware that the USRP supports. There are other
radios (such as LimeSDR?) that could offer the same capabilities.

## License, Copyright, Author

ZEPASSD is licensed under the GPLv3 or later.

ZEPASSD is Copyright (c) 2018 Phil Vachon
You can reach him at <phil@security-embedded.com>

