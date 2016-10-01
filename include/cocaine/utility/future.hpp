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

#pragma once

#include <future>
#include <string>
#include <system_error>

namespace cocaine {

template<class T>
std::future<typename std::decay<T>::type> make_ready_future(T&& value) {
    std::promise<typename std::decay<T>::type> promise;
    promise.set_value(std::forward<T>(value));
    return promise.get_future();
}

inline
std::future<void> make_ready_future() {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();

}

template<class T>
std::future<T> make_exceptional_future() {
    std::promise<T> promise;
    promise.set_exception(std::current_exception());
    return promise.get_future();
}

template<class T, class E>
std::future<T> make_exceptional_future(E&& e) {
    std::promise<T> promise;
    auto e_ptr = std::make_exception_ptr(e);
    promise.set_exception(e_ptr);
    return promise.get_future();
}

template<class T>
std::future<T> make_exceptional_future(std::error_code ec) {
    return make_exceptional_future<T>(std::system_error(std::move(ec)));
}

template<class T>
std::future<T> make_exceptional_future(std::error_code ec, std::string msg) {
    return make_exceptional_future<T>(std::system_error(std::move(ec), std::move(msg)));
}

}
