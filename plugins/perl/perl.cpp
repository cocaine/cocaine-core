//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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

#include <sstream>
#include <boost/filesystem/fstream.hpp>

#include <EXTERN.h>
#include <perl.h>

#include "cocaine/interfaces/plugin.hpp"

namespace cocaine { namespace engine {

class perl_t:
    public plugin_t
{
    public:
        static object_t* create(context_t& ctx) {
            return new perl_t(ctx);
        }

    public:
        perl_t(context_t& ctx):
            plugin_t(ctx, "perl")
        { }

        ~perl_t() 
        {
            perl_destruct(my_perl);
            perl_free(my_perl);
        }

        virtual void initialize(const app_t& app)
        {
            Json::Value args(app.manifest["args"]);

            if(!args.isObject()) {
                throw unrecoverable_error_t("malformed manifest");
            }

            boost::filesystem::path source(args["source"].asString());

            if(source.empty()) {
                throw unrecoverable_error_t("no code location has been specified");
            }

            boost::filesystem::ifstream input(source);
    
            if(!input) {
                throw unrecoverable_error_t("unable to open " + source.string());
            }

            std::stringstream stream;
            stream << input.rdbuf();
            
            my_perl = perl_alloc();
            perl_construct(my_perl);

            compile(stream.str());
        }
            
        virtual void invoke(invocation_site_t& site, const std::string& method)
        {
            std::string input;
            
            if (site.request && site.request_size > 0) {
               input = std::string((const char*)site.request, site.request_size);
            }

            std::string result;
            const char* input_value_buff = NULL;

            if (!input.empty()) {
                input_value_buff = input.c_str();

                dSP;
                ENTER;
                SAVETMPS;
				
                PUSHMARK(SP);
                XPUSHs(sv_2mortal(newSVpv(input_value_buff, 0)));
                PUTBACK;
                
				int ret_vals_count = call_pv(method.c_str(), G_SCALAR);
                SPAGAIN;

                if (ret_vals_count > 0) {
                    char* str_ptr = savepv(POPp);

                    if (str_ptr) {
                        result = std::string(str_ptr);
                    }
                }

                FREETMPS;
                LEAVE;
            }
            else {
                PERL_SET_CONTEXT(my_perl);
                dSP;
                ENTER;
                SAVETMPS;

                PUSHMARK(SP);				
                PUTBACK;

                int ret_vals_count = call_pv(method.c_str(), G_SCALAR | G_NOARGS);
                SPAGAIN;

                if (ret_vals_count > 0) {
                    char* str_ptr = savepv(POPp);

                    if (str_ptr) {
                        result = std::string(str_ptr);
                    }
                }

                FREETMPS;
                LEAVE;
            }

            if (!result.empty()) {
                site.push(result.data(), result.size());
            }
        }

    private:
        void compile(const std::string& code)
        {
            const char* embedding[] = {"", "-e", "0"};
            perl_parse(my_perl, NULL, 3, (char**)embedding, NULL);
            PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
            perl_run(my_perl);

            eval_pv(code.c_str(), TRUE);
        }
    
    private:
        PerlInterpreter* my_perl;
};

static const cocaine::core::module_info_t module_info[] = {
    { "perl", &perl_t::create },
    { NULL, NULL }
};

extern "C" {
    const cocaine::core::module_info_t* initialize() {
        PERL_SYS_INIT3(NULL, NULL, NULL);
        return module_info;
    }

    __attribute__((destructor)) void finalize() {
        PERL_SYS_TERM();
    }
}

}}
