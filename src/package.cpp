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

#include <archive.h>
#include <archive_entry.h>

#include "cocaine/package.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine::engine;

package_error_t::package_error_t(archive * source):
    std::runtime_error(archive_error_string(source))
{ }

package_t::package_t(context_t& context, const blob_t& archive):
    m_log(context.log("packaging")),
    m_archive(archive_read_new())
{
    int rv = ARCHIVE_OK;

    archive_read_support_format_all(m_archive);
    archive_read_support_compression_all(m_archive);
    
    rv = archive_read_open_memory(
        m_archive, 
        const_cast<void*>(archive.data()),
        archive.size()
    );

    if(rv) {
        throw package_error_t(m_archive);
    }
}

package_t::~package_t() {
    archive_read_close(m_archive);
    archive_read_finish(m_archive);
}

void package_t::deploy(const boost::filesystem::path& prefix) {
    archive * target = archive_write_disk_new();
    archive_entry * entry = NULL;

    int rv = ARCHIVE_OK;

    int flags = ARCHIVE_EXTRACT_TIME |
                ARCHIVE_EXTRACT_PERM |
                ARCHIVE_EXTRACT_ACL |
                ARCHIVE_EXTRACT_FFLAGS |
                ARCHIVE_EXTRACT_SECURE_NODOTDOT;

    archive_write_disk_set_options(target, flags);
    archive_write_disk_set_standard_lookup(target);
    
    while(true) {
        rv = archive_read_next_header(m_archive, &entry);
        
        if(rv == ARCHIVE_EOF) {
            break;
        } else if(rv != ARCHIVE_OK) {
            throw package_error_t(m_archive);
        }

        boost::filesystem::path path = archive_entry_pathname(entry);

        // Prepend the path.
        archive_entry_set_pathname(entry, (prefix / path).string().c_str());

        rv = archive_write_header(target, entry);
        
        if(rv != ARCHIVE_OK) {
            throw package_error_t(target);
        } else if(archive_entry_size(entry) > 0) {
            extract(m_archive, target);
        }
    }

    rv = archive_write_finish_entry(target);

    if(rv != ARCHIVE_OK) {
        throw package_error_t(target);
    }

    m_log->info(
        "app archive type: %s, extracted %d files to '%s'",
        archive_compression_name(m_archive),
        archive_file_count(m_archive),
        prefix.string().c_str()
    );

    archive_write_close(target);
    archive_write_finish(target);
}

void package_t::extract(archive * source, 
                        archive * target)
{
    int rv = ARCHIVE_OK;

    const void * buffer = NULL;
    size_t size = 0;
    off_t offset = 0;

    while(true) {
        rv = archive_read_data_block(source, &buffer, &size, &offset);
        
        if(rv == ARCHIVE_EOF) {
            return;
        } else if(rv != ARCHIVE_OK) {
            throw package_error_t(source);
        }

        rv = archive_write_data_block(target, buffer, size, offset);
        
        if(rv != ARCHIVE_OK) {
            throw package_error_t(target);
        }
    }
}
