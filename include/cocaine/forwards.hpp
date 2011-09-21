#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    namespace core {
        class future_t;
    }

    namespace engine {
        class engine_t;

        namespace threading {
            class overseer_t;
            class thread_t;
        }

        namespace drivers {
            class abstract_driver_t;
        }
    }

    namespace plugin {
        class plugin_t;
    }

    namespace security {
        class digest_t;
        class signatures_t;
    }

    namespace storage {
        class storage_t;

        namespace backends {
            class abstract_storage_t;
        }
    }
}

#endif
