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

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include <boost/thread/mutex.hpp>
#include <deque>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/interfaces/storage.hpp"

namespace cocaine {

struct defaults {
    // Default engine policy.
    static const float heartbeat_timeout;
    static const float suicide_timeout;
    static const unsigned int pool_limit;
    static const unsigned int queue_limit;

    // I/O bulk size.
    static const unsigned int io_bulk_size;

    // Default slave binary.
    static const char slave[];

    // Default paths.
    static const char plugin_path[];
    static const char ipc_path[];
};

struct config_t {
    config_t(const std::string& config_path);

    std::string config_path,
                plugin_path,
                ipc_path;

    typedef struct {
        std::string type;
        std::string uri;
    } storage_info_t;

    typedef std::map<
        std::string,
        storage_info_t
    > storage_info_map_t;

    storage_info_map_t storages;

    typedef struct {
        std::string hostname;
    } runtime_info_t;

    runtime_info_t runtime;
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config,
                  boost::shared_ptr<logging::sink_t> sink);

        ~context_t();

        // Repository API
        // --------------

        template<class Category>
        typename category_traits<Category>::ptr_type
        get(const std::string& type,
            const typename category_traits<Category>::args_type& args)
        {
            return m_repository->get<Category>(type, args);
        }

        // Networking
        // ----------

        zmq::context_t& io();
        
        // Storage
        // -------

        template<class T>
        typename category_traits<
            storages::storage_concept<T>
        >::ptr_type
        storage(const std::string& name) {
            typedef storages::storage_concept<T> storage_type;

            config_t::storage_info_map_t::const_iterator it(
                config.storages.find(name)
            );
    
            if(it == config.storages.end()) {
                throw configuration_error_t("the specified storage doesn't exist");
            }

            return get<storage_type>(
                it->second.type,
                typename category_traits<storage_type>::args_type(
                    it->second.uri
                )
            );
        }

        // Logging
        // -------

        boost::shared_ptr<logging::logger_t>
        log(const std::string& name);

    public:
        const config_t config;

    private:
        // Initialization interlocking.
        boost::mutex m_mutex;

        // Logging.    
        boost::shared_ptr<logging::sink_t> m_sink;
        
        // Core subsystems.
        std::auto_ptr<zmq::context_t> m_io;
        std::auto_ptr<repository_t> m_repository;
};

}

#endif
