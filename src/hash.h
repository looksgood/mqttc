/* hash.h hash table interface
** 
** Copyright (C) 2005-2009 Collecta, Inc. 
**
**  This software is provided AS-IS with no warranty, either express
**  or implied.
**
**  This software is distributed under license and may not be copied,
**  modified or distributed except as expressly authorized under the
**  terms of the license contained in the file LICENSE.txt in this
**  distribution.
*/

/** @file
 *  Hash table API.
 */

#ifndef __HASH_H
#define __HASH_H

typedef struct _Hash Hash;

typedef void (*hash_free_func)(void *p);

/** allocate and initialize a new hash table */
Hash *hash_new(const int size, hash_free_func free_fun);

/** allocate a new reference to an existing hash table */
Hash *hash_clone(Hash * const table);

/** release a hash table when no longer needed */
void hash_release(Hash * const table);

/** add a key, value pair to a hash table.
 *  each key can appear only once; the value of any
 *  identical key will be replaced
 */
int hash_add(Hash *table, const char * const key, void *data);

/** look up a key in a hash table */
void *hash_get(Hash *table, const char *key);

/** delete a key from a hash table */
int hash_drop(Hash *table, const char *key);

/** return the number of keys in a hash */
int hash_num_keys(Hash *table);

/** hash key iterator functions */
typedef struct _hash_iterator_t hash_iterator_t;

/** allocate and initialize a new iterator */
hash_iterator_t *hash_iter_new(Hash *table);

/** release an iterator that is no longer needed */
void hash_iter_release(hash_iterator_t *iter);

/** return the next hash table key from the iterator.
    the returned key should not be freed */
const char * hash_iter_next(hash_iterator_t *iter);

#endif /* _HASH_H__ */
