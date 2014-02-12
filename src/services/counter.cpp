/*
    Copyright (c) 2013-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/services/counter.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::service;

namespace {

struct counter_machine_tag;

struct counter_machine {

    struct inc {
        typedef counter_machine_tag tag;

        typedef boost::mpl::list<
            int
        > tuple_type;

        typedef int result_type;
    };

    struct dec {
        typedef counter_machine_tag tag;

        typedef boost::mpl::list<
            int
        > tuple_type;

        typedef int result_type;
    };

    struct cas {
        typedef counter_machine_tag tag;

        typedef boost::mpl::list<
            int,
            int
        > tuple_type;

        typedef bool result_type;
    };

}; // struct counter_machine

} // namespace

namespace cocaine { namespace io {

template<>
struct protocol<counter_machine_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        counter_machine::inc,
        counter_machine::dec,
        counter_machine::cas
    > messages;

    typedef counter_machine type;
};

}} // namespace cocaine::io

struct counter_t::counter_machine_t {
    typedef counter_machine_tag tag;
    typedef int snapshot_type;

    counter_machine_t():
        m_value(0)
    { }

    counter_machine_t(counter_machine_t&& other) {
        *this = std::move(other);
    }

    counter_machine_t&
    operator=(counter_machine_t&& other) {
        m_value = other.m_value.load();
        return *this;
    }

    snapshot_type
    snapshot() const {
        return m_value;
    }

    void
    consume(const snapshot_type& snapshot) {
        m_value = snapshot;
    }

    int
    operator()(const io::aux::frozen<counter_machine::inc>& req) {
        auto val = m_value.fetch_add(std::get<0>(req.tuple)) + std::get<0>(req.tuple);
        return val;
    }

    int
    operator()(const io::aux::frozen<counter_machine::dec>& req) {
        auto val = m_value.fetch_sub(std::get<0>(req.tuple)) - std::get<0>(req.tuple);
        return val;
    }

    bool
    operator()(const io::aux::frozen<counter_machine::cas>& req) {
        int expected, desired;
        std::tie(expected, desired) = req.tuple;
        auto res = m_value.compare_exchange_strong(expected, desired);

        return res;
    }

private:
    std::atomic<int> m_value;
};

counter_t::counter_t(context_t& context,
                     io::reactor_t& reactor,
                     const std::string& name,
                     const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    implements<io::counter_tag>(context, name),
    m_log(new logging::log_t(context, name))
{
    using namespace std::placeholders;

    on<io::counter::inc>(std::bind(&counter_t::on_inc, this, _1));
    on<io::counter::dec>(std::bind(&counter_t::on_dec, this, _1));
    on<io::counter::cas>(std::bind(&counter_t::on_cas, this, _1, _2));

    m_raft = context.raft->insert(name, counter_machine_t());

    COCAINE_LOG_INFO(m_log, "Start counter service with name %s.", name);
}

namespace {

template<class T>
void
deferred_producer(deferred<T> promise,
                  boost::variant<T, std::error_code> result)
{
    T *value = boost::get<T>(&result);

    if(value) {
        promise.write(*value);
    } else {
        promise.abort(boost::get<std::error_code>(result).value(),
                      boost::get<std::error_code>(result).message());
    }
}

} // namespace

deferred<int>
counter_t::on_inc(int value) {
    COCAINE_LOG_INFO(m_log, "Inc received %d.", value);
    deferred<int> promise;
    m_raft->call<counter_machine::inc>(
        std::bind(deferred_producer<int>, promise, std::placeholders::_1),
        value
    );
    return promise;
}

deferred<int>
counter_t::on_dec(int value) {
    COCAINE_LOG_INFO(m_log, "Dec received %d.", value);
    deferred<int> promise;
    m_raft->call<counter_machine::dec>(
        std::bind(deferred_producer<int>, promise, std::placeholders::_1),
        value
    );
    return promise;
}

deferred<bool>
counter_t::on_cas(int expected, int desired) {
    COCAINE_LOG_INFO(m_log, "CAS received %d %d.", expected, desired);
    deferred<bool> promise;
    m_raft->call<counter_machine::cas>(
        std::bind(deferred_producer<bool>, promise, std::placeholders::_1),
        expected,
        desired
    );
    return promise;
}
