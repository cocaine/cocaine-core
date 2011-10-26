#ifndef COCAINE_PYTHON_HPP
#define COCAINE_PYTHON_HPP

#include "cocaine/plugin.hpp"

#include "common.hpp"

namespace cocaine { namespace plugin {

class python_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args);

    public:
        python_t(const std::string& args);

        // Source protocol
        virtual void invoke(
            callback_fn_t callback,
            const std::string& method, 
            const void* request = NULL,
            size_t request_size = 0);

    private:
        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code);

        // Tries to convert the Python object to something sendable
        bool push(callback_fn_t callback, object_t& result);

    public:
        static char identity[];

    private:
        object_t m_module;
};

}}

#endif
