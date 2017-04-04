#pragma once

#include <map>
#include <unordered_map>

#include "cocaine/format/base.hpp"
#include "cocaine/format/kv_container.hpp"

namespace cocaine {

template<typename K, typename V>
struct display<std::map<K, V>> : public kv_container_display<std::map<K, V>> {};

template<typename K, typename V>
struct display<std::unordered_map<K, V>> : public kv_container_display<std::unordered_map<K, V>> {};

template<typename K, typename V>
struct display<std::multimap<K, V>> : public kv_container_display<std::multimap<K, V>> {};

template<typename K, typename V>
struct display<std::unordered_multimap<K, V>> : public kv_container_display<std::unordered_multimap<K, V>> {};

template<typename K, typename V>
struct display_traits<std::map<K, V>> : public lazy_display<std::map<K, V>> {};

template<typename K, typename V>
struct display_traits<std::unordered_map<K, V>> : public lazy_display<std::unordered_map<K, V>> {};

template<typename K, typename V>
struct display_traits<std::multimap<K, V>> : public lazy_display<std::multimap<K, V>> {};

template<typename K, typename V>
struct display_traits<std::unordered_multimap<K, V>> : public lazy_display<std::unordered_multimap<K, V>> {};

} // namespace cocaine
