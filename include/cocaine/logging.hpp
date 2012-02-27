//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_LOGGING_HPP
#define COCAINE_LOGGING_HPP

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"

namespace cocaine { namespace logging {

enum logging_level {
	debug,
	info,
	warning,
	error
};

class sink_t {
	public:
		virtual void emit(logging_level level, const char* format, ...) = 0;
};

class emitter_t {
    public:
        emitter_t(context_t& context, const std::string& source):
        	m_sink(context.sink()),
        	m_source(source + ": ")
        { }

        void debug(const char* format, ...) { }
        void info(const char* format, ...) { }
        void warning(const char* format, ...) { }
        void error(const char* format, ...) { }

    private:
    	void emit(logging_level level, const char* format, ...) { }

    private:
    	sink_t& m_sink;
    	const std::string m_source;
};

}}

#endif
