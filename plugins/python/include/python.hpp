#ifndef COCAINE_PYTHON_HPP
#define COCAINE_PYTHON_HPP

#include <Python.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/track.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
typedef helpers::track<PyObject*, Py_DecRef> object_t;

class python_t:
    public source_t
{
    public:
        // The source protocol implementation
        python_t(const std::string& uri);

        // Source protocol
        virtual uint32_t capabilities() const;
        virtual Json::Value iterate();
        virtual Json::Value process(const void* data, size_t data_size);
        virtual float reschedule();

    private:
        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code,
                     const std::string& name, 
                     const std::map<std::string, std::string>& parameters);

        // Fetches and formats current Python exception as a string
        std::string exception() const;

        // Unwraps the Python result object
        Json::Value unwrap(object_t& object) const;

    public:
        static char identity[];

    private:
        object_t m_module, m_object;
};

}}

#endif
