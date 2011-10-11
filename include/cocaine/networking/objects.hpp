#ifndef COCAINE_RAW_OBJECTS_HPP
#define COCAINE_RAW_OBJECTS_HPP

namespace cocaine { namespace lines {

template<class T> class raw;

template<class T>
static raw<T> protect(T& object) {
    return raw<T>(object);
}

template<class T>
static raw<const T> protect(const T& object) {
    return raw<const T>(object);
}

template<> class raw<std::string> {
    public:
        raw(std::string& object):
            m_object(object)
        { }

        void pack(zmq::message_t& message) const {
            message.rebuild(m_object.length());
            memcpy(message.data(), m_object.data(), m_object.length());
        }

        bool unpack(/* const */ zmq::message_t& message) {
            m_object.assign(
                static_cast<const char*>(message.data()),
                message.size());
            return true;
        }

    private:
        std::string& m_object;
};

template<> class raw<const std::string> {
    public:
        raw(const std::string& object):
            m_object(object)
        { }

        void pack(zmq::message_t& message) const {
            message.rebuild(m_object.length());
            memcpy(message.data(), m_object.data(), m_object.length());
        }

    private:
        const std::string& m_object;
};

}}

#endif
