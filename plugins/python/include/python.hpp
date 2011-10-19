#ifndef COCAINE_PYTHON_HPP
#define COCAINE_PYTHON_HPP

#include "cocaine/plugin.hpp"

#include "common.hpp"

namespace cocaine { namespace plugin {

class python_t:
    public source_t
{
    public:
        // The source protocol implementation
        python_t(const std::string& name, const std::string& args);

        // Source protocol
        virtual Json::Value invoke(const std::string& callable, 
            const void* request = NULL, size_t request_length = 0);

    private:
        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code);

    public:
        static char identity[];

    private:
        object_t m_module;
};

}}

#endif
