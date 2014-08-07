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
#include "cocaine/asio/reactor.hpp"
#include "cocaine/memory.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

using namespace cocaine::io;

struct chamber_t::named_runnable {
    void
    operator()() const {
#if defined(__linux__)
        if(name.size() < 16) {
            ::prctl(PR_SET_NAME, name.c_str());
        } else {
            ::prctl(PR_SET_NAME, name.substr(0, 16).data());
        }
#endif

        reactor->run();
    }

    const std::string name;
    const std::shared_ptr<io::reactor_t>& reactor;
};

chamber_t::chamber_t(const std::string& name_, const std::shared_ptr<io::reactor_t>& reactor_):
    name(name_),
    reactor(reactor_)
{
    thread = std::make_unique<boost::thread>(named_runnable{name, reactor});
}

chamber_t::~chamber_t() {
    reactor->post(std::bind(&io::reactor_t::stop, reactor));

    thread->join();
    thread.reset();
}
