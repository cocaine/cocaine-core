#include "cocaine/drivers/abstract.hpp"
#include "cocaine/job.hpp"

using namespace cocaine::engine;

job_policy job_policy::defaults() {
    job_policy policy;

    policy.urgent = false;
    policy.timeout = config_t::get().engine.heartbeat_timeout;
    policy.deadline = 0.0;

    return policy;
}

job_t::job_t(driver_t* parent, job_policy policy):
    m_parent(parent),
    m_policy(policy)
{
    if(m_policy.deadline) {
        m_expiration_timer.set<job_t, &job_t::expire>(this);
        m_expiration_timer.start(m_policy.deadline);
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

void job_t::expire(ev::periodic&, int) {
    m_parent->expire(shared_from_this());
}

publication_t::publication_t(driver_t* parent, job_policy policy):
    job_t(parent, policy)
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
