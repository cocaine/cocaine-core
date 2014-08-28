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

#include "cocaine/detail/service/node/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"
#include "cocaine/defaults.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/detail/service/node/engine.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/messages.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node/stream.hpp"

#include "cocaine/idl/node.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/literal.hpp"

#include <boost/asio/local/connect_pair.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace boost::asio;
using namespace boost::asio::local;

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

namespace fs = boost::filesystem;

namespace {

class streaming_service_t:
    public dispatch<event_traits<app::enqueue>::dispatch_type>
{
    const api::stream_ptr_t downstream;

public:
    streaming_service_t(const std::string& name, const api::stream_ptr_t& downstream_):
        dispatch<event_traits<app::enqueue>::dispatch_type>(name),
        downstream(downstream_)
    {
        typedef io::protocol<event_traits<app::enqueue>::dispatch_type>::scope protocol;

        using namespace std::placeholders;

        on<protocol::chunk>(std::bind(&streaming_service_t::write, this, _1));
        on<protocol::error>(std::bind(&streaming_service_t::error, this, _1, _2));
        on<protocol::choke>(std::bind(&streaming_service_t::close, this));
    }

private:
    void
    write(const std::string& chunk) {
        downstream->write(chunk.data(), chunk.size());
    }

    void
    error(int code, const std::string& reason) {
        downstream->error(code, reason);
        downstream->close();
    }

    void
    close() {
        downstream->close();
    }
};

class app_service_t:
    public dispatch<app_tag>
{
    app_t* impl;

private:
    struct enqueue_slot_t:
        public basic_slot<app::enqueue>
    {
        enqueue_slot_t(app_service_t* impl_):
            impl(impl_)
        { }

        typedef basic_slot<app::enqueue>::dispatch_type dispatch_type;
        typedef basic_slot<app::enqueue>::tuple_type tuple_type;
        typedef basic_slot<app::enqueue>::upstream_type upstream_type;

        virtual
        boost::optional<std::shared_ptr<const dispatch_type>>
        operator()(tuple_type&& args, upstream_type&& upstream) {
            return tuple::invoke(
                std::bind(&app_service_t::enqueue, impl, std::ref(upstream), std::placeholders::_1, std::placeholders::_2),
                std::move(args)
            );
        }

    private:
        app_service_t* impl;
    };

    struct engine_stream_adapter_t:
        public api::stream_t
    {
        engine_stream_adapter_t(enqueue_slot_t::upstream_type& upstream_):
            upstream(upstream_)
        { }

        typedef enqueue_slot_t::upstream_type::protocol protocol;

        virtual
        void
        write(const char* chunk, size_t size) {
            upstream.send<protocol::chunk>(literal_t { chunk, size });
        }

        virtual
        void
        error(int code, const std::string& reason) {
            upstream.send<protocol::error>(code, reason);
        }

        virtual
        void
        close() {
            upstream.send<protocol::choke>();
        }

    private:
        enqueue_slot_t::upstream_type upstream;
    };

    std::shared_ptr<const enqueue_slot_t::dispatch_type>
    enqueue(enqueue_slot_t::upstream_type& upstream, const std::string& event, const std::string& tag) {
        api::stream_ptr_t downstream;

        if(tag.empty()) {
            downstream = impl->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream));
        } else {
            downstream = impl->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream), tag);
        }

        return std::make_shared<const streaming_service_t>(name(), downstream);
    }

public:
    app_service_t(const std::string& name_, app_t* impl_):
        dispatch<app_tag>(name_),
        impl(impl_)
    {
        on<app::enqueue>(std::make_shared<enqueue_slot_t>(this));
        on<app::info>(std::bind(&app_t::info, impl));
    }
};

} // namespace

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(context.log(name)),
    m_manifest(new manifest_t(context, name)),
    m_profile(new profile_t(context, profile))
{
    auto isolate = m_context.get<api::isolate_t>(
        m_profile->isolate.type,
        m_context,
        m_manifest->name,
        m_profile->isolate.args
    );

    if(m_manifest->source() != cached<dynamic_t>::sources::cache) {
        isolate->spool();
    }

    auto lhs = std::make_unique<stream_protocol::socket>(m_asio),
         rhs = std::make_unique<stream_protocol::socket>(m_asio);

    // Create the engine control sockets.
    connect_pair(*lhs, *rhs);

    m_engine_control = std::make_unique<channel<stream_protocol>>(std::move(rhs));

    try {
        m_engine = std::make_shared<engine_t>(m_context, *m_manifest, *m_profile, std::move(lhs));
    } catch(const boost::system::system_error& e) {
        throw cocaine::error_t("unable to create engine - [%d] %s", e.code().value(), e.code().message());
    }
}

