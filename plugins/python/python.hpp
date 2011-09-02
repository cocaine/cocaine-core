#ifndef YAPPI_PYTHON_HPP
#define YAPPI_PYTHON_HPP

#include <Python.h>

#include "plugin.hpp"
#include "track.hpp"

namespace yappi { namespace plugin {

class python_t:
    public source_t
{
    public:
        typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef helpers::track<PyObject*, Py_DecRef> object_t;
        
        // The source protocol implementation
        python_t(const std::string& uri);

        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code,
                     const std::string& name, 
                     const dict_t& parameters);

        // Source protocol
        virtual uint32_t capabilities() const;
        virtual dict_t invoke();
        virtual float reschedule();
        virtual dict_t process(const void* data, size_t data_size);

        // Fetches and formats current Python exception as a string
        std::string exception();

        // Unwraps the Python result object
        dict_t unwrap(object_t& object);

    public:
        static char identity[];

    private:
        object_t m_module, m_object;
};

}}

#endif
