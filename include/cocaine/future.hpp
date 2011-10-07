#ifndef COCAINE_FUTURE_HPP
#define COCAINE_FUTURE_HPP

#include "cocaine/common.hpp"
#include "cocaine/response.hpp"

namespace cocaine { namespace core {

class future_t:
    public boost::noncopyable,
    public helpers::birth_control_t<future_t>
{
    public:
        inline void bind(const std::string& key, boost::shared_ptr<response_t> response) {
            m_key = key;
            m_response = response;
        }

        inline void move(boost::shared_ptr<future_t> future) {
            std::swap(m_key, future->m_key);
            std::swap(m_response, future->m_response);
        }

        inline void push(const Json::Value& value) {
            if(m_response) {
                m_response->push(m_key, value);
            }
        }

    private:
        std::string m_key;
        boost::shared_ptr<response_t> m_response;
};

}}

#endif
