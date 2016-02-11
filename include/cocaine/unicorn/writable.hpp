/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef COCAINE_UNICORN_WRITABLE_ADAPTER_HPP
#define COCAINE_UNICORN_WRITABLE_ADAPTER_HPP

namespace cocaine {

template<class T>
struct deferred;
template<class T>
struct streamed;

} //namespace cocaine

namespace cocaine { namespace unicorn {

template<class T>
class writable_adapter_base_t {
public:
    virtual
    ~writable_adapter_base_t() {}

    virtual void
    write(T&& t) = 0;

    virtual void
    abort(const std::error_code& code, const std::string&) = 0;

    void
    abort(const std::error_code& code) {
        abort(code, "");
    }
};

template<class W, class T>
class writable_adapter_t :
public writable_adapter_base_t<T>
{
public:
    writable_adapter_t(W _w) :
    w(std::move(_w)) {
    }

    virtual void
    write(T&& t) {
        w.write(std::move(t));
    }

    virtual void
    abort(const std::error_code& code, const std::string& reason) {
        std::string full_reason = code.message();
        if(!reason.empty()) {
            full_reason += " - ";
            full_reason += reason;
        }
        w.abort(code, full_reason);
    }

private:
    W w;
};

template<class T>
struct writable_helper {
    typedef std::shared_ptr <writable_adapter_base_t<T>> ptr;
    typedef writable_adapter_t<deferred <T>, T> deferred_adapter;
    typedef writable_adapter_t<streamed <T>, T> streamed_adapter;
};

template<class F, template<typename> class T>
std::shared_ptr<writable_adapter_base_t<F>>
make_writable(const T<F>& t) {
    return std::make_shared<writable_adapter_t<T<F>, F>>(t);
}

}}

#endif
