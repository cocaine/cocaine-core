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

#include "cocaine/detail/service/node/engine.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node/stream.hpp"

#include "cocaine/idl/rpc.hpp"
#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/actor.hpp"
#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/literal.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <asio/local/stream_protocol.hpp>

using namespace asio;
using namespace asio::local;

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

        on<protocol::chunk>(std::bind(&streaming_service_t::write, this, ph::_1));
        on<protocol::error>(std::bind(&streaming_service_t::error, this, ph::_1, ph::_2));
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
    std::shared_ptr<app_t> parent;
    logging::log_t& log;

private:
    struct enqueue_slot_t:
        public basic_slot<app::enqueue>
    {
        enqueue_slot_t(app_service_t *const parent_):
            parent(parent_)
        { }

        typedef basic_slot<app::enqueue>::dispatch_type dispatch_type;
        typedef basic_slot<app::enqueue>::tuple_type tuple_type;
        typedef basic_slot<app::enqueue>::upstream_type upstream_type;

        virtual
        boost::optional<std::shared_ptr<const dispatch_type>>
        operator()(tuple_type&& args, upstream_type&& upstream) {
            return tuple::invoke(
                std::move(args),
                std::bind(&app_service_t::enqueue, parent, std::ref(upstream), ph::_1, ph::_2)
            );
        }

    private:
        app_service_t *const parent;
    };

    struct engine_stream_adapter_t:
        public api::stream_t
    {
        engine_stream_adapter_t(enqueue_slot_t::upstream_type& upstream_):
            upstream(upstream_)
        { }

        typedef io::protocol<event_traits<app::enqueue>::upstream_type>::scope protocol;

        virtual
        void
        write(const char* chunk, size_t size) {
            upstream = upstream.send<protocol::chunk>(literal_t { chunk, size });
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
            downstream = parent->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream));
        } else {
            downstream = parent->enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream), tag);
        }

        if(!downstream) {
            typedef io::protocol<event_traits<app::enqueue>::upstream_type>::scope protocol;
            try {
                upstream.send<protocol::error>(cocaine::error::dispatch_errors::service_error, "application was stopped");
            } catch (const error_t& e) {
                COCAINE_LOG_WARNING(&log, "unable to send application stop event to client: %s", e.what());
            }
            return nullptr;
        }

        return std::make_shared<const streaming_service_t>(name(), downstream);
    }

public:
    app_service_t(const std::string& name_, std::shared_ptr<app_t> parent_, logging::log_t& log_):
        dispatch<app_tag>(name_),
        parent(std::move(parent_)),
        log(log_)
    {
        on<app::enqueue>(std::make_shared<enqueue_slot_t>(this));
        on<app::info>(std::bind(&app_t::info, parent));
    }
};

} // namespace

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(context.log(name)),
    m_manifest(new manifest_t(context, name)),
    m_profile(new profile_t(context, profile)),
    m_asio(std::make_shared<asio::io_service>())
{
    auto isolate = m_context.get<api::isolate_t>(
        m_profile->isolate.type,
        m_context,
        m_manifest->name,
        m_profile->isolate.args
    );

    // TODO: Spooling state?
    if(m_manifest->source() != cached<dynamic_t>::sources::cache) {
        isolate->spool();
    }
}

app_t::~app_t() {
    // Empty.
}

void
app_t::start() {
    COCAINE_LOG_DEBUG(m_log, "creating engine '%s'", m_manifest->name);

    // Start the engine thread.
    try {
        m_engine = std::make_shared<engine_t>(m_context, *m_manifest, *m_profile);
    } catch(const std::exception& err) {
#if defined(HAVE_GCC48)
        std::throw_with_nested(cocaine::error_t("unable to create engine"));
#else
        COCAINE_LOG_ERROR(m_log, "unable to initialize engine: %s", err.what());

        throw cocaine::error_t("unable to create engine");
#endif
    }

    COCAINE_LOG_DEBUG(m_log, "starting invocation service");

    // Publish the app service.
    m_context.insert(m_manifest->name, std::make_unique<actor_t>(
        m_context,
        m_asio,
        std::make_unique<app_service_t>(m_manifest->name, shared_from_this(), *m_log)
    ));
}

void
app_t::pause() {
    COCAINE_LOG_DEBUG(m_log, "stopping app '%s'", m_manifest->name);

    m_context.remove(m_manifest->name);
    m_engine.reset();

    COCAINE_LOG_DEBUG(m_log, "app '%s' has been stopped", m_manifest->name);
}

cocaine::result_of<io::app::info>::type
app_t::info() const {
    typedef cocaine::result_of<io::app::info>::type result_type;

    COCAINE_LOG_DEBUG(m_log, "handling info request");

    auto engine = m_engine;
    if(!engine) {
        dynamic_t::object_t info;
        info["profile"] = m_profile->name;
        info["error"] = "engine is not active";
        return info;
    }

    return engine->info();
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    auto engine = m_engine;
    if(!engine) {
        return nullptr;
    }
    return engine->enqueue(event, upstream);
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag) {
    auto engine = m_engine;
    if(!engine) {
        return nullptr;
    }
    return engine->enqueue(event, upstream, tag);
}
