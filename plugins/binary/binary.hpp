/*
 * Copyright (C) 2012+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the BSD 2-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifndef COCAINE_BINARY_SANDBOX_HPP
#define COCAINE_BINARY_SANDBOX_HPP

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/manifest.hpp>

#include <cocaine/interfaces/sandbox.hpp>

#include <cocaine/helpers/json.hpp>
#include <cocaine/helpers/track.hpp>

namespace cocaine { namespace engine {

typedef void *(* init_fn_t)(const char *cfg, const size_t size);
typedef void (* cleanup_fn_t)(void *);
typedef int (* process_fn_t)(void *, const char *event, const size_t esize, const char *data, const size_t dsize);

class binary_t: public sandbox_t {
	public:
		typedef sandbox_t category_type;

		binary_t(context_t& context, const manifest_t& manifest);
		virtual ~binary_t();

		virtual void invoke(const std::string& method, io_t& io);

		const logging::logger_t& log() const {
			return *m_log;
		}

	private:
        	boost::shared_ptr<logging::logger_t> m_log;
		lt_dladvise m_advice;
		process_fn_t m_process;
		cleanup_fn_t m_cleanup;
		lt_dlhandle m_bin;
		void *m_handle;
};

}}

#endif /* COCAINE_BINARY_SANDBOX_HPP */
