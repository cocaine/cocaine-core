#ifndef YAPPI_PYTHON_HPP
#define YAPPI_PYTHON_HPP

#include <Python.h>

#include "plugin.hpp"
#include "track.hpp"
// #include "storage.hpp"

namespace yappi { namespace plugin {

typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
typedef helpers::track<PyObject*, Py_DecRef> object_t;

/*
class storage_object_t {
    public:
        bool set(const std::string& key, const object& value) {
            return true;
        }

        object get(const std::string& key) {
            // Json::Value store = storage::storage_t::instance()->get("store");
            // return convert(store[key]);
            return object();
        }

    private:
        object convert(const Json::Value& value) {
            if(value.isBool()) {
                return object(value.asBool());
            } else if(value.isIntegral()) {
                return object(value.asInt());
            } else if(value.isDouble()) {
                return object(value.asDouble());
            } else if(value.isArray()) {
                list results;

                for(Json::Value::const_iterator it = value.begin(); it != value.end(); ++it) {
                    results.append(convert(*it));
                }

                return results;
            } else if(value.isObject()) {
                dict results;
                Json::Value::Members names = value.getMemberNames();

                for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
                   results[it] = convert(value[*it]);
                }

                return results;
            } else if(value.isString()) {
                return object(value.asString());
            } else {
                return object();
            }
        }
};

BOOST_PYTHON_MODULE(_context) {
    class_<storage_object_t>("Storage", "Yappi per-task storage")
        .def("set", &storage_object_t::set,
            "Sets a value for a given key",
            args("self", "key", "value"))
        .def("get", &storage_object_t::get,
            "Fetches a value for a given key",
            args("self", "key"));
}
*/

class python_t:
    public source_t
{
    public:
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
