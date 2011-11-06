#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

using namespace cocaine::engine;

deferred_t::deferred_t(driver_t* parent):
    m_parent(parent)
{ }

void deferred_t::enqueue() {
    m_parent->engine()->enqueue(
        shared_from_this(),
        boost::make_tuple(
            INVOKE,
            m_parent->method()
        )
    );
}

void deferred_t::audit(ev::tstamp spent) {
    m_parent->audit(spent);
}

publication_t::publication_t(driver_t* parent):
    deferred_t(parent)
{ }

void publication_t::respond(zmq::message_t& chunk) {
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(reader.parse(
        static_cast<const char*>(chunk.data()),
        static_cast<const char*>(chunk.data()) + chunk.size(),
        root))
    {
        m_parent->engine()->publish(m_parent->method(), root);
    } else {
        m_parent->engine()->publish(m_parent->method(),
            helpers::make_json("error", "unable to parse the json"));
    }
}

void publication_t::abort(error_code code, const std::string& error) {
    m_parent->engine()->publish(m_parent->method(),
        helpers::make_json("error", error));
}
