/*
 * Copyright (C) 2012+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * This file is part of Cocaine.
 *
 * Cocaine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cocaine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. 
 */

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "binary.hpp"

using namespace cocaine;
using namespace cocaine::engine;
namespace fs = boost::filesystem;

binary_t::binary_t(context_t &context, const manifest_t &manifest) :
    category_type(context, manifest),
    m_log(context.log("app/" + manifest.name)),
    m_process(NULL), m_cleanup(NULL)
{
	Json::Value args(manifest.root["args"]);

	if (!args.isObject()) {
		m_log->error("malformed manifest: no args");
		throw configuration_error_t("malformed manifest");
	}

	boost::filesystem::path source(manifest.path);

	if (lt_dlinit() != 0)
		throw repository_error_t("unable to initialize binary loader");

	if (!boost::filesystem::is_directory(source))
		throw repository_error_t("binary loaded object must be unpacked into directory");

	Json::Value name(args["name"]);
	if (!name.isString())
		throw configuration_error_t("malformed manifest: args/name must be a string");

	source /= name.asString();

	fs::path path(source);

	lt_dladvise_init(&m_advice);
	lt_dladvise_global(&m_advice);

	m_bin = lt_dlopenadvise(source.string().c_str(), m_advice);
	if (!m_bin) {
		m_log->error("unable to load binary object %s: %s", source.string().c_str(), lt_dlerror());
		lt_dladvise_destroy(&m_advice);
		throw repository_error_t("unable to load binary object");
	}

	init_fn_t init = NULL;
	init = reinterpret_cast<init_fn_t>(lt_dlsym(m_bin, "initialize"));
	m_process = reinterpret_cast<process_fn_t>(lt_dlsym(m_bin, "process"));
	m_cleanup = reinterpret_cast<cleanup_fn_t>(lt_dlsym(m_bin, "cleanup"));

	if (!m_process || !m_cleanup || !init) {
		m_log->error("invalid binary loaded: init: %p, process: %p, cleanup: %p",
				init, m_process, m_cleanup);
		lt_dladvise_destroy(&m_advice);
		throw repository_error_t("invalid binary loaded: not all callbacks are present");
	}

	Json::Value config(args["config"]);
	std::string cfg = config.toStyledString();

	m_handle = (*init)(cfg.c_str(), cfg.size() + 1);
	if (!m_handle) {
		m_log->error("binary initialization failed");
		lt_dladvise_destroy(&m_advice);
		throw repository_error_t("binary initialization failed");
	}

	m_log->info("successfully initialized binary module from %s", source.string().c_str());
}

binary_t::~binary_t()
{
	m_cleanup(m_handle);
	lt_dladvise_destroy(&m_advice);
}

namespace {
	void binary_write(struct binary_io *__io, const void *data, size_t size)
	{
		io_t *io = (io_t *)__io->priv_io;
		io->write(data, size);
	}
}

void binary_t::invoke(const std::string &method, io_t &io)
{
	struct binary_io bio;
	int err;

	while (true) {
		blob_t data = io.read(0);

		if (!data.size())
			break;

		bio.chunk.data = (const char *)data.data();
		bio.chunk.size = data.size();
		bio.priv_io = (void *)&io;
		bio.write = binary_write;

		err = m_process(m_handle, &bio);
		if (err < 0) {
			m_log->error("process failed: %d", err);
			throw unrecoverable_error_t("processing error: " + boost::lexical_cast<std::string>(err));
		}
	}

	m_log->debug("process: method: %.*s, err: %d", (int)method.size(), method.data(), err);
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<binary_t>("binary");
    }
}
