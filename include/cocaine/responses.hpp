#ifndef COCAINE_RESPONSE_HPP
#define COCAINE_RESPONSE_HPP

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace lines {

class deferred_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<deferred_t>
{
    public:
        inline void push(const std::string& key, const Json::Value& object) {
            if(key.empty()) {
                m_root = object;
            } else {
                m_root[key] = object;
            }
        }

        template<class T>
        inline void wait(boost::shared_ptr<T> object) {
            object->bind("", shared_from_this());
        }

        template<class T>
        inline void wait(const std::string& key, boost::shared_ptr<T> object) {
            object->bind(key, shared_from_this());
        }

        inline void abort(const std::string& error) {
            m_root.clear();
            m_root["error"] = error;
        }

        inline void abort(const std::string& key, const std::string& error) {
            Json::Value object;
            object["error"] = error;
            push(key, object);
        }
       
    protected:
        deferred_t() { /* allow only inherited objects to be instantiated */ }

    protected:
        Json::Value m_root;
};

class responder_t {
    public:
        virtual void respond(const route_t& route, const Json::Value& object) = 0;
};

class publisher_t {
    public:
        virtual void publish(const std::string& key, const Json::Value& object) = 0;
};


class response_t:
    public deferred_t,
    public helpers::birth_control_t<response_t>
{
    public:
        response_t(const lines::route_t& route, boost::shared_ptr<responder_t> parent):
            m_route(route),
            m_parent(parent)
        {
            m_root["hostname"] = config_t::get().core.hostname;
        }

        virtual ~response_t() {
            m_parent->respond(m_route, m_root);
        }

    private:
        const lines::route_t m_route;
        const boost::shared_ptr<responder_t> m_parent;
};

class publication_t:
    public deferred_t,
    public helpers::birth_control_t<publication_t>
{
    public:
        publication_t(const std::string& key, boost::shared_ptr<publisher_t> parent):
            m_key(key),
            m_parent(parent)
        { }

        virtual ~publication_t() {
            m_parent->publish(m_key, m_root);
        }

    private:
        const std::string m_key;
        const boost::shared_ptr<publisher_t> m_parent;
};

}}

#endif
