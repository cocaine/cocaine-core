#include "cocaine/bootstrap.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/detail/actor.hpp"

using namespace cocaine;
using namespace cocaine::logging;

bootstrap_t::bootstrap_t(context_t& context):
    m_context(context),
    m_log(new log_t(context, "bootstrap"))
{
    COCAINE_LOG_INFO(
        m_log,
        "initializing %d %s",
        m_context.config.services.size(),
        m_context.config.services.size() == 1 ? "service" : "services"
    );

    auto it = m_context.config.services.begin(),
         end = m_context.config.services.end();

    for(; it != end; ++it) {
        auto reactor = std::unique_ptr<
            io::reactor_t
        >(new io::reactor_t());

        try {
            m_services.emplace_back(
                it->first,
                std::unique_ptr<actor_t>(new actor_t(
                    m_context.get<api::service_t>(
                        it->second.type,
                        m_context,
                        *reactor,
                        cocaine::format("service/%s", it->first),
                        it->second.args
                    ),
                    std::move(reactor),
                    it->second.args
                ))
            );
        } catch(const cocaine::error_t& e) {
            throw cocaine::error_t(
                "unable to initialize the '%s' service - %s",
                it->first,
                e.what()
            );
        }
    }

    for(service_list_t::iterator it = m_services.begin();
        it != m_services.end();
        ++it)
    {
        COCAINE_LOG_INFO(m_log, "starting the '%s' service", it->first);
        it->second->run();
    }
}

bootstrap_t::~bootstrap_t() {
    for(service_list_t::reverse_iterator it = m_services.rbegin();
        it != m_services.rend();
        ++it)
    {
        COCAINE_LOG_INFO(m_log, "stopping the '%s' service", it->first);
        it->second->terminate();
    }

    m_services.clear();
}
