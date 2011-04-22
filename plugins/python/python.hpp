#ifndef YAPPI_PYTHON_HPP
#define YAPPI_PYTHON_HPP

#include <Python.h>

#include "plugin.hpp"
#include "track.hpp"

namespace yappi { namespace plugin {

// Format: python://[hostname]/path/to/file.py/callable?arg1=val1&arg2=...
class python_t: public source_t {
    public:
        // The source protocol implementation
        python_t(const std::string& uri);
        virtual dict_t fetch();

        // Instantiates the iterable object from the supplied code
        void create(const std::string& code,
                    const std::string& name, 
                    const dict_t& parameters);

        // Fetches and formats current Python exception as a string
        std::string exception();

    protected:
        static char identity[];

        typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef helpers::track<PyObject*, Py_DecRef> object_t;
        
        object_t m_module, m_object;
};

}}

#endif
