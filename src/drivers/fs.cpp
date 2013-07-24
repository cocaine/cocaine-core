/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/drivers/fs.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/traits.hpp"

using namespace cocaine;
using namespace cocaine::driver;

fs_t::fs_t(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args):
    category_type(context, reactor, app, name, args),
    m_log(new logging::log_t(context, cocaine::format("app/%s", name))),
    m_app(app),
    m_event(args.get("emit", name).asString()),
    m_path(args.get("path", "").asString()),
    m_watcher(reactor.native())
{
    if(m_path.empty()) {
        throw cocaine::error_t("no path has been specified");
    }

    m_watcher.set<fs_t, &fs_t::on_event>(this);
    m_watcher.start(m_path.c_str());
}

fs_t::~fs_t() {
    m_watcher.stop();
}

Json::Value
fs_t::info() const {
    Json::Value result;

    result["type"] = "filesystem-monitor";
    result["path"] = m_path;

    return result;
}

void
fs_t::on_event(ev::stat& w, int) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer.pack_array(10);

    packer << w.attr.st_mode
           << w.attr.st_ino
           << w.attr.st_dev
           << w.attr.st_nlink
           << w.attr.st_uid
           << w.attr.st_gid
           << w.attr.st_size
           << w.attr.st_atime
           << w.attr.st_mtime
           << w.attr.st_ctime;

    try {
        m_app.enqueue(api::event_t(m_event), std::make_shared<api::null_stream_t>())->write(
            buffer.data(),
            buffer.size()
        );
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to enqueue an event - %s", e.what());
    }
}

