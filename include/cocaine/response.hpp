#ifndef COCAINE_RESPONSE_HPP
#define COCAINE_RESPONSE_HPP

#include <set>

#include "cocaine/common.hpp"

namespace cocaine { namespace core {

class response_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<response_t>,
    public helpers::birth_control_t<response_t>
{
    public:
        response_t(const std::vector<std::string>& route, boost::shared_ptr<core_t> parent):
            m_route(route),
            m_parent(parent)
        { }

    public:
        template<class T>
        void reserve(const T& list) {
            m_reserve.insert(list.begin(), list.end());
        }

        template<class T>
        void wait(const std::string& key, boost::shared_ptr<T> future) {
            future->bind(key, shared_from_this());
        }
        
        void push(const std::string& key, const Json::Value& result) {
            if(m_reserve.find(key) != m_reserve.end()) {
                m_root[key] = result;
                m_reserve.erase(key);
            }

            if(m_reserve.empty()) {
                m_parent->seal(shared_from_this());
            }
        }

        void abort(const std::string& error) {
            m_reserve.clear();
            m_root["error"] = error;
        }

        void abort(const std::string& key, const std::string& error) {
            Json::Value object;

            object["error"] = error;

            push(key, object);
        }

    public:
        std::string id() const {
            return m_id.get();
        }
        
        const Json::Value& root() const {
            return m_root;
        }

        const std::vector<std::string>& route() const {
            return m_route;
        }

    private:
        helpers::auto_uuid_t m_id;
        std::vector<std::string> m_route;
        boost::shared_ptr<core_t> m_parent;

        std::set<std::string> m_reserve;

        Json::Value m_root;
};

}}

#endif
