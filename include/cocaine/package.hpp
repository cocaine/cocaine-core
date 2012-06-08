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

#include <archive.h>
#include <archive_entry.h>
#include <boost/filesystem/path.hpp>

namespace cocaine { namespace engine {

struct archive_error_t:
    public std::runtime_error
{
    archive_error_t(archive * source):
        std::runtime_error(archive_error_string(source))
    { }
};

static void emplace(archive * source, 
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
            throw archive_error_t(source);
        }

        rv = archive_write_data_block(target, buffer, size, offset);
        
        if(rv != ARCHIVE_OK) {
            throw archive_error_t(target);
        }
    }
}

static void extract(const void * data,
                    size_t size,
                    const boost::filesystem::path& prefix)
{
    int rv = ARCHIVE_OK;

    archive * source = archive_read_new(),
            * target = archive_write_disk_new();
    
    int flags = ARCHIVE_EXTRACT_TIME |
                ARCHIVE_EXTRACT_PERM |
                ARCHIVE_EXTRACT_ACL |
                ARCHIVE_EXTRACT_FFLAGS |
                ARCHIVE_EXTRACT_SECURE_NODOTDOT;

    archive_entry * entry;

    archive_read_support_format_all(source);
    archive_read_support_compression_all(source);
    
    archive_write_disk_set_options(target, flags);
    archive_write_disk_set_standard_lookup(target);

    rv = archive_read_open_memory(
        source, 
        const_cast<void*>(data),
        size
    );

    if(rv) {
        throw archive_error_t(source);
    }

    while(true) {
        rv = archive_read_next_header(source, &entry);
        
        if(rv == ARCHIVE_EOF) {
            break;
        } else if(rv != ARCHIVE_OK) {
            throw archive_error_t(source);
        }

        boost::filesystem::path path = archive_entry_pathname(entry);

        // Prepend the path.
        archive_entry_set_pathname(entry, (prefix / path).string().c_str());

        rv = archive_write_header(target, entry);
        
        if(rv != ARCHIVE_OK) {
            throw archive_error_t(target);
        } else if(archive_entry_size(entry) > 0) {
            emplace(source, target);
        }
    }

    rv = archive_write_finish_entry(target);

    if(rv != ARCHIVE_OK) {
        throw archive_error_t(target);
    }

    archive_read_close(source);
    archive_read_finish(source);

    archive_write_close(target);
    archive_write_finish(target);
}

}}

#endif
