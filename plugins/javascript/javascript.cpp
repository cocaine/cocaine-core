#include <stdexcept>
#include <fstream>

#include <v8.h>

#include "plugin.hpp"
#include "uri.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error

namespace yappi { namespace plugin {

class javascript_t: public source_t {
    public:
        javascript_t(const std::string& uri_):
            source_t(uri_)
        {
            // Parse the URI
            helpers::uri_t uri(uri_);
            
            // Get the callable name
            std::vector<std::string> target = uri.path();

            // Join the path components
            std::string path("/usr/lib/yappi/javascript.d");
            std::vector<std::string>::const_iterator it = target.begin();
               
            do {
                path += ('/' + *it);
                ++it;
            } while(it != target.end());
            
            // Get the code
            std::stringstream code;
            
            std::ifstream input(path.c_str());
            
            if(!input.is_open()) {
                throw std::runtime_error("cannot open " + path);
            }
            
            // Read the code
            code << input.rdbuf();

            // Compile the code
            v8::Context::Scope scope(m_context);
            v8::Handle<v8::String> source = v8::String::New(code.str().c_str());
            m_script = v8::Script::Compile(source);
        }
    
        virtual dict_t fetch() {
            dict_t dict;

            v8::Context::Scope scope(m_context);
            v8::Handle<v8::Value> result = m_script->Run();

            v8::String::AsciiValue string(result);
            dict["result"] = *string;

            return dict;
        }

    private:
        v8::HandleScope m_handle_scope;
        v8::Persistent<v8::Context> m_context;
        v8::Handle<v8::Script> m_script;
};

source_t* create_javascript_instance(const char* uri) {
    return new javascript_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "javascript", &create_javascript_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        // Global initialization logic
        // This function will be called once, from the main thread

        return &plugin_info;
    }

    // __attribute__((destructor)) void finalize() {
        // This is guaranteed to be called from the main thread,
        // when there're no more plugin instances left running
    // }
}

}}
