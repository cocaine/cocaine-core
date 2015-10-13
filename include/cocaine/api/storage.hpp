/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_STORAGE_API_HPP
#define COCAINE_STORAGE_API_HPP

#include "cocaine/common.hpp"

#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/traits.hpp"

#include <mutex>
#include <sstream>

namespace cocaine { namespace api {

struct storage_t {
    typedef storage_t category_type;

    virtual
   ~storage_t() {
        // Empty.
    }

    virtual
    std::string
    read(const std::string& collection, const std::string& key) = 0;

    virtual
    void
    write(const std::string& collection, const std::string& key, const std::string& blob,
          const std::vector<std::string>& tags) = 0;

    virtual
    void
    remove(const std::string& collection, const std::string& key) = 0;

    virtual
    std::vector<std::string>
    find(const std::string& collection, const std::vector<std::string>& tags) = 0;

    // Helper methods

    template<class T>
    T
    get(const std::string& collection, const std::string& key);

    template<class T>
    void
    put(const std::string& collection, const std::string& key, const T& object,
        const std::vector<std::string>& tags);

protected:
    storage_t(context_t&, const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
};

template<class T>
T
storage_t::get(const std::string& collection, const std::string& key) {
    T result;
    msgpack::unpacked unpacked;

    std::string blob(read(collection, key));

    try {
        msgpack::unpack(&unpacked, blob.data(), blob.size());
    } catch(const msgpack::unpack_error& e) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
    }

    try {
        io::type_traits<T>::unpack(unpacked.get(), result);
    } catch(const msgpack::type_error& e) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
    }

    return result;
}

template<class T>
void
storage_t::put(const std::string& collection, const std::string& key, const T& object,
               const std::vector<std::string>& tags)
{
    std::ostringstream buffer;
    msgpack::packer<std::ostringstream> packer(buffer);

    io::type_traits<T>::pack(packer, object);

    write(collection, key, buffer.str(), tags);
}

template<>
struct category_traits<storage_t> {
    typedef std::shared_ptr<storage_t> ptr_type;

    struct factory_type: public basic_factory<storage_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<storage_t>>& instances) {
                if((instance = instances[name].lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, args);
                    instances[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<storage_t>>> instances;
    };
};

category_traits<storage_t>::ptr_type
storage(context_t& context, const std::string& name);

}} // namespace cocaine::api

#endif
