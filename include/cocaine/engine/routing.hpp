#ifndef COCAINE_SELECTORS_HPP
#define COCAINE_SELECTOPS_HPP

#include "cocaine/common.hpp"
#include "cocaine/engine.hpp"

namespace cocaine { namespace engine { namespace routing {
    struct specific_thread:
        public helpers::unique_id_t
    {
        specific_thread(helpers::unique_id_t::type thread_id):
            unique_id_t(thread_id)
        { }

        engine_t::thread_map_t::iterator operator()(engine_t::thread_map_t& threads) {
            return threads.find(id());
        }
    };
    
    struct shortest_queue:
        public helpers::unique_id_t
    {
        struct predicate {
            bool operator()(engine_t::thread_map_t::value_type left, engine_t::thread_map_t::value_type right) {
                return left->second->queue_size() < right->second->queue_size();
            }
        };
       
        shortest_queue(unsigned int limit_):
            limit(limit_)
        { }

        engine_t::thread_map_t::iterator operator()(engine_t::thread_map_t& threads) {
            engine_t::thread_map_t::iterator thread(std::min_element(
                threads.begin(), threads.end(), predicate()));
            
            if(thread != threads.end() && thread->second->queue_size() < limit) {
                return thread;
            } else {
                return threads.end();
            }
        }

        unsigned int limit;
    };
}}}

#endif
