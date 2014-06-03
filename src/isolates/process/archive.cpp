/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/isolates/archive.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <archive.h>
#include <archive_entry.h>

using namespace cocaine;

namespace fs = boost::filesystem;

archive_error_t::archive_error_t(archive* source):
    std::runtime_error(archive_error_string(source))
{ }

archive_t::archive_t(context_t& context, const std::string& archive):
    m_log(new logging::log_t(context, "packaging")),
    m_archive(archive_read_new())
{
#if ARCHIVE_VERSION_NUMBER < 3000000
    archive_read_support_compression_all(m_archive);
#else
    archive_read_support_filter_all(m_archive);
#endif

    archive_read_support_format_all(m_archive);

    const int rv = archive_read_open_memory(
        m_archive,
        const_cast<char*>(archive.data()),
        archive.size()
    );

    if(rv != ARCHIVE_OK) {
        throw archive_error_t(m_archive);
    }

    COCAINE_LOG_INFO(m_log, "compression: %s, size: %llu bytes", type(), archive.size());
}

archive_t::~archive_t() {
    archive_read_close(m_archive);

#if ARCHIVE_VERSION_NUMBER < 3000000
    archive_read_finish(m_archive);
#else
    archive_read_free(m_archive);
#endif
}

void
archive_t::deploy(const std::string& prefix_) {
    const fs::path prefix = prefix_;

    for(fs::directory_iterator end_dir_it, it(prefix); it != end_dir_it; ++it) {
        fs::remove_all(it->path());
    }

    archive* target = archive_write_disk_new();
    archive_entry* entry = nullptr;

    int rv = ARCHIVE_OK;

    int flags = ARCHIVE_EXTRACT_TIME |
                ARCHIVE_EXTRACT_SECURE_SYMLINKS |
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

        const fs::path pathname = prefix / archive_entry_pathname(entry);

        // NOTE: Prepend the target path to the stored file path
        // in order to unpack it into the right place.
        archive_entry_set_pathname(entry, pathname.string().c_str());

        if(archive_entry_hardlink(entry)) {
            const fs::path hardlink = prefix / archive_entry_hardlink(entry);

            // NOTE: This entry might be a hardlink to some other file, for example
            // due to tar file deduplication mechanics. We need to update this path as well.
            archive_entry_set_hardlink(entry, hardlink.string().c_str());
        }

        COCAINE_LOG_DEBUG(m_log, "extracting %s", pathname);

        rv = archive_write_header(target, entry);

        if(rv != ARCHIVE_OK) {
            throw archive_error_t(target);
        } else if(archive_entry_size(entry) > 0) {
            extract(m_archive, target);
        }

        rv = archive_write_finish_entry(target);

        if(rv != ARCHIVE_OK) {
            throw archive_error_t(target);
        }
    }

    archive_write_close(target);

#if ARCHIVE_VERSION_NUMBER < 3000000
    archive_write_finish(target);
#else
    archive_write_free(target);
#endif

    const size_t count = archive_file_count(m_archive);

    COCAINE_LOG_INFO(m_log, "extracted %d %s", count, count == 1 ? "file" : "files");
}

void
archive_t::extract(archive* source, archive* target) {
    int rv = ARCHIVE_OK;

    const void* buffer = nullptr;
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
#if ARCHIVE_VERSION_NUMBER < 3000000
    return archive_compression_name(m_archive);
#else
    return archive_filter_name(m_archive, 0);
#endif
}