app_t::~app_t() {
    // Empty.
}

void
app_t::start() {
    COCAINE_LOG_INFO(m_log, "starting engine");

    // Start the engine thread.
    m_thread = std::make_unique<std::thread>(std::bind(&engine_t::run, m_engine));

    COCAINE_LOG_DEBUG(m_log, "starting invocation service");

    // Publish the app service.
    m_context.insert(m_manifest->name, std::make_unique<actor_t>(
        m_context,
        std::make_shared<io_service>(),
        std::make_unique<app_service_t>(m_manifest->name, this)
    ));
}

namespace {

class call_action_t {
    enum class phases { request, message };

    channel<stream_protocol>& channel;

    encoder_t::message_type request;
    decoder_t::message_type message;

public:
    call_action_t(io::channel<stream_protocol>& channel_, encoder_t::message_type&& request_):
        channel(channel_),
        request(std::move(request_))
    { }

    void
    operator()() {
        channel.writer->write(request,
            std::bind(&engine_action_t::finalize, this, std::placeholders::_1, phases::request)
        );

        channel.reader->read(message,
            std::bind(&engine_action_t::finalize, this, std::placeholders::_1, phases::message)
        );
    }

    template<class Event>
    auto
    response() const -> typename basic_slot<Event>::tuple_type {
        if(message.type() != event_traits<Event>::id) {
            throw cocaine::error_t("unexpected engine response - %d", message.type());
        }

        typename basic_slot<Event>::tuple_type tuple;

        // Unpacks the object into a tuple using the message typelist as opposed to using the plain
        // tuple type traits, in order to support parameter tags, like optional<T>.
        io::type_traits<typename io::event_traits<Event>::tuple_type>::unpack(message.args(), tuple);

        return tuple;
    }

private:
    void
    finalize(const boost::system::error_code& ec, phases phase) {
        if(ec) {
            throw cocaine::error_t("unable to access engine - [%d] %s", ec.value(), ec.message());
        }

        if(phase == phases::message) {
            channel.socket->get_io_service().stop();
        }
    }
};

} // namespace

void
app_t::pause() {
    COCAINE_LOG_INFO(m_log, "trying to stop engine");

    if(!m_manifest->local) {
        // Destroy the app service.
        m_context.remove(m_manifest->name);
    }

    auto action = std::make_shared<call_action_t>(
       *m_engine_control,
        encoded<control::terminate>(1)
    );

    // Start the terminate action.
    m_asio.dispatch(std::bind(&call_action_t::operator(), action));

    try {
        m_asio.run();
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to stop engine - %s", e.what());

        // NOTE: Eventually the process will crash because the engine's thread is still on.
        return;
    }

    m_thread->join();

    COCAINE_LOG_INFO(m_log, "engine is now stopped");
}

dynamic_t
app_t::info() const {
    dynamic_t info = dynamic_t::object_t();

    if(!m_thread) {
        info.as_object()["error"] = "engine is not active";
        return info;
    }

    auto action = std::make_shared<call_action_t>(
       *m_engine_control,
        encoded<control::report>(1)
    );

    boost::asio::deadline_timer timeout(m_asio, boost::posix_time::seconds(defaults::control_timeout));
    boost::system::error_code ec;

    // Start the info action.
    m_asio.dispatch(std::bind(&call_action_t::operator(), action));

    try {
        // Blocks until either the response cancels the timer or timeout happens.
        timeout.wait(ec);
    } catch(const cocaine::error_t& e) {
        info.as_object()["error"] = std::string(e.what());
        return info;
    }

    if(ec != boost::asio::error::operation_aborted) {
        // Timer has succesfully finished, means no response has arrived.
        info.as_object()["error"] = "engine is unresponsive";
    } else {
        std::tie(info) = action->response<control::info>();
    }

    info.as_object()["profile"] = m_profile->name;

    return info;
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    return m_engine->enqueue(event, upstream);
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag) {
    return m_engine->enqueue(event, upstream, tag);
}
