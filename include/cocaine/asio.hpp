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

#ifndef COCAINE_ASIO_HPP
#define COCAINE_ASIO_HPP

#include "cocaine/messaging.hpp"

#include <array>
#include <deque>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#define EV_MINIMAL       0
#define EV_USE_MONOTONIC 1
#define EV_USE_REALTIME  1
#define EV_USE_NANOSLEEP 1
#define EV_USE_EVENTFD   1

#include <ev++.h>

namespace cocaine { namespace io {

template<class T>
struct write_queue {
    write_queue(ev::loop_ref& loop,
                const boost::shared_ptr<T>& pipe):
        m_pipe(pipe),
        m_pipe_watcher(loop)
    {
        m_pipe_watcher.set<write_queue, &write_queue::on_event>(this);
    }

    ~write_queue() {
        BOOST_ASSERT(m_queue.empty());
    }

    void
    write(const std::string& data) {
        if(m_queue.empty()) {
            ssize_t length = m_pipe->write(data);

            if(length != data.size()) {
                // Enqueue the chunk.
                m_queue.emplace_back(data, length);

                // Enable the poller.
                m_pipe_watcher.start(m_pipe->fd(), ev::WRITE);
            }
        } else {
            // Queue up the the chunk.
            m_queue.emplace_back(data, 0);
        } 
    }

    void
    write(const char * data,
          size_t size)
    {
        write(std::string(data, size));
    }

private:
    void
    on_event(ev::io& io, int revents) {
        if(m_queue.empty()) {
            m_pipe_watcher.stop();
            return;
        }

        // Get the top of the write queue.  
        auto& chunk = m_queue.front();

        // Calculate the chunk to be sent.
        const char * ptr = chunk.first.data() + chunk.second;
        size_t remaining = chunk.first.size() - chunk.second;

        // Try to send the data.
        ssize_t length = m_pipe->write(ptr, remaining);

        if(length == remaining) {
            // Chunk is sent, pop it.
            m_queue.pop_front();
        } else {
            // Recalculate the next offset.
            chunk.second += length;
        }
    }

private:
    boost::shared_ptr<T> m_pipe;
    ev::io m_pipe_watcher;

    std::deque<
        std::pair<std::string, off_t>
    > m_queue;
};

template<class T, size_t ChunkSize = 4096>
struct read_queue {
    template<class F>
    read_queue(ev::loop_ref& loop,
               const boost::shared_ptr<T>& pipe,
               F callback):
        m_pipe(pipe),
        m_pipe_watcher(loop),
        m_callback(callback)
    {
        m_pipe_watcher.set<read_queue, &read_queue::on_event>(this);
        m_pipe_watcher.start(m_pipe->fd(), ev::READ);
    }

private:
    void
    on_event(ev::io& io, int revents) {
        std::array<char, ChunkSize> chunk;

        // Try to read some data.
        ssize_t length = m_pipe->read(chunk.data(), chunk.size());

        if(length == 0) {
            return;
        }

        m_callback(chunk.data(), length);
    }

private:
    boost::shared_ptr<T> m_pipe;
    ev::io m_pipe_watcher;

    boost::function<
        void(const char*, size_t)
    > m_callback;
};

template<class T>
struct codex {
    codex(ev::loop_ref& loop,
          const boost::shared_ptr<T>& pipe):
        m_output(new write_queue<T>(loop, pipe)),
        m_input(new read_queue<T>(loop, pipe, boost::bind(&codex::on_message, this, _1, _2))),
        m_unpacker(new msgpack::unpacker())
    { }

    template<class F>
    void
    bind(F callback) {
        m_callback = callback;
    }

    template<class Event, typename... Args>
    void
    send(Args&&... args) {
        msgpack::packer<write_queue<T>> packer(*m_output);

        // NOTE: Format is [ID, [Args...]].
        packer.pack_array(2);

        type_traits<int>::pack(
            packer,
            event_traits<Event>::id
        );
        
        if(!event_traits<Event>::empty) {
            type_traits<typename event_traits<Event>::tuple_type>::pack(
                packer,
                std::forward<Args>(args)...
            );
        } else {
            packer.pack_nil();
        }
    }

    void
    send(const std::string& chunk) {
        m_output->write(chunk);
    }

private:
    void
    on_message(const char * chunk,
               size_t size)
    {
        m_unpacker->reserve_buffer(size);

        ::memcpy(m_unpacker->buffer(), chunk, size);

        m_unpacker->buffer_consumed(size);

        msgpack::unpacked u;

        while(m_unpacker->next(&u)) {
            m_callback(message_t(std::move(u)));
        }
    }

private:
    boost::shared_ptr<write_queue<T>> m_output;
    boost::shared_ptr<read_queue<T>> m_input;

    boost::function<
        void(const message_t&)
    > m_callback;

    boost::shared_ptr<msgpack::unpacker> m_unpacker;
};

}}

#endif
