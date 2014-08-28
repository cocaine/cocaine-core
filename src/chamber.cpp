/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/chamber.hpp"
#include "cocaine/memory.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#elif defined(__APPLE__)
    #include <pthread.h>
#endif

using namespace cocaine::io;

class chamber_t::named_runnable_t {
    const std::string name;
    const std::shared_ptr<boost::asio::io_service>& asio;

public:
    named_runnable_t(const std::string& name_, const std::shared_ptr<boost::asio::io_service>& asio_):
        name(name_),
        asio(asio_)
    { }

    void
    operator()() const {
#if defined(__linux__)
        if(name.size() < 16) {
            ::prctl(PR_SET_NAME, name.c_str());
        } else {
            ::prctl(PR_SET_NAME, name.substr(0, 16).data());
        }
#elif defined(__APPLE__)
        pthread_setname_np(name.c_str());
#endif

        asio->run();
    }

};

chamber_t::chamber_t(const std::string& name_, const std::shared_ptr<boost::asio::io_service>& asio_):
    name(name_),
    asio(asio_),
    work(new boost::asio::io_service::work(*asio))
{
    thread = std::make_unique<boost::thread>(named_runnable_t(name, asio));
}

chamber_t::~chamber_t() {
    // NOTE: Instead of calling io_service::stop() to terminate the reactor immediately, kill the
    // work object and wait for all the outstanding operations to gracefully finish up.
    work = nullptr;

    // TODO: Check if this might hang forever because of the above.
    thread->join();
}
