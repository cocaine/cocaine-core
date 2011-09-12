#include <stdexcept>
#include <fstream>

#include <v8.h>

#include "plugin.hpp"

#include "helpers/uri.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error

namespace cocaine { namespace plugin {

using namespace v8;

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
            std::string path("/usr/lib/cocaine/javascript.d");
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

            // Compile
            compile(code.str(), "invoke");
        }

        void compile(const std::string& code,
                     const std::string& name)
        {
            HandleScope handle_scope;

            m_context = Context::New();

            Context::Scope context_scope(m_context);
            
            TryCatch try_catch;

            Handle<String> source = String::New(code.c_str());
            Handle<Script> script = Script::Compile(source);

            if(script.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw std::runtime_error(*exception);
            }

            Handle<Value> result = script->Run();

            if(result.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw std::runtime_error(*exception);
            }

            Handle<String> target = String::New(name.c_str());
            Handle<Value> object = m_context->Global()->Get(target);

            if(!object->IsFunction()) {
                throw std::runtime_error("target object is not a function");
            }

            Handle<Function> function = Handle<Function>::Cast(object);
            m_function = Persistent<Function>::New(function);
        }

        ~javascript_t() {
            m_function.Dispose();
            m_context.Dispose();
        }

        virtual uint32_t capabilities() const {
            return ITERATOR;
        }

        virtual dict_t invoke() {
            dict_t dict;

            HandleScope handle_scope;
            Context::Scope context_scope(m_context);
            
            TryCatch try_catch;
            Handle<Value> result = m_function->Call(m_context->Global(), 0, NULL);

            if(!result.IsEmpty()) {
                dict["result"] = "success";
            } else {
                String::AsciiValue exception(try_catch.Exception());
                dict["exception"] = *exception;
            }

            return dict;
        }

    private:
        Persistent<Context> m_context;
        Persistent<Function> m_function;
};

source_t* create_javascript_instance(const char* uri) {
    return new javascript_t(uri);
}

static const source_info_t plugin_info[] = {
    { "javascript", &create_javascript_instance },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        // Global initialization logic
        // This function will be called once, from the main thread

        return plugin_info;
    }

    // __attribute__((destructor)) void finalize() {
        // This is guaranteed to be called from the main thread,
        // when there're no more plugin instances left running
    // }
}

}}
