/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#include "cocaine/dealer/storage/eblob.hpp"

namespace cocaine {
namespace dealer {

eblob::eblob() {
}

eblob::eblob(const std::string& path,
			 const boost::shared_ptr<context_t>& ctx,
			 bool logging_enabled,
			 uint64_t blob_size,
			 int sync_interval,
			 int defrag_timeout) :
	dealer_object_t(ctx, logging_enabled)
{
	create_eblob(path, blob_size, sync_interval, defrag_timeout);
}

eblob::~eblob() {
	log("eblob at path: %s destroyed.", m_path.c_str());
}

void
eblob::write(const std::string& key, const std::string& value, int column) {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	if (column < 0) {
		std::string error_msg = "bad column index at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	// 2DO: truncate written value
	m_storage->write_hashed(key, value, 0, BLOB_DISK_CTL_OVERWRITE, column);
}

void
eblob::write(const std::string& key, void* data, size_t size, int column) {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	if (column < 0) {
		std::string error_msg = "bad column index at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	// 2DO: truncate written value
	std::string value(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data) + size);
	m_storage->write_hashed(key, value, 0, BLOB_DISK_CTL_OVERWRITE, column);
}

std::string
eblob::read(const std::string& key, int column) {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	return m_storage->read_hashed(key, 0, 0, column);
}

void
eblob::remove_all(const std::string &key) {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key;
		throw internal_error(error_msg);
	}

	eblob_key ekey;
	m_storage->key(key, ekey);
	m_storage->remove_all(ekey);
}

void
eblob::remove(const std::string& key, int column) {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
		throw internal_error(error_msg);
	}

	m_storage->remove_hashed(key, column);
}

unsigned long long
eblob::items_count() {
	if (!m_storage.get()) {
		std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}

	return m_storage->elements();
}

void
eblob::iterate(iteration_callback_t iteration_callback, int start_column, int end_column) {
	if (!iteration_callback) {
		return;
	}

	iteration_callback_m = iteration_callback;

	eblob_iterate_control ctl;
    eblob_iterate_callbacks	iterator_cb;

    iterator_cb.iterator = eblob::iteration_callback;
    iterator_cb.iterator_init = NULL;
    iterator_cb.iterator_free = NULL;

    memset(&ctl, 0, sizeof(ctl));

    ctl.check_index = 1;
    ctl.flags = EBLOB_ITERATE_FLAGS_ALL;
    ctl.priv = this;
    ctl.iterator_cb = iterator_cb;
    ctl.start_type = start_column;
    ctl.max_type = end_column;
    ctl.thread_num = 1;
    m_storage->iterate(ctl);
}

void
eblob::create_eblob(const std::string& path,
					uint64_t blob_size,
					int sync_interval,
					int defrag_timeout)
{
	m_path = path;

	// create eblob logger
	eblob_logger_m.reset(new ioremap::eblob::eblob_logger("/dev/stdout", 0));

	// create config
    eblob_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.file = const_cast<char*>(m_path.c_str());
    cfg.log = eblob_logger_m->log();
    cfg.sync = sync_interval;
    cfg.blob_size = blob_size;
    cfg.defrag_timeout = defrag_timeout;
    cfg.iterate_threads = 1;

    // create eblob
    m_storage.reset(new ioremap::eblob::eblob(&cfg));

	log("eblob at path: %s created.", m_path.c_str());
}

int
eblob::iteration_callback(__attribute__ ((unused)) eblob_disk_control* dc,
						  eblob_ram_control* rc,
						  void* data,
						  void* priv,
						  __attribute__ ((unused)) void* thread_priv)
{
	eblob* eb = reinterpret_cast<eblob*>(priv);
	eb->iteration_callback_instance(data, rc->size, rc->type);

	return 0;
}

void
eblob::iteration_callback_instance(void* data, uint64_t size, int column) {
	if (iteration_callback_m) {
		iteration_callback_m(data, size, column);
	}
}

} // namespace dealer
} // namespace cocaine
