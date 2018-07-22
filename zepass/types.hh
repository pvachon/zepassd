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

#include <complex>
#include <memory>
#include <vector>
#include <cstdint>

namespace zepass {

typedef std::complex<double> sample_t;
typedef std::vector<sample_t> sample_vector_t;
typedef std::shared_ptr<sample_vector_t> sample_vector_ptr_t;

typedef std::int64_t freq_t;
typedef std::int64_t freq_diff_t;
typedef std::int64_t sample_id_t;

typedef std::uint64_t wallclock_t;

} // end namespace zepass

