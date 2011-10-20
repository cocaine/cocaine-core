#ifndef COCAINE_FUTURE_HPP
#define COCAINE_FUTURE_HPP

#include "cocaine/common.hpp"
#include "cocaine/responses.hpp"

namespace cocaine { namespace lines {

class future_t:
    public boost::noncopyable,
    public helpers::birth_control_t<future_t>
{
    public:
        void bind(boost::shared_ptr<deferred_t> parent) {
            m_parent = parent;
        }

        void bind(const std::string& key, boost::shared_ptr<deferred_t> parent) {
            m_key = key;
            m_parent = parent;
        }

        void push(const Json::Value& value) {
            if(m_parent) {
                if(!m_key.empty()) {
                    m_parent->push(m_key, value);
                } else {
                    m_parent->push(value);
                }
            }
        }

    private:
        boost::shared_ptr<deferred_t> m_parent;
        std::string m_key;
};

}}

#endif
