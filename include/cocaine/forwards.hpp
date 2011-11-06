#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    namespace engine {
        enum error_code {
            request_error     = 400,
            server_error      = 500,
            application_error = 502,
            resource_error    = 503,
            timeout_error     = 504
        };
        
        class backend_t;
        class driver_t;
        class engine_t;
        class job_t;
        class overseer_t;
    }

    namespace plugin {
        class source_t;
    }
}

#endif
