#include "cocaine/drivers/abstract.hpp"
#include "cocaine/job.hpp"

using namespace cocaine::engine;

job_t::job_t(driver_t* parent, ev::tstamp deadline, bool urgent):
    m_parent(parent),
    m_urgent(urgent)
{
    if(deadline) {
        m_expiration_timer.set<job_t, &job_t::timeout>(this);
        m_expiration_timer.start(deadline);
    }
}

void job_t::enqueue() {
    job_state state = m_parent->engine()->enqueue(
        shared_from_this(),
        boost::make_tuple(
            INVOKE,
            m_parent->method()
        )
    );
    
    if(m_expiration_timer.is_active() && state == running) {
        m_expiration_timer.stop();
    }
}

void job_t::audit(ev::tstamp spent) {
    m_parent->audit(spent);
}

void job_t::timeout(ev::periodic&, int) {
    m_parent->expire(shared_from_this());
}

publication_t::publication_t(driver_t* parent):
    job_t(parent)
{ }

void publication_t::send(zmq::message_t& chunk) {
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

void publication_t::send(error_code code, const std::string& error) {
    m_parent->engine()->publish(m_parent->method(),
        helpers::make_json("error", error));
}
