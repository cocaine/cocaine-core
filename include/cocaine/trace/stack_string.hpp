/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_TRACE_STACK_STRING_HPP
#define COCAINE_TRACE_STACK_STRING_HPP

#include <cstring>
#include <string>

namespace cocaine {

template<size_t N>
struct stack_string {
    static constexpr size_t max_size = N;
    char blob[N+1];
    size_t size;

    stack_string(const char* lit) {
        size = std::min(N, strlen(lit));
        memcpy(blob, lit, size);
        blob[size] = '\0';
    }

    stack_string(const char* lit, size_t sz) {
        size = std::min(N, sz);
        memcpy(blob, lit, size);
        blob[size] = '\0';
    }

    stack_string(const std::string& source) {
        size = std::min(N, source.size());
        memcpy(blob, source.c_str(), size);
        blob[size] = '\0';
    }

    stack_string() :
        blob(),
        size(0)
    {}

    void reset() {
        size = 0;
    }
};

} // namespace cocaine

#endif // COCAINE_TRACE_STACK_STRING_HPP
