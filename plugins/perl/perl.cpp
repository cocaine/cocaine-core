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
#include "cocaine/registry.hpp"

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void xs_init(pTHX) {
	char* file = (char*)__FILE__;
	dXSUB_SYS;

	// DynaLoader is a special case
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

namespace cocaine { namespace engine {

class perl_t:
    public plugin_t,
    public core::module_t<perl_t>
{
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
            
        virtual void invoke(io_t& io, const std::string& method)
        {
            std::string input;
            
            if (io.request && io.request_size > 0) {
               input = std::string((const char*)io.request, io.request_size);
            }

            std::string result;
            const char* input_value_buff = NULL;

            // init stack pointer
            dSP;
            ENTER;
            SAVETMPS;

            // remember stack pointer
            PUSHMARK(SP);

            int call_flags = G_EVAL | G_SCALAR | G_NOARGS;

            // if there's an argument - push it on the stack
            if (!input.empty()) {
            	call_flags = G_EVAL | G_SCALAR;
                input_value_buff = input.c_str();

                XPUSHs(sv_2mortal(newSVpv(input_value_buff, 0)));
                PUTBACK;
            }

            // call function
            int ret_vals_count = call_pv(method.c_str(), call_flags);

            // refresh stack pointer
			SPAGAIN;

			// get error (if any)
			if (SvTRUE(ERRSV)) {
				STRLEN n_a;

				std::string error_msg = "perl eval error: ";
				error_msg += SvPV(ERRSV, n_a);
				throw unrecoverable_error_t(error_msg);
			}

			// pop returned value of the stack
			if (ret_vals_count > 0) {
				char* str_ptr = savepv(POPp);

				if (str_ptr) {
					result = std::string(str_ptr);
				}
			}

			// clean-up
			PUTBACK;
			FREETMPS;
			LEAVE;

			// invoke callback with resulting data
            if (!result.empty()) {
                io.push(result.data(), result.size());
            }
        }

    private:
        void compile(const std::string& code)
        {
            const char* embedding[] = {"", "-e", "0"};
            perl_parse(my_perl, xs_init, 3, (char**)embedding, NULL);
            PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
            perl_run(my_perl);
            eval_pv(code.c_str(), TRUE);
        }
    
    private:
        PerlInterpreter* my_perl;
};

extern "C" {
    void initialize(core::registry_t& registry) {
        PERL_SYS_INIT3(NULL, NULL, NULL);
        registry.install("perl", &perl_t::create);
    }

    __attribute__((destructor)) void finalize() {
        PERL_SYS_TERM();
    }
}

}}
