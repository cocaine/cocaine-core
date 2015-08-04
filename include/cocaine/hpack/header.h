/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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


#ifndef COCAINE_HPACK_HEADER_H
#define COCAINE_HPACK_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Here and further "ch" is "cocaine hpack"
 * ch_table is an opaque pointer to header table
 */
struct ch_table;

/**
 * Header data.
 * Used to store header name and value.
 */
struct ch_header_data {
    const char* blob;
    size_t size;
};

/**
 * Non-owning header entity.
 * Contains name and value which points to some other resources.
 */
struct ch_header {
    ch_header_data name;
    ch_header_data value;
};

/**
 * Creates header table.
 * Should be destroyed after usage via ch_table_destroy
 */
ch_table*
ch_table_init();

/**
 * Destroys header table.
 */
void
ch_table_destroy(ch_table* table);

/**
 * Get a header from header table by index.
 * idx should be valid index in table.
 * Otherwise behaviour is undefined
 */
ch_header
ch_table_get_header(ch_table* table, size_t idx);

/**
 * Push a header inside a header table.
 * As far as table is implemented as a some kind of circular buffer
 * this can result in removal of some old headers.
 */
void
ch_table_push(ch_table*, const ch_header*);

/**
 * Try to find a header inside header table by exact match (name AND value).
 * Returns index in a table or 0 if header was not found.
 */
size_t
ch_table_find_by_full_match(ch_table*, const ch_header*);

/**
 * Try to find a header inside header table ONLY by name.
 * Returns index in a table or 0 if header was not found.
 */
size_t
ch_table_find_by_name(ch_table*, const ch_header*);

/**
 * Return data size of a header table.
 */
size_t
ch_table_data_size(ch_table* table);

/**
 * Returns number of headers(including static) in header table.
 */
size_t
ch_table_size(ch_table* table);

/**
 * Max capacity of a header table in bytes.
 */
size_t
ch_table_data_capacity(ch_table* table);

/**
 * Checks if dynamic part of header table is empty.
 */
int
ch_table_empty(ch_table* table);

#ifdef __cplusplus
}
#endif

#endif // COCAINE_HPACK_HEADER_H
