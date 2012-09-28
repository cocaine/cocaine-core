/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <archive.h>
#include <archive_entry.h>

#include "cocaine/archive.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

archive_error_t::archive_error_t(archive * source):
    std::runtime_error(archive_error_string(source))
{ }

archive_t::archive_t(context_t& context, const std::string& archive):
    m_log(context.log("packaging")),
    m_archive(archive_read_new())
{
    int rv = ARCHIVE_OK;

    archive_read_support_format_all(m_archive);
    archive_read_support_compression_all(m_archive);
    
    rv = archive_read_open_memory(
        m_archive, 
        const_cast<char*>(archive.data()),
        archive.size()
    );

    if(rv) {
        throw archive_error_t(m_archive);
    }
}

archive_t::~archive_t() {
    archive_read_close(m_archive);
    archive_read_finish(m_archive);
}

void
archive_t::deploy(const boost::filesystem::path& prefix) {
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
            throw archive_error_t(m_archive);
        }

        boost::filesystem::path path = archive_entry_pathname(entry);

        // Prepend the path.
        archive_entry_set_pathname(entry, (prefix / path).string().c_str());

        rv = archive_write_header(target, entry);
        
        if(rv != ARCHIVE_OK) {
            throw archive_error_t(target);
        } else if(archive_entry_size(entry) > 0) {
            extract(m_archive, target);
        }
    }

    rv = archive_write_finish_entry(target);

    if(rv != ARCHIVE_OK) {
        throw archive_error_t(target);
    }

    // NOTE: The reported count is off by one for some reason.
    size_t count = archive_file_count(m_archive) - 1;

    m_log->info(
        "archive type: %s, extracted %d %s to '%s'",
        type().c_str(),
        count,
        count == 1 ? "file" : "files",
        prefix.string().c_str()
    );

    archive_write_close(target);
    archive_write_finish(target);
}

void
archive_t::extract(archive * source, 
                   archive * target)
{
    int rv = ARCHIVE_OK;

    const void * buffer = NULL;
    size_t size = 0;
    
#if ARCHIVE_VERSION_NUMBER < 3000000
    off_t offset = 0;
#else
    int64_t offset = 0;
#endif

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

std::string
archive_t::type() const {
    return archive_compression_name(m_archive);
}
