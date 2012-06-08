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

#ifndef COCAINE_PACKAGE_HPP
#define COCAINE_PACKAGE_HPP

#include <boost/filesystem/path.hpp>

#include "cocaine/common.hpp"

#include "cocaine/helpers/blob.hpp"

struct archive;

namespace cocaine {

struct package_error_t:
    public std::runtime_error
{
    package_error_t(archive * source);
};

class package_t {
    public:
        package_t(context_t& context,
                  const blob_t& archive);
        
        ~package_t();

        void deploy(const boost::filesystem::path& prefix);
        
    public:
        std::string type() const;

    private:
        static void extract(archive * source, 
                            archive * target);

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        archive * m_archive;
};

}

#endif
