//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <sstream>

#include <v8.h>

#include "cocaine/app.hpp"

#include "cocaine/interfaces/plugin.hpp"

namespace cocaine { namespace engine {

using namespace v8;

class javascript_t:
    public plugin_t
{
    public:
        javascript_t(context_t& context, const app_t& app):
            plugin_t(context, app)
        {
            Json::Value args(app.args());

            if(!args.isObject()) {
                throw configuration_error_t("malformed manifest");
            }
            
            boost::filesystem::path source(args["source"].asString());

            if(source.empty()) {
                throw configuration_error_t("no code location has been specified");
            }

            boost::filesystem::ifstream input(source);
    
            if(!input) {
                throw configuration_error_t("unable to open " + source.string());
            }

            std::stringstream stream;
            stream << input.rdbuf();
            
            compile(stream.str(), "iterate");
        }

        ~javascript_t() {
            m_function.Dispose();
            m_v8_context.Dispose();
        }

        virtual void invoke(const std::string& method,
                            io_t& io)
        {
            Json::Value result;

            HandleScope handle_scope;
            Context::Scope context_scope(m_v8_context);
            
            TryCatch try_catch;
            Handle<Value> rv(m_function->Call(m_v8_context->Global(), 0, NULL));

            if(!rv.IsEmpty()) {
                result["result"] = "success";
            } else if(try_catch.HasCaught()) {
                String::AsciiValue exception(try_catch.Exception());
                result["error"] = std::string(*exception, exception.length());
            }

            Json::FastWriter writer;
            std::string response(writer.write(result));

            io.write(response.data(), response.size());
        }

    private:
        void compile(const std::string& code,
                     const std::string& name)
        {
            HandleScope handle_scope;

            m_v8_context = Context::New();

            Context::Scope context_scope(m_v8_context);
            
            TryCatch try_catch;

            Handle<String> source(String::New(code.c_str()));
            Handle<Script> script(Script::Compile(source));

            if(script.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw unrecoverable_error_t(*exception);
            }

            Handle<Value> result(script->Run());

            if(result.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw unrecoverable_error_t(*exception);
            }

            Handle<String> target(String::New(name.c_str()));
            Handle<Value> object(m_v8_context->Global()->Get(target));

            if(!object->IsFunction()) {
                throw configuration_error_t("target object is not a function");
            }

            Handle<Function> function(Handle<Function>::Cast(object));
            m_function = Persistent<Function>::New(function);
        }

    private:
        Persistent<Context> m_v8_context;
        Persistent<Function> m_function;
};

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<javascript_t, plugin_t>("javascript");
    }
}

}}
